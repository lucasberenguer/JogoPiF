#include "enemy.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define ENEMY_SPEED 98.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12
#define ENEMY_HURT_RADIUS 20.0f
#define RANGED_MIN_DISTANCE 170.0f
#define RANGED_MAX_DISTANCE 300.0f
#define RANGED_SHOT_SPEED 365.0f
#define ARENA_BOSS_BASE_HEALTH 1500
#define GUARDIAN_PHASE_TRANSITION_TIME 1.0f
#define GUARDIAN_ROOT_ECHO_TIME .95f
#define GUARDIAN_CURSE_ECHO_TIME .62f
#define GUARDIAN_ROOT_WARNING 0.8f
#define GUARDIAN_ROOT_RADIUS 74.0f
#define GUARDIAN_ROOT_DAMAGE 30
#define GUARDIAN_WAVE_WARNING .65f
#define GUARDIAN_WAVE_RADIUS 60.0f
#define GUARDIAN_WAVE_DAMAGE 36
#define GUARDIAN_WAVE_FPS 12.0f
#define GUARDIAN_WAVE_FRAMES 8
#define GUARDIAN_WAVE_TILE_WIDTH 352
#define GUARDIAN_WAVE_TILE_HEIGHT 288
#define GUARDIAN_CURSE_WARNING 1.0f
#define GUARDIAN_CURSE_FPS 12.0f
#define GUARDIAN_CURSE_CHARGE_FRAMES 8
#define GUARDIAN_CURSE_IMPACT_FRAMES 7
#define GUARDIAN_CURSE_TILE_WIDTH 384
#define GUARDIAN_CURSE_TILE_HEIGHT 320
#define GUARDIAN_MELEE_RANGE 118.0f
#define GUARDIAN_MELEE_DAMAGE 26
#define GUARDIAN_MELEE_COOLDOWN 1.65f
#define GUARDIAN_MELEE_FPS 10.0f
#define GUARDIAN_MELEE_FRAMES 8
#define GUARDIAN_MELEE_HIT_FRAME 4

typedef struct GuardianHazard {
    float warning, hitStart, hitEnd, duration, radius;
    int damage;
    Color warningColor;
} GuardianHazard;

static const GuardianHazard guardianHazards[GUARDIAN_ATTACK_COUNT] = {
    [GUARDIAN_ROOTS] = {GUARDIAN_ROOT_WARNING, GUARDIAN_ROOT_WARNING, 1.3f, 1.6f,
                        GUARDIAN_ROOT_RADIUS, GUARDIAN_ROOT_DAMAGE, ORANGE},
    [GUARDIAN_ROOT_WAVE] = {GUARDIAN_WAVE_WARNING,
        GUARDIAN_WAVE_WARNING + 2.0f / GUARDIAN_WAVE_FPS,
        GUARDIAN_WAVE_WARNING + 5.0f / GUARDIAN_WAVE_FPS,
        GUARDIAN_WAVE_WARNING + GUARDIAN_WAVE_FRAMES / GUARDIAN_WAVE_FPS,
        GUARDIAN_WAVE_RADIUS, GUARDIAN_WAVE_DAMAGE, GOLD},
    [GUARDIAN_CURSE] = {GUARDIAN_CURSE_WARNING,
        GUARDIAN_CURSE_WARNING + 1.0f / GUARDIAN_CURSE_FPS,
        GUARDIAN_CURSE_WARNING + 6.0f / GUARDIAN_CURSE_FPS,
        GUARDIAN_CURSE_WARNING + GUARDIAN_CURSE_IMPACT_FRAMES / GUARDIAN_CURSE_FPS,
        110.0f, 42, VIOLET},
};

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
        if (enemy->state == ENEMY_ATTACK && enemy->guardianMelee)
            return (int)fminf(enemy->stateTime * GUARDIAN_MELEE_FPS, GUARDIAN_MELEE_FRAMES - 1);
        if (enemy->state == ENEMY_ATTACK && enemy->bossAttack != GUARDIAN_ROOTS)
            return 0;
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
    group->bossPortrait = LoadTexture("assets/guardiao_intro_pixel_v2.png");
    group->cemeteryPortrait = LoadTexture("assets/coveiro_intro_pixel.png");
    group->bossAttack = LoadTexture("assets/boss_clareira_especial_1.png");
    group->roots = LoadTexture("assets/boss_clareira_raizes_area.png");
    group->rootWave = LoadTexture("assets/boss_clareira_onda.png");
    group->curse = LoadTexture("assets/boss_clareira_maldicao.png");
    group->guardianMelee = LoadTexture("assets/boss_clareira_corpo_a_corpo.png");
    if (!group->sprites.id || !group->backSprites.id || !group->frontSprites.id ||
        !group->rangedSprites.id || !group->bossSprites.id || !group->bossAttack.id ||
        !group->roots.id || !group->rootWave.id || !group->curse.id || !group->guardianMelee.id ||
        !group->bossPortrait.id || !group->cemeteryPortrait.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->backSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->frontSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rangedSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->cemeteryPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->roots, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rootWave, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->curse, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianMelee, TEXTURE_FILTER_POINT);
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
    if (group->bossPortrait.id) UnloadTexture(group->bossPortrait);
    if (group->cemeteryPortrait.id) UnloadTexture(group->cemeteryPortrait);
    if (group->bossAttack.id) UnloadTexture(group->bossAttack);
    if (group->roots.id) UnloadTexture(group->roots);
    if (group->rootWave.id) UnloadTexture(group->rootWave);
    if (group->curse.id) UnloadTexture(group->curse);
    if (group->guardianMelee.id) UnloadTexture(group->guardianMelee);
    *group = (EnemyGroup){0};
}

void ResetEnemies(EnemyGroup *group, const Clearing *clearing) {
    StartEnemyWave(group, clearing, 1, clearing->spawn, 1.0f);
}

void ClearEnemyAttacks(EnemyGroup *group) {
    memset(group->projectiles, 0, sizeof(group->projectiles));
    memset(group->cemeteryProjectiles,0,sizeof(group->cemeteryProjectiles));
    memset(group->rootAttacks, 0, sizeof(group->rootAttacks));
}

static Vector2 SpawnPosition(const Clearing *clearing, Vector2 player) {
    Rectangle bounds = clearing->playableBounds;
    Vector2 fallback = clearing->spawn;
    bool hasFallback = false;
    for (int attempt = 0; attempt < 800; attempt++) {
        float x = bounds.x + GetRandomValue(35, (int)bounds.width - 35);
        float y = bounds.y + GetRandomValue(35, (int)bounds.height - 35);
        // With the whole arena visible, entry points stay near its edges.
        if (attempt < 500 && x > bounds.x + bounds.width*.25f &&
            x < bounds.x + bounds.width*.75f && y > bounds.y + bounds.height*.25f &&
            y < bounds.y + bounds.height*.75f) continue;
        Vector2 point = {x, y};
        if (Vector2Distance(point, player) <= 230.0f || IsMapActorBlocked(clearing, point)) continue;
        if (!hasFallback) {
            fallback = point;
            hasFallback = true;
        }

        // Prefer a clear approach so the enemy can start chasing immediately.
        if (MapHasClearPath(clearing, point, player)) return point;
    }
    if (!hasFallback) FindFreeMapPosition(clearing, fallback, &fallback);
    return fallback;
}

void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower) {
    group->cemetery=clearing->cemetery;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    int total = 7 + (wave - 1) * 4;
    if (total > ENEMY_COUNT) total = ENEMY_COUNT;
    int minimumSpecials = 1 + (wave - 1) / 2;
    int specials = GetRandomValue(minimumSpecials, minimumSpecials + (wave >= 3));
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
        enemy->speed = ENEMY_SPEED * (1.0f + (wave-1)*.045f);
        enemy->power = 1.0f + (wave-1)*.09f;
        enemy->state = ENEMY_SPAWNING;
        enemy->stateTime = -i*(.30f - (wave-1)*.025f);
        enemy->facing = i%2 ? 2 : 3;
        enemy->lastKatanaSwing = -1;
        enemy->attackCooldown = 0.8f;
    }
}

void StartArenaBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player, float power) {
    group->cemetery=clearing->cemetery;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    for (int i=0; i<ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
    Enemy *boss = &group->list[0];
    *boss = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_SPAWNING,
        .position = SpawnPosition(clearing, player), .maxHealth = (int)(ARENA_BOSS_BASE_HEALTH*fmaxf(1, power)),
        .speed = 100, .power = power, .attackCooldown = 1.25f, .lastKatanaSwing = -1,
        .bossAttack = (clearing->cemetery ? COVEIRO_ATTACK_COUNT : GUARDIAN_ATTACK_COUNT) - 1};
    boss->health = boss->maxHealth;
}

int EnemiesRemaining(const EnemyGroup *group) {
    int count = 0;
    for (int i=0; i<ENEMY_COUNT; i++) if (group->list[i].state != ENEMY_GONE) count++;
    return count;
}

float EnemyHurtRadius(const Enemy *enemy) { return enemy->type == ENEMY_BOSS ? 48.0f : ENEMY_HURT_RADIUS; }

bool EnemyCanBeHit(const Enemy *enemy) {
    if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) return false;
    return enemy->state != ENEMY_DIE && enemy->state != ENEMY_GONE &&
           enemy->state != ENEMY_SPAWNING && enemy->state != ENEMY_FALLEN && enemy->health > 0;
}

Vector2 EnemyCenter(const Enemy *enemy) {
    return (Vector2){enemy->position.x, enemy->position.y-(enemy->type == ENEMY_BOSS ? 75.0f : 36.0f)};
}

void DamageEnemy(Enemy *enemy, int damage) {
    if (!EnemyCanBeHit(enemy) || damage<=0) return;
    enemy->health -= damage;
    if (enemy->type == ENEMY_BOSS && enemy->guardianPhase == GUARDIAN_PHASE_ONE &&
        enemy->health <= enemy->maxHealth / 2) {
        enemy->health = enemy->maxHealth / 2;
        enemy->guardianPhase = GUARDIAN_PHASE_TRANSITION;
        enemy->guardianMelee = false;
        enemy->state = ENEMY_IDLE;
        enemy->stateTime = 0.0f;
    } else if (enemy->health <= 0) {
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

void MoveEnemy(const Clearing *clearing, Enemy *enemy, Vector2 delta) {
    MoveMapActor(clearing, &enemy->position, delta);
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
    Vector2 origin = group->cemetery ? enemy->position : EnemyCenter(enemy);
    Vector2 toward = group->cemetery ? enemy->attackDirection :
                     Vector2Subtract((Vector2){player.x, player.y - 35.0f}, origin);
    if (Vector2LengthSqr(toward) < 0.001f) return;
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        if (group->projectiles[i].remaining <= 0.0f) {
            group->projectiles[i] = (EnemyProjectile){origin, Vector2Normalize(toward),
                group->cemetery ? 4.0f : 1.5f,
                (int)((group->cemetery ? 10 : ENEMY_ATTACK_DAMAGE)*enemy->power)};
            return;
        }
    }
}

static bool GuardianEnraged(const Enemy *enemy) {
    return enemy->guardianPhase == GUARDIAN_PHASE_TWO;
}

static void PlaceGuardianRoot(EnemyGroup *group, Vector2 position, GuardianAttack attack, float delay) {
    for (int i = 0; i < ENEMY_ROOT_COUNT; i++) {
        if (group->rootAttacks[i].active) continue;
        group->rootAttacks[i] = (EnemyRoot){.position = position, .age = -delay,
                                           .active = true, .attack = attack};
        return;
    }
}

static void StartGuardianCast(EnemyGroup *group, const Clearing *clearing,
                              Enemy *enemy, Vector2 player) {
    enemy->attackCooldown = GuardianEnraged(enemy) ? 1.95f : 3.0f;
    enemy->bossAttack = (enemy->bossAttack + 1) % GUARDIAN_ATTACK_COUNT;
    if (enemy->bossAttack == GUARDIAN_ROOTS) {
        Vector2 flank = Vector2Scale(Vector2Rotate(enemy->attackDirection, PI / 2), 120);
        const Vector2 targets[] = {player, Vector2Add(player, flank), Vector2Subtract(player, flank)};
        for (int i = 0; i < 3; i++) {
            if (!IsClearingBlocked(clearing, targets[i])) PlaceGuardianRoot(group, targets[i], GUARDIAN_ROOTS, 0);
        }
        return;
    }

    if (enemy->bossAttack == GUARDIAN_CURSE) {
        if (!IsClearingBlocked(clearing, player))
            PlaceGuardianRoot(group, player, GUARDIAN_CURSE, 0);
        return;
    }

    int segments = GuardianEnraged(enemy) ? 6 : 5;
    float stagger = GuardianEnraged(enemy) ? .08f : .10f;
    for (int i = 0; i < segments; i++) {
        float distance = 85 + i * 105;
        Vector2 position = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, distance));
        // Probe between eruptions too, so the wave ends at walls and rocks.
        for (float sample = (i == 0 ? 12 : distance - 96); sample < distance; sample += 9) {
            Vector2 point = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, sample));
            if (IsClearingBlocked(clearing, point)) return;
        }
        if (IsClearingBlocked(clearing, position)) return;
        PlaceGuardianRoot(group, position, GUARDIAN_ROOT_WAVE, i * stagger);
    }
}

static void UpdateGuardianMelee(EnemyGroup *group, const Clearing *clearing, Enemy *enemy, Vector2 player,
                                bool playerDodging, int *playerHealth) {
    if (!enemy->attackConnected && EnemyAnimationFrame(enemy) >= GUARDIAN_MELEE_HIT_FRAME) {
        enemy->attackConnected = true;
        Vector2 offset = Vector2Subtract(player, enemy->position);
        float distance = Vector2Length(offset);
        if (!playerDodging && *playerHealth > 0 && group->playerHitCooldown <= 0 &&
            distance <= GUARDIAN_MELEE_RANGE &&
            Vector2DotProduct(offset, enemy->attackDirection) >= distance * .48f &&
            MapHasClearPath(clearing, enemy->position, player)) {
            *playerHealth = (int)fmaxf(0, *playerHealth - GUARDIAN_MELEE_DAMAGE * enemy->power);
            group->playerHitCooldown = .65f;
        }
    }
    if (enemy->stateTime >= GUARDIAN_MELEE_FRAMES / GUARDIAN_MELEE_FPS) {
        enemy->state = ENEMY_IDLE;
        enemy->stateTime = 0;
        enemy->guardianMelee = false;
    }
}

void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float dt, bool playerDodging,
                   int *playerHealth, int playerMaxHealth) {
    group->playerHitCooldown = fmaxf(0.0f, group->playerHitCooldown-dt);
    if (group->cemetery && group->list[0].type == ENEMY_BOSS &&
        (group->list[0].guardianPhase == GUARDIAN_PHASE_TRANSITION || group->list[0].state == ENEMY_DIE))
        ClearEnemyAttacks(group);
    if (group->cemetery)
        CemeteryUpdateProjectiles(group, clearing, player, dt, playerDodging, playerHealth);
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
        if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
            ClearEnemyAttacks(group);
            if (enemy->stateTime >= GUARDIAN_PHASE_TRANSITION_TIME) {
                enemy->guardianPhase = GUARDIAN_PHASE_TWO;
                enemy->stateTime = 0;
                /* The Coveiro opens with a charge and the Guardian with a curse. */
                enemy->bossAttack = group->cemetery ? COVEIRO_SHOVEL : GUARDIAN_ROOT_WAVE;
                enemy->attackCooldown = .20f;
                enemy->guardianMeleeCooldown = .90f;
            }
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            if (enemy->stateTime >= 1.0f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        enemy->attackCooldown = fmaxf(0.0f, enemy->attackCooldown-dt);
        if (enemy->type == ENEMY_BOSS)
            enemy->guardianMeleeCooldown = fmaxf(0.0f, enemy->guardianMeleeCooldown - dt);
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
        if (group->cemetery && enemy->type == ENEMY_BOSS) {
            CemeteryUpdateBoss(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (enemy->state == ENEMY_ATTACK) {
            if (enemy->type == ENEMY_BOSS) {
                if (enemy->guardianMelee) {
                    UpdateGuardianMelee(group, clearing, enemy, player, playerDodging, playerHealth);
                    continue;
                }
                bool roots = enemy->bossAttack == GUARDIAN_ROOTS;
                bool curse = enemy->bossAttack == GUARDIAN_CURSE;
                float echoTime = roots ? GUARDIAN_ROOT_ECHO_TIME : GUARDIAN_CURSE_ECHO_TIME;
                if (GuardianEnraged(enemy) && (roots || curse) &&
                    !enemy->attackConnected && enemy->stateTime >= echoTime) {
                    enemy->attackConnected = true;
                    if (!IsClearingBlocked(clearing, player))
                        PlaceGuardianRoot(group, player, roots ? GUARDIAN_ROOTS : GUARDIAN_CURSE, 0);
                }
                if (enemy->stateTime >= 1.5f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            if (enemy->type == ENEMY_SPECIAL) {
                if (enemy->stateTime >= .5f && enemy->stateTime < 1.0f) {
                    MoveEnemy(clearing, enemy, Vector2Scale(enemy->attackDirection, 360 * dt));
                    if (!enemy->attackConnected && !playerDodging && group->playerHitCooldown <= 0 &&
                        Vector2Distance(player, enemy->position) < 65 && *playerHealth > 0 &&
                        MapHasClearPath(clearing, enemy->position, player)) {
                        *playerHealth = (int)fmaxf(0, *playerHealth - 18*enemy->power);
                        group->playerHitCooldown = .65f;
                        enemy->attackConnected = true;
                    }
                }
                if (enemy->stateTime >= 1.1f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            bool mage = group->cemetery && enemy->type == ENEMY_COMMON_RANGED;
            if (!enemy->attackConnected && enemy->stateTime >= (mage ? .55f : .32f)) {
                enemy->attackConnected = true;
                if (enemy->type == ENEMY_COMMON_RANGED) {
                    FireRangedProjectile(group, enemy, player);
                } else if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0 &&
                    Vector2Distance(enemy->position, player) <= ENEMY_ATTACK_RANGE+8.0f &&
                    MapHasClearPath(clearing, enemy->position, player)) {
                    *playerHealth -= (int)(ENEMY_ATTACK_DAMAGE * enemy->power);
                    if (*playerHealth < 0) *playerHealth = 0;
                    group->playerHitCooldown = 0.65f;
                }
            }
            if (enemy->stateTime >= (mage ? .95f : .62f)) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
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
            float range = enemy->type == ENEMY_BOSS ? 560 : 280;
            bool melee = enemy->type == ENEMY_BOSS && distance <= GUARDIAN_MELEE_RANGE &&
                         enemy->guardianMeleeCooldown <= 0;
            if (melee || (distance <= range && enemy->attackCooldown <= 0)) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0;
                enemy->attackConnected = false;
                enemy->attackDirection = distance > .01f ? Vector2Normalize(toPlayer) : (Vector2){0,1};
                if (melee) {
                    const Vector2 directions[] = {{0,1}, {0,-1}, {-1,0}, {1,0}};
                    enemy->guardianMelee = true;
                    enemy->guardianMeleeCooldown = GuardianEnraged(enemy) ? 1.25f : GUARDIAN_MELEE_COOLDOWN;
                    enemy->attackDirection = directions[enemy->facing];
                } else if (enemy->type == ENEMY_BOSS) {
                    StartGuardianCast(group, clearing, enemy, player);
                } else {
                    enemy->attackCooldown = 2.75f / fminf(enemy->power, 1.36f);
                }
            } else if (distance > (enemy->type == ENEMY_BOSS ? 110 : 100)) {
                enemy->state = ENEMY_WALK;
                enemy->walkTime += dt;
                float speed = enemy->speed * (GuardianEnraged(enemy) ? 1.5f : 1.0f);
                MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), speed*dt));
            } else enemy->state = ENEMY_IDLE;
        } else if (enemy->type == ENEMY_COMMON_RANGED) {
            if (distance <= RANGED_MAX_DISTANCE && enemy->attackCooldown <= 0.0f) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0.0f;
                enemy->attackConnected = false;
                enemy->attackDirection = Vector2Normalize(toPlayer);
                enemy->attackCooldown = 1.5f / fminf(enemy->power, 1.36f);
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
            enemy->attackCooldown = 1.25f / fminf(enemy->power, 1.36f);
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
        const GuardianHazard *hazard = &guardianHazards[root->attack];
        if (root->age >= hazard->hitStart && root->age < hazard->hitEnd && !root->hit && !playerDodging &&
            group->playerHitCooldown <= 0 && *playerHealth > 0 &&
            Vector2Distance(player, root->position) < hazard->radius) {
            *playerHealth = (int)fmaxf(0, *playerHealth - hazard->damage * group->list[0].power);
            group->playerHitCooldown = .65f;
            root->hit = true;
        }
        if (root->age >= hazard->duration) root->active = false;
    }
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        float speed = group->cemetery ? 210 : RANGED_SHOT_SPEED;
        Vector2 movement = Vector2Scale(shot->direction, speed * dt);
        int steps = (int)(Vector2Length(movement) / 6.0f) + 1;
        Vector2 step = Vector2Scale(movement, 1.0f / steps);
        for (int j = 0; j < steps && shot->remaining > 0.0f; j++) {
            Vector2 next = Vector2Add(shot->position, step);
            if (IsClearingBlocked(clearing, next)) {
                shot->remaining = 0.0f;
            } else {
                shot->position = next;
                Vector2 playerCenter = {player.x, player.y - (group->cemetery ? 0 : 35.0f)};
                float radius = group->cemetery ? 19 : 24;
                if (Vector2DistanceSqr(next, playerCenter) <= radius * radius) {
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

static void DrawGuardianWaveFrame(Texture2D texture, Vector2 position, float elapsed,
                                  bool eruption, bool flip) {
    int frame = (int)(elapsed * GUARDIAN_WAVE_FPS);
    if (frame >= GUARDIAN_WAVE_FRAMES) return;
    const float scale = .50f;
    const float width = GUARDIAN_WAVE_TILE_WIDTH * scale;
    const float height = GUARDIAN_WAVE_TILE_HEIGHT * scale;
    float ground = (eruption ? 217 : 252) * scale;
    DrawTexturePro(texture,
        (Rectangle){frame * GUARDIAN_WAVE_TILE_WIDTH, eruption ? GUARDIAN_WAVE_TILE_HEIGHT : 0,
                    flip ? -GUARDIAN_WAVE_TILE_WIDTH : GUARDIAN_WAVE_TILE_WIDTH, GUARDIAN_WAVE_TILE_HEIGHT},
        (Rectangle){position.x - width / 2, position.y - ground, width, height},
        (Vector2){0}, 0, WHITE);
}

static void DrawGuardianCurse(Texture2D texture, const EnemyRoot *root, const GuardianHazard *hazard) {
    bool charging = root->age < hazard->warning;
    if (root->age < hazard->hitEnd) {
        DrawCircleV(root->position, hazard->radius, Fade(PURPLE, charging ? .23f : .36f));
        DrawCircleLines((int)root->position.x, (int)root->position.y, hazard->radius, hazard->warningColor);
    }
    float elapsed = root->age - hazard->warning;
    if (charging) elapsed += GUARDIAN_CURSE_CHARGE_FRAMES / GUARDIAN_CURSE_FPS;
    if (elapsed < 0) return;
    int count = charging ? GUARDIAN_CURSE_CHARGE_FRAMES : GUARDIAN_CURSE_IMPACT_FRAMES;
    int frame = (int)(elapsed * GUARDIAN_CURSE_FPS);
    if (frame >= count) frame = count - 1;
    const float scale = .80f;
    DrawTexturePro(texture,
        (Rectangle){frame * GUARDIAN_CURSE_TILE_WIDTH, charging ? 0 : GUARDIAN_CURSE_TILE_HEIGHT,
                    GUARDIAN_CURSE_TILE_WIDTH, GUARDIAN_CURSE_TILE_HEIGHT},
        (Rectangle){root->position.x - GUARDIAN_CURSE_TILE_WIDTH * scale / 2,
                    root->position.y - 250 * scale,
                    GUARDIAN_CURSE_TILE_WIDTH * scale, GUARDIAN_CURSE_TILE_HEIGHT * scale},
        (Vector2){0}, 0, WHITE);
}

static void DrawRangedProjectiles(const EnemyGroup *group) {
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        const EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        if (group->cemetery) {
            DrawCircleV(shot->position, 14, Fade(GREEN, .3f));
            DrawCircleV(shot->position, 8, LIME);
            DrawCircleV(shot->position, 3, RAYWHITE);
            continue;
        }
        Vector2 tail = Vector2Subtract(shot->position, Vector2Scale(shot->direction, 16.0f));
        DrawLineEx(tail, shot->position, 3.0f, GOLD);
        DrawCircleV(shot->position, 3.0f, RAYWHITE);
    }
}

void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    if (group->cemetery) {
        CemeteryDrawEnemies(group,player,foreground);
        if (foreground) DrawRangedProjectiles(group);
        return;
    }
    if (!foreground) for (int i=0; i<ENEMY_ROOT_COUNT; i++) {
        const EnemyRoot *root = &group->rootAttacks[i];
        if (!root->active) continue;
        const GuardianHazard *hazard = &guardianHazards[root->attack];
        if (root->attack == GUARDIAN_CURSE) {
            DrawGuardianCurse(group->curse, root, hazard);
        } else if (root->age < hazard->warning) {
            DrawCircleV(root->position, hazard->radius, Fade(RED, .18f));
            DrawCircleLines((int)root->position.x, (int)root->position.y, hazard->radius, hazard->warningColor);
        } else if (root->attack == GUARDIAN_ROOT_WAVE) {
            DrawGuardianWaveFrame(group->rootWave, root->position, root->age - hazard->warning, true,
                                  group->list[0].attackDirection.x < 0);
        } else {
            int frame = (int)((root->age - hazard->warning)*10);
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
            if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
                float progress = fminf(enemy->stateTime / GUARDIAN_PHASE_TRANSITION_TIME, 1);
                DrawCircleV(enemy->position, 50 + 120 * progress, Fade(PURPLE, .35f * (1 - progress)));
                DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                50 + 120 * progress, Fade(VIOLET, 1 - progress));
            } else if (GuardianEnraged(enemy) && enemy->health > 0) {
                DrawCircleV(enemy->position, 43, Fade(PURPLE, .26f));
            }
            bool melee = enemy->state == ENEMY_ATTACK && enemy->guardianMelee;
            bool castingWave = enemy->state == ENEMY_ATTACK && !melee && enemy->bossAttack == GUARDIAN_ROOT_WAVE;
            bool attack = enemy->state == ENEMY_ATTACK && !melee && enemy->bossAttack == GUARDIAN_ROOTS;
            if (castingWave)
                DrawGuardianWaveFrame(group->rootWave, enemy->position, enemy->stateTime, false,
                                      enemy->attackDirection.x < 0);
            bool widePose = attack || melee;
            float w = widePose ? 384 : 256, h = widePose ? 288 : 272;
            Texture2D texture = melee ? group->guardianMelee : attack ? group->bossAttack : group->bossSprites;
            DrawTexturePro(texture,
                (Rectangle){EnemyAnimationFrame(enemy)*w, enemy->facing*h,w,h},
                (Rectangle){enemy->position.x-w*.325f, enemy->position.y-(widePose?280:264)*.65f,
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
    if (foreground) DrawRangedProjectiles(group);
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
