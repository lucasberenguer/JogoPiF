#include "enemy.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define ENEMY_SPEED 92.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12
#define ENEMY_HURT_RADIUS 20.0f
#define RANGED_MIN_DISTANCE 170.0f
#define RANGED_MAX_DISTANCE 300.0f
#define RANGED_SHOT_SPEED 330.0f

int EnemyOptionalDropChance(EnemyType type) {
    switch (type) {
        case ENEMY_COMMON_MELEE: return 20;
        case ENEMY_COMMON_RANGED: return 25;
        case ENEMY_SPECIAL: return 45;
        case ENEMY_ELITE: return 65;
        case ENEMY_BOSS: return 100;
        default: return 0;
    }
}

bool EnemyShouldDrop(EnemyType type, int roll) {
    return roll >= 0 && roll < 10000 && roll < EnemyOptionalDropChance(type)*100;
}

int EnemyAnimationFrame(const Enemy *enemy) {
    if (enemy->type == ENEMY_BOSS) {
        int count = enemy->state == ENEMY_ATTACK ? 9 : 6;
        int frame = (int)(enemy->stateTime * (enemy->state == ENEMY_ATTACK ? 6.0f : 7.0f));
        return enemy->state == ENEMY_ATTACK ? (frame < count ? frame : count-1) : frame % count;
    }
    int count = enemy->type == ENEMY_COMMON_RANGED ? 4 :
                enemy->state == ENEMY_ATTACK || enemy->state == ENEMY_DIE ? 5 : 4;
    float speed = enemy->state == ENEMY_DIE ? 5.9f :
                  enemy->state == ENEMY_ATTACK ? 8.0f :
                  enemy->state == ENEMY_HURT ? 12.0f : 7.5f;
    float elapsed = enemy->state == ENEMY_WALK ? enemy->walkTime : enemy->stateTime;
    int frame = (int)(elapsed*speed);
    return enemy->state == ENEMY_IDLE || enemy->state == ENEMY_WALK
        ? frame % count : (frame < count ? frame : count-1);
}

bool InitEnemies(EnemyGroup *group) {
    *group = (EnemyGroup){0};
    group->sprites = LoadTexture("assets/inimigo_encapuzado.png");
    group->backSprites = LoadTexture("assets/inimigo_encapuzado_costas.png");
    group->frontSprites = LoadTexture("assets/inimigo_encapuzado_frente.png");
    group->skeleton = LoadTexture("assets/esqueleto.png");
    group->mage = LoadTexture("assets/esqueleto_mago.png");
    group->boss[0] = LoadTexture("assets/coveiro_raizes.png");
    group->boss[1] = LoadTexture("assets/coveiro_pa.png");
    group->boss[2] = LoadTexture("assets/coveiro_investida.png");
    group->effects[0]=LoadTexture("assets/coveiro_efeito_raizes.png");
    group->effects[1]=LoadTexture("assets/coveiro_efeito_pa.png");
    group->effects[2]=LoadTexture("assets/coveiro_efeito_investida.png");
    if (!group->skeleton.id || !group->mage.id || !group->boss[0].id ||
        !group->boss[1].id || !group->boss[2].id || !group->effects[0].id ||
        !group->effects[1].id || !group->effects[2].id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->skeleton, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->mage, TEXTURE_FILTER_POINT);
    for (int i=0;i<3;i++) SetTextureFilter(group->boss[i], TEXTURE_FILTER_POINT);
    for (int i=0;i<3;i++) SetTextureFilter(group->effects[i], TEXTURE_FILTER_POINT);
    group->rangedSprites = LoadTexture("assets/inimigo_ranged.png");
    group->bossSprites = LoadTexture("assets/boss_clareira.png");
    group->bossAttack = LoadTexture("assets/boss_clareira_especial_1.png");
    group->roots = LoadTexture("assets/boss_clareira_raizes_area.png");
    if (!group->sprites.id || !group->backSprites.id || !group->frontSprites.id ||
        !group->rangedSprites.id || !group->bossSprites.id || !group->bossAttack.id || !group->roots.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->backSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->frontSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rangedSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->roots, TEXTURE_FILTER_POINT);
    return true;
}

void UnloadEnemies(EnemyGroup *group) {
    if (group->skeleton.id) UnloadTexture(group->skeleton);
    if (group->mage.id) UnloadTexture(group->mage);
    for (int i=0;i<3;i++) if (group->boss[i].id) UnloadTexture(group->boss[i]);
    for (int i=0;i<3;i++) if (group->effects[i].id) UnloadTexture(group->effects[i]);

    if (group->sprites.id) UnloadTexture(group->sprites);
    if (group->backSprites.id) UnloadTexture(group->backSprites);
    if (group->frontSprites.id) UnloadTexture(group->frontSprites);
    if (group->rangedSprites.id) UnloadTexture(group->rangedSprites);
    if (group->bossSprites.id) UnloadTexture(group->bossSprites);
    if (group->bossAttack.id) UnloadTexture(group->bossAttack);
    if (group->roots.id) UnloadTexture(group->roots);
    *group = (EnemyGroup){0};
}

void ResetEnemies(EnemyGroup *group, const Clearing *clearing) {
    if (clearing->cemetery) { CemeteryResetEnemies(group,clearing); return; }
    group->cemetery=false;
    StartEnemyWave(group, clearing, 1, clearing->spawn, 1.0f);
}

void ClearEnemyAttacks(EnemyGroup *group) {
    memset(group->projectiles, 0, sizeof(group->projectiles));
    memset(group->cemeteryProjectiles,0,sizeof(group->cemeteryProjectiles));
    memset(group->rootAttacks, 0, sizeof(group->rootAttacks));
}

static Vector2 SpawnPosition(const Clearing *clearing, Vector2 player) {
    Rectangle bounds = clearing->playableBounds;
    for (int attempt = 0; attempt < 800; attempt++) {
        float x = bounds.x + GetRandomValue(35, (int)bounds.width - 35);
        float y = bounds.y + GetRandomValue(35, (int)bounds.height - 35);
        // With the whole arena visible, entry points stay near its edges.
        if (attempt < 500 && x > bounds.x + bounds.width*.25f &&
            x < bounds.x + bounds.width*.75f && y > bounds.y + bounds.height*.25f &&
            y < bounds.y + bounds.height*.75f) continue;
        Vector2 point = {x, y};
        if (!IsClearingBlocked(clearing, point) && Vector2Distance(point, player) > 230.0f)
            return point;
    }
    return clearing->spawn;
}

void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower) {
    group->cemetery=false;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    int total = 6 + (wave - 1) * 3;
    if (total > ENEMY_COUNT) total = ENEMY_COUNT;
    int specials = GetRandomValue(1, 1 + (wave - 1) / 2);
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        *enemy = (Enemy){.state = ENEMY_GONE};
        if (i >= total) continue;
        enemy->position = SpawnPosition(clearing, player);
        enemy->type = i < specials ? ENEMY_SPECIAL : i%3 == 0 ? ENEMY_COMMON_RANGED : ENEMY_COMMON_MELEE;
        float adjustment = 1.0f + fminf(.15f, fmaxf(0, playerPower - 1.0f) * .05f);
        enemy->maxHealth = (int)((enemy->type == ENEMY_SPECIAL ? 100 : 45) *
                                (1.0f + (wave-1)*.12f) * adjustment);
        enemy->health = enemy->maxHealth;
        enemy->speed = ENEMY_SPEED * (1.0f + (wave-1)*.025f);
        enemy->power = 1.0f + (wave-1)*.06f;
        enemy->state = ENEMY_SPAWNING;
        enemy->stateTime = -i*.35f;
        enemy->facing = i%2 ? 2 : 3;
        enemy->lastKatanaSwing = -1;
        enemy->attackCooldown = 0.8f;
    }
}

void StartClearingBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player, float power) {
    group->cemetery=false;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    for (int i=0; i<ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
    Enemy *boss = &group->list[0];
    *boss = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_SPAWNING,
        .position = SpawnPosition(clearing, player), .maxHealth = (int)(650*power),
        .speed = 65, .power = power, .attackCooldown = 2, .lastKatanaSwing = -1};
    boss->health = boss->maxHealth;
}

int EnemiesRemaining(const EnemyGroup *group) {
    int count = 0;
    for (int i=0; i<ENEMY_COUNT; i++) if (group->list[i].state != ENEMY_GONE) count++;
    return count;
}

float EnemyHurtRadius(const Enemy *enemy) { return enemy->type == ENEMY_BOSS ? 48.0f : ENEMY_HURT_RADIUS; }

bool EnemyCanBeHit(const Enemy *enemy) {
    if (enemy->dormant) return false;
    return enemy->state != ENEMY_DIE && enemy->state != ENEMY_GONE &&
           enemy->state != ENEMY_SPAWNING && enemy->state != ENEMY_FALLEN && enemy->health > 0;
}

Vector2 EnemyCenter(const Enemy *enemy) {
    return (Vector2){enemy->position.x, enemy->position.y-(enemy->type == ENEMY_BOSS ? 75.0f : 36.0f)};
}

void DamageEnemy(Enemy *enemy, int damage) {
    if (enemy->cemeteryUnit) { CemeteryDamageEnemy(enemy,damage); return; }
    if (!EnemyCanBeHit(enemy) || damage<=0) return;
    enemy->health -= damage;
    if (enemy->health <= 0) {
        enemy->stateTime = 0.0f;
        enemy->health = 0;
        enemy->state = ENEMY_DIE;
        if (!enemy->dropRolled) {
            enemy->dropRolled = true;
            enemy->pickupAvailable = EnemyShouldDrop(enemy->type, GetRandomValue(0, 9999));
        }
    } else if (enemy->type != ENEMY_BOSS) {
        enemy->stateTime = 0.0f;
        enemy->state = ENEMY_HURT;
    }
}

static void MoveEnemy(const Clearing *clearing, Enemy *enemy, Vector2 delta) {
    int steps = (int)(Vector2Length(delta)/3.0f)+1;
    Vector2 step = Vector2Scale(delta, 1.0f/steps);
    for (int i=0; i<steps; i++) {
        Vector2 next = Vector2Add(enemy->position, step);
        if (!IsClearingBlocked(clearing, next)) enemy->position = next;
        else {
            Vector2 x = {enemy->position.x+step.x, enemy->position.y};
            Vector2 y = {enemy->position.x, enemy->position.y+step.y};
            if (!IsClearingBlocked(clearing, x)) enemy->position = x;
            else if (!IsClearingBlocked(clearing, y)) enemy->position = y;
        }
    }
}

void KnockbackEnemy(Enemy *enemy, const Clearing *clearing,
                    Vector2 source, float distance) {
    if (!EnemyCanBeHit(enemy)) return;
    if (enemy->type == ENEMY_BOSS) distance *= .15f;
    Vector2 away = Vector2Subtract(enemy->position, source);
    if (Vector2LengthSqr(away) < 0.001f) return;
    MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(away), distance));
}

static void FireRangedProjectile(EnemyGroup *group, const Enemy *enemy, Vector2 player) {
    Vector2 origin = EnemyCenter(enemy);
    Vector2 toward = Vector2Subtract((Vector2){player.x, player.y - 35.0f}, origin);
    if (Vector2LengthSqr(toward) < 0.001f) return;
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        if (group->projectiles[i].remaining <= 0.0f) {
            group->projectiles[i] = (EnemyProjectile){origin, Vector2Normalize(toward), 1.5f,
                (int)(ENEMY_ATTACK_DAMAGE*enemy->power)};
            return;
        }
    }
}

void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float dt, bool playerDodging,
                   int *playerHealth, int playerMaxHealth) {
    if (group->cemetery) { CemeteryUpdateEnemies(group,clearing,player,dt,playerDodging,playerHealth,playerMaxHealth); return; }
    group->playerHitCooldown = fmaxf(0.0f, group->playerHitCooldown-dt);
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE && enemy->pickupAvailable &&
            *playerHealth > 0 && *playerHealth < playerMaxHealth &&
            Vector2Distance(enemy->position, player) < 26.0f) {
            *playerHealth += 15;
            if (*playerHealth > playerMaxHealth) *playerHealth = playerMaxHealth;
            enemy->pickupAvailable = false;
        }
        if (enemy->state == ENEMY_GONE) continue;
        if (enemy->state == ENEMY_FALLEN) continue;
        enemy->stateTime += dt;
        if (enemy->state == ENEMY_SPAWNING) {
            if (enemy->stateTime >= 1.0f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        enemy->attackCooldown = fmaxf(0.0f, enemy->attackCooldown-dt);
        enemy->bibleHitCooldown = fmaxf(0.0f, enemy->bibleHitCooldown-dt);
        if (enemy->state == ENEMY_DIE) {
            if (enemy->type == ENEMY_BOSS) ClearEnemyAttacks(group);
            if (enemy->stateTime >= 0.85f) {
                enemy->state = enemy->type == ENEMY_SPECIAL || enemy->type == ENEMY_BOSS ?
                               ENEMY_FALLEN : ENEMY_GONE;
            }
            continue;
        }
        if (enemy->state == ENEMY_HURT) {
            if (enemy->stateTime >= 0.32f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (enemy->state == ENEMY_ATTACK) {
            if (enemy->type == ENEMY_BOSS) {
                if (enemy->stateTime >= 1.5f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            if (enemy->type == ENEMY_SPECIAL) {
                if (enemy->stateTime >= .5f && enemy->stateTime < 1.0f) {
                    MoveEnemy(clearing, enemy, Vector2Scale(enemy->attackDirection, 360 * dt));
                    if (!enemy->attackConnected && !playerDodging && group->playerHitCooldown <= 0 &&
                        Vector2Distance(player, enemy->position) < 65 && *playerHealth > 0) {
                        *playerHealth = (int)fmaxf(0, *playerHealth - 18*enemy->power);
                        group->playerHitCooldown = .65f;
                        enemy->attackConnected = true;
                    }
                }
                if (enemy->stateTime >= 1.1f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            if (!enemy->attackConnected && enemy->stateTime >= 0.32f) {
                enemy->attackConnected = true;
                if (enemy->type == ENEMY_COMMON_RANGED) {
                    FireRangedProjectile(group, enemy, player);
                } else if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0 &&
                    Vector2Distance(enemy->position, player) <= ENEMY_ATTACK_RANGE+8.0f) {
                    *playerHealth -= (int)(ENEMY_ATTACK_DAMAGE * enemy->power);
                    if (*playerHealth < 0) *playerHealth = 0;
                    group->playerHitCooldown = 0.65f;
                }
            }
            if (enemy->stateTime >= 0.62f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (*playerHealth <= 0) { enemy->state = ENEMY_IDLE; continue; }
        Vector2 toPlayer = Vector2Subtract(player, enemy->position);
        float distance = Vector2Length(toPlayer);
        if (distance > 0.001f) {
            enemy->facing = fabsf(toPlayer.x) > fabsf(toPlayer.y)*0.65f
                ? (toPlayer.x < 0 ? 2 : 3) : (toPlayer.y < 0 ? 1 : 0);
        }
        if (enemy->type == ENEMY_BOSS || enemy->type == ENEMY_SPECIAL) {
            float range = enemy->type == ENEMY_BOSS ? 460 : 280;
            if (distance <= range && enemy->attackCooldown <= 0) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0;
                enemy->attackConnected = false;
                enemy->attackDirection = distance > .01f ? Vector2Normalize(toPlayer) : (Vector2){0,1};
                enemy->attackCooldown = enemy->type == ENEMY_BOSS ? 4.0f : 3.0f;
                if (enemy->type == ENEMY_BOSS) {
                    Vector2 centers[3] = {player, {player.x-100,player.y+45}, {player.x+100,player.y-45}};
                    for (int j=0; j<3; j++) if (!IsClearingBlocked(clearing, centers[j])) {
                        for (int k=0; k<ENEMY_ROOT_COUNT; k++) if (!group->rootAttacks[k].active) {
                            group->rootAttacks[k] = (EnemyRoot){.position=centers[j], .active=true};
                            break;
                        }
                    }
                }
            } else if (distance > (enemy->type == ENEMY_BOSS ? 210 : 100)) {
                enemy->state = ENEMY_WALK;
                enemy->walkTime += dt;
                MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), enemy->speed*dt));
            } else enemy->state = ENEMY_IDLE;
        } else if (enemy->type == ENEMY_COMMON_RANGED) {
            if (distance <= RANGED_MAX_DISTANCE && enemy->attackCooldown <= 0.0f) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0.0f;
                enemy->attackConnected = false;
                enemy->attackCooldown = 1.65f;
            } else if (distance < RANGED_MIN_DISTANCE || distance > RANGED_MAX_DISTANCE) {
                enemy->state = ENEMY_WALK;
                enemy->walkTime += dt;
                Vector2 step = Vector2Scale(Vector2Normalize(toPlayer), enemy->speed * dt);
                MoveEnemy(clearing, enemy, distance < RANGED_MIN_DISTANCE ?
                          Vector2Negate(step) : step);
            } else {
                enemy->state = ENEMY_IDLE;
            }
        } else if (distance <= ENEMY_ATTACK_RANGE && enemy->attackCooldown <= 0.0f) {
            enemy->state = ENEMY_ATTACK;
            enemy->stateTime = 0.0f;
            enemy->attackConnected = false;
            enemy->attackCooldown = 1.35f;
        } else if (distance > ENEMY_ATTACK_RANGE*0.85f) {
            enemy->state = ENEMY_WALK;
            enemy->walkTime += dt;
            MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), enemy->speed*dt));
        } else {
            enemy->state = ENEMY_IDLE;
        }
    }
    for (int i=0; i<ENEMY_ROOT_COUNT; i++) {
        EnemyRoot *root = &group->rootAttacks[i];
        if (!root->active) continue;
        root->age += dt;
        if (root->age >= .8f && root->age < 1.3f && !root->hit && !playerDodging &&
            group->playerHitCooldown <= 0 && *playerHealth > 0 &&
            Vector2Distance(player, root->position) < 62) {
            *playerHealth = (int)fmaxf(0, *playerHealth - 24 * group->list[0].power);
            group->playerHitCooldown = .65f;
            root->hit = true;
        }
        if (root->age >= 1.6f) root->active = false;
    }
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        Vector2 movement = Vector2Scale(shot->direction, RANGED_SHOT_SPEED * dt);
        int steps = (int)(Vector2Length(movement) / 6.0f) + 1;
        Vector2 step = Vector2Scale(movement, 1.0f / steps);
        for (int j = 0; j < steps && shot->remaining > 0.0f; j++) {
            Vector2 next = Vector2Add(shot->position, step);
            if (IsClearingBlocked(clearing, next)) {
                shot->remaining = 0.0f;
            } else {
                shot->position = next;
                Vector2 playerCenter = {player.x, player.y - 35.0f};
                if (Vector2DistanceSqr(next, playerCenter) <= 24.0f * 24.0f) {
                    if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0) {
                        *playerHealth -= shot->damage;
                        if (*playerHealth < 0) *playerHealth = 0;
                        group->playerHitCooldown = 0.65f;
                    }
                    shot->remaining = 0.0f;
                }
            }
        }
        if (shot->remaining > 0.0f) shot->remaining = fmaxf(0.0f, shot->remaining - dt);
    }
}

void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    if (group->cemetery) { CemeteryDrawEnemies(group,player,foreground); return; }
    if (!foreground) for (int i=0; i<ENEMY_ROOT_COUNT; i++) {
        const EnemyRoot *root = &group->rootAttacks[i];
        if (!root->active) continue;
        if (root->age < .8f) {
            DrawCircleV(root->position, 62, Fade(RED, .18f));
            DrawCircleLines((int)root->position.x, (int)root->position.y, 62, ORANGE);
        } else {
            int frame = (int)((root->age - .8f)*10);
            if (frame > 7) frame = 7;
            DrawTexturePro(group->roots, (Rectangle){frame*288.0f,0,288,256},
                (Rectangle){root->position.x-90,root->position.y-145,180,160}, (Vector2){0},0,WHITE);
        }
    }
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE && enemy->pickupAvailable) {
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 9.0f,
                        Fade(GOLD, 0.65f));
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 6.0f, RED);
        }
        if (enemy->state == ENEMY_GONE) continue;
        if (enemy->state == ENEMY_SPAWNING) {
            if (enemy->stateTime >= 0) DrawCircleLines((int)enemy->position.x,
                (int)enemy->position.y, 24 + 10*sinf(enemy->stateTime*8), GOLD);
            continue;
        }
        if (enemy->type == ENEMY_BOSS) {
            bool attack = enemy->state == ENEMY_ATTACK;
            float w = attack ? 384 : 256, h = attack ? 288 : 272;
            DrawTexturePro(attack ? group->bossAttack : group->bossSprites,
                (Rectangle){EnemyAnimationFrame(enemy)*w, enemy->facing*h,w,h},
                (Rectangle){enemy->position.x-w*.325f, enemy->position.y-(attack?280:264)*.65f,
                            w*.65f,h*.65f}, (Vector2){0},0,
                enemy->state == ENEMY_FALLEN || enemy->state == ENEMY_DIE ? Fade(WHITE,.45f) : WHITE);
            if (enemy->state == ENEMY_FALLEN) DrawText("E: selar",(int)enemy->position.x-30,(int)enemy->position.y+10,18,GOLD);
            continue;
        }
        int row = enemy->state == ENEMY_WALK ? 1 : enemy->state == ENEMY_ATTACK ? 2 :
                  enemy->state == ENEMY_HURT ? 3 : enemy->state == ENEMY_DIE ? 4 : 0;
        int frame = EnemyAnimationFrame(enemy);
        if (enemy->state == ENEMY_FALLEN) { row=0; frame=0; }
        Texture2D tex = group->sprites;
        if (enemy->type == ENEMY_COMMON_RANGED) {
            Rectangle src = {(enemy->facing * 4 + frame) * 80.0f, row * 80.0f,
                             80.0f, 80.0f};
            DrawTexturePro(group->rangedSprites, src,
                (Rectangle){enemy->position.x - 38, enemy->position.y - 75, 76, 76},
                (Vector2){0}, 0, WHITE);
        } else {
        if (enemy->facing < 2 && enemy->state != ENEMY_DIE) {
            tex = enemy->facing == 1 ? group->backSprites : group->frontSprites;
            if (enemy->state == ENEMY_ATTACK) {
                row = 1; /* Sem arte de ataque frontal/traseiro: movimento seguro. */
                frame = (int)(enemy->stateTime*6.5f);
                if (frame > 3) frame = 3;
            } else if (enemy->state == ENEMY_HURT) {
                row = 0;
                frame = 0;
            }
        }
        Rectangle src = {frame*80.0f, row*80.0f,
                         enemy->facing==2 && tex.id==group->sprites.id ? -80.0f : 80.0f, 80.0f};
        Color tint = enemy->state == ENEMY_HURT ? (Color){255,160,160,255} :
                     enemy->type == ENEMY_SPECIAL ? (Color){215,160,255,255} : WHITE;
        DrawTexturePro(tex, src,
            (Rectangle){enemy->position.x-38, enemy->position.y-75, 76, 76},
            (Vector2){0},0,tint);
        }
        if (enemy->state == ENEMY_FALLEN) DrawText("E: selar",(int)enemy->position.x-30,(int)enemy->position.y+10,16,GOLD);
        if (enemy->type == ENEMY_SPECIAL && enemy->state == ENEMY_ATTACK && enemy->stateTime < .5f)
            DrawCircleLines((int)enemy->position.x,(int)enemy->position.y,55,ORANGE);
        if (EnemyCanBeHit(enemy) && enemy->health < enemy->maxHealth) {
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,48,5,DARKGRAY);
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,
                          48*enemy->health/enemy->maxHealth,5,RED);
        }
    }
    if (foreground) for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        const EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        Vector2 tail = Vector2Subtract(shot->position, Vector2Scale(shot->direction, 16.0f));
        DrawLineEx(tail, shot->position, 3.0f, GOLD);
        DrawCircleV(shot->position, 3.0f, RAYWHITE);
    }
}

void DrawEnemyDebug(const EnemyGroup *group) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE) continue;
        Vector2 center = EnemyCenter(enemy);
        DrawCircleLines((int)center.x,(int)center.y,EnemyHurtRadius(enemy),GREEN);
        DrawCircleV(enemy->position,3.0f,YELLOW);
        DrawText(TextFormat("E%d F%d",enemy->state,EnemyAnimationFrame(enemy)),
                 (int)enemy->position.x-28,(int)enemy->position.y-97,12,GOLD);
    }
}
