#include "enemy.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>

enum { VILLAGE_AXE_SLASH, VILLAGE_CHARGE, VILLAGE_SEAL_PULSE, VILLAGE_ATTACK_COUNT };

static Vector2 VillageHordeSpot(const Clearing *clearing, Vector2 player) {
    float w = (float)clearing->background.width, h = (float)clearing->background.height;
    for (int attempt = 0; attempt < 48; attempt++) {
        Vector2 cand = {
            w * (0.24f + 0.58f * (float)GetRandomValue(0, 100) / 100.0f),
            h * (0.30f + 0.45f * (float)GetRandomValue(0, 100) / 100.0f)
        };
        if (!IsMapActorBlocked(clearing, cand) &&
            Vector2Distance(cand, player) >= 150.0f) {
            return cand;
        }
    }
    Vector2 fallback = {w * 0.56f, h * 0.54f};
    FindFreeMapPosition(clearing, fallback, &fallback);
    return fallback;
}

static void SpawnNecroticVillager(Enemy *enemy, const Clearing *clearing,
                                  Vector2 player, float power) {
    int hp = (int)ceilf(48.0f * fmaxf(1.0f, power));
    *enemy = (Enemy){
        .type = ENEMY_COMMON_MELEE,
        .state = ENEMY_SPAWNING,
        .position = VillageHordeSpot(clearing, player),
        .health = hp,
        .maxHealth = hp,
        .speed = 118.0f,
        .power = power,
        .attackCooldown = 0.45f,
        .lastKatanaSwing = -1
    };
}

void StartVillageHorde(EnemyGroup *group, const Clearing *clearing,
                       Vector2 player, float power) {
    group->village = true;
    group->cemetery = false;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0.0f;
    for (int i = 0; i < ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
    for (int i = 0; i < 19 && i < ENEMY_COUNT; i++) {
        SpawnNecroticVillager(&group->list[i], clearing, player, power);
        group->list[i].attackCooldown = 0.35f + 0.08f * (float)(i % 6);
    }
    group->villageSpawnRemaining = 12;
    group->villageSpawnTimer = 0.65f;
    group->villageHordePower = power;
}

void VillageUpdateHorde(EnemyGroup *group, const Clearing *clearing,
                        Vector2 player, float dt) {
    if (!group->village || group->villageSpawnRemaining <= 0) return;
    group->villageSpawnTimer -= dt;
    if (group->villageSpawnTimer > 0.0f) return;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (group->list[i].state == ENEMY_GONE) {
            SpawnNecroticVillager(&group->list[i], clearing, player, group->villageHordePower);
            group->villageSpawnRemaining--;
            group->villageSpawnTimer = 0.65f;
            return;
        }
    }
    group->villageSpawnTimer = 0.25f;
}

static int VillageFacing(Vector2 delta, int fallback) {
    if (Vector2LengthSqr(delta) < 0.001f) return fallback;
    if (fabsf(delta.x) >= fabsf(delta.y)) return delta.x < 0.0f ? 2 : 3;
    return delta.y < 0.0f ? 1 : 0;
}

static void HurtPlayer(EnemyGroup *group, int *playerHealth, bool dodging, int damage) {
    if (dodging || group->playerHitCooldown > 0.0f || *playerHealth <= 0) return;
    *playerHealth -= damage;
    if (*playerHealth < 0) *playerHealth = 0;
    group->playerHitCooldown = 0.55f;
}

void VillageUpdateBoss(EnemyGroup *group, const Clearing *map, Enemy *boss,
                       Vector2 player, float dt, bool dodging, int *playerHealth) {
    if (boss->guardianPhase == GUARDIAN_PHASE_ONE && boss->health <= boss->maxHealth / 2) {
        boss->guardianPhase = GUARDIAN_PHASE_TWO;
    }
    Vector2 delta = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(delta);
    Vector2 toward = distance > 0.001f ? Vector2Scale(delta, 1.0f / distance) : (Vector2){0.0f, 1.0f};

    if (boss->state != ENEMY_ATTACK) {
        boss->facing = VillageFacing(delta, boss->facing);
        if (boss->attackCooldown <= 0.0f && distance < 320.0f) {
            int maxAtk = boss->guardianPhase == GUARDIAN_PHASE_TWO
                ? VILLAGE_ATTACK_COUNT : VILLAGE_SEAL_PULSE;
            if (distance > 115.0f && boss->guardianPhase == GUARDIAN_PHASE_ONE)
                boss->bossAttack = VILLAGE_CHARGE;
            else
                boss->bossAttack = (boss->bossAttack + 1) % maxAtk;
            boss->state = ENEMY_ATTACK;
            boss->stateTime = 0.0f;
            boss->attackConnected = false;
            boss->attackDirection = toward;
            return;
        }
        if (distance > 78.0f) {
            boss->state = ENEMY_WALK;
            boss->walkTime += dt;
            MoveEnemy(map, boss, Vector2Scale(toward, boss->speed * dt));
        } else {
            boss->state = ENEMY_IDLE;
        }
        return;
    }

    float t = boss->stateTime;
    boss->facing = VillageFacing(boss->attackDirection, boss->facing);
    if (boss->bossAttack == VILLAGE_AXE_SLASH) {
        if (t >= 0.52f && t <= 0.65f) MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, 140.0f * dt));
        if (!boss->attackConnected && t >= 0.55f) {
            boss->attackConnected = true;
            if (distance <= 112.0f &&
                Vector2DotProduct(toward, boss->attackDirection) >= -0.15f) {
                HurtPlayer(group, playerHealth, dodging, (int)(18.0f * boss->power));
            }
        }
        if (t >= 1.00f) {
            boss->state = ENEMY_IDLE;
            boss->stateTime = 0.0f;
            boss->attackCooldown = boss->guardianPhase == GUARDIAN_PHASE_TWO ? 0.65f : 0.90f;
        }
    } else if (boss->bossAttack == VILLAGE_CHARGE) {
        if (t >= 0.65f && t < 1.10f) {
            MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, boss->speed * 2.55f * dt));
            if (!boss->attackConnected && Vector2Distance(boss->position, player) <= 44.0f) {
                boss->attackConnected = true;
                HurtPlayer(group, playerHealth, dodging, (int)(20.0f * boss->power));
            }
        }
        if (t >= 1.65f) {
            boss->state = ENEMY_IDLE;
            boss->stateTime = 0.0f;
            boss->attackCooldown = boss->guardianPhase == GUARDIAN_PHASE_TWO ? 0.75f : 1.05f;
        }
    } else {
        if (!boss->attackConnected && t >= 0.85f) {
            boss->attackConnected = true;
            if (distance <= 122.0f) {
                HurtPlayer(group, playerHealth, dodging, (int)(18.0f * boss->power));
            }
        }
        if (t >= 1.30f) {
            boss->state = ENEMY_IDLE;
            boss->stateTime = 0.0f;
            boss->attackCooldown = 0.85f;
        }
    }
}

void VillageDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE) continue;
        if ((enemy->position.y >= player.y) != foreground) continue;

        if (enemy->type == ENEMY_BOSS) {
            if (!foreground && enemy->state == ENEMY_ATTACK) {
                if (enemy->bossAttack == VILLAGE_CHARGE && enemy->stateTime < 0.65f) {
                    Vector2 end = Vector2Add(enemy->position,
                        Vector2Scale(enemy->attackDirection, 190.0f));
                    DrawLineEx(enemy->position, end, 4.0f, Fade(MAROON, 0.55f));
                } else if (enemy->bossAttack == VILLAGE_SEAL_PULSE && enemy->stateTime < 0.85f) {
                    float p = enemy->stateTime / 0.85f;
                    DrawCircleV(enemy->position, 122.0f, Fade(GOLD, 0.14f + 0.12f * p));
                    DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                    122.0f, Fade(GOLD, 0.75f));
                }
            }
            Texture2D tex = group->villageBossWalk;
            int rows = 4, row = enemy->facing, frame = 0;
            bool flip = false;
            if (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN) {
                tex = group->villageBossCharge;
                rows = 3;
                row = 2;
                frame = enemy->state == ENEMY_FALLEN ? 5
                    : (int)fminf(enemy->stateTime / 1.1f * 6.0f, 5.0f);
            } else if (enemy->state == ENEMY_ATTACK && enemy->bossAttack == VILLAGE_CHARGE) {
                tex = group->villageBossCharge;
                rows = 3;
                row = (enemy->facing == 0 || enemy->facing == 1) ? 1 : 0;
                flip = (enemy->facing == 2);
                frame = enemy->stateTime < 0.65f ? 0
                    : (enemy->stateTime < 1.10f ? 1 + (int)(enemy->stateTime * 10.0f) % 3
                                                : 4 + (int)fminf((enemy->stateTime - 1.10f) * 3.5f, 1.0f));
            } else if (enemy->state == ENEMY_ATTACK) {
                tex = group->villageBossAttack;
                row = (enemy->facing == 2) ? 2 : enemy->facing;
                if (enemy->facing == 2) { row = 2; flip = true; }
                frame = enemy->stateTime < 0.55f
                    ? (int)fminf(enemy->stateTime / 0.55f * 2.0f, 1.0f)
                    : (enemy->stateTime < 0.75f ? 2 + (int)((enemy->stateTime - 0.55f) / 0.20f * 2.0f)
                                                : 4 + (int)fminf((enemy->stateTime - 0.75f) / 0.25f * 2.0f, 1.0f));
            } else {
                if (enemy->facing == 2) { row = 2; flip = true; }
                else if (enemy->facing == 3) row = 2;
                else row = enemy->facing;
                frame = enemy->state == ENEMY_WALK ? ((int)(enemy->walkTime * 9.0f) % 6) : 0;
            }
            float cw = (float)tex.width / 6.0f;
            float ch = (float)tex.height / (float)rows;
            Rectangle src = {frame * cw, row * ch, flip ? -cw : cw, ch};
            float drawW = 104.0f, drawH = 104.0f * (ch / cw);
            Rectangle dst = {enemy->position.x - drawW * 0.5f,
                             enemy->position.y - drawH * 0.88f, drawW, drawH};
            Color tint = enemy->state == ENEMY_HURT ? (Color){255, 160, 160, 255} : WHITE;
            DrawTexturePro(tex, src, dst, (Vector2){0}, 0.0f, tint);
            continue;
        }

        /* Necrotic villager (6x4: row 0 emerge, row 1 down, row 2 right, row 3 attack) */
        Texture2D vtex = group->villageVillager;
        float cw = (float)vtex.width / 6.0f;
        float ch = (float)vtex.height / 4.0f;
        int row = 1, frame = 0;
        bool flip = false;
        if (enemy->state == ENEMY_SPAWNING) {
            row = 0;
            frame = (int)fminf(enemy->stateTime * 6.0f, 5.0f);
        } else if (enemy->state == ENEMY_ATTACK) {
            row = 3;
            flip = (enemy->facing == 2);
            frame = (int)fminf(enemy->stateTime * 10.0f, 5.0f);
        } else if (enemy->facing == 0 || enemy->facing == 1) {
            row = 1;
            frame = enemy->state == ENEMY_WALK ? ((int)(enemy->walkTime * 9.0f) % 6) : 0;
        } else {
            row = 2;
            flip = (enemy->facing == 2);
            frame = enemy->state == ENEMY_WALK ? ((int)(enemy->walkTime * 9.0f) % 6) : 0;
        }
        Rectangle src = {frame * cw, row * ch, flip ? -cw : cw, ch};
        float drawW = 86.0f, drawH = 86.0f * (ch / cw);
        Rectangle dst = {enemy->position.x - drawW * 0.5f,
                         enemy->position.y - drawH * 0.88f, drawW, drawH};
        Color tint = enemy->state == ENEMY_HURT ? (Color){255, 165, 165, 255}
            : (enemy->state == ENEMY_DIE ? Fade(WHITE, fmaxf(0.0f, 1.0f - enemy->stateTime / 0.45f)) : WHITE);
        DrawTexturePro(vtex, src, dst, (Vector2){0}, 0.0f, tint);
    }
}
