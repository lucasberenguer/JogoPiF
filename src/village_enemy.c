#include "enemy.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>

enum { VILLAGE_AXE_SLASH, VILLAGE_CHARGE, VILLAGE_SEAL_PULSE, VILLAGE_ATTACK_COUNT };
enum { VILLAGE_ACTIVE_LIMIT = 24 };

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
                                  Vector2 player, float playerPower) {
    float adjustment = 1.0f + fminf(0.15f, fmaxf(0.0f, playerPower - 1.0f) * 0.05f);
    int hp = (int)ceilf(45.0f * 1.36f * adjustment);
    *enemy = (Enemy){
        .type = ENEMY_COMMON_MELEE,
        .state = ENEMY_SPAWNING,
        .position = VillageHordeSpot(clearing, player),
        .health = hp,
        .maxHealth = hp,
        .speed = 112.0f,
        .power = 1.05f,
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
        group->list[i].speed = 104.0f + 4.0f * (float)(i % 5);
        group->list[i].attackCooldown = 0.35f + 0.08f * (float)(i % 6);
    }
    group->villageSpawnRemaining = 12;
    group->villageSpawnTimer = 0.65f;
    group->villageHordePower = power;
}

void VillageUpdateHorde(EnemyGroup *group, const Clearing *clearing,
                        Vector2 player, float dt) {
    if (!group->village) return;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        Enemy *a = &group->list[i];
        if (!EnemyCanBeHit(a) || a->type == ENEMY_BOSS) continue;
        for (int j = i + 1; j < ENEMY_COUNT; j++) {
            Enemy *b = &group->list[j];
            if (!EnemyCanBeHit(b) || b->type == ENEMY_BOSS) continue;
            Vector2 sep = Vector2Subtract(a->position, b->position);
            float distSq = Vector2LengthSqr(sep);
            if (distSq > 0.01f && distSq < 34.0f * 34.0f) {
                float dist = sqrtf(distSq);
                Vector2 push = Vector2Scale(sep, (34.0f - dist) / dist * 45.0f * dt);
                MoveEnemy(clearing, a, push);
                MoveEnemy(clearing, b, Vector2Negate(push));
            }
        }
    }
    if (group->villageSpawnRemaining <= 0) return;
    group->villageSpawnTimer -= dt;
    if (group->villageSpawnTimer > 0.0f) return;
    for (int i = 0; i < VILLAGE_ACTIVE_LIMIT; i++) {
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
        if (boss->attackCooldown <= 0.0f && distance < 430.0f) {
            int maxAtk = boss->guardianPhase == GUARDIAN_PHASE_TWO
                ? VILLAGE_ATTACK_COUNT : VILLAGE_SEAL_PULSE;
            if (distance > 155.0f && boss->guardianPhase == GUARDIAN_PHASE_ONE)
                boss->bossAttack = VILLAGE_CHARGE;
            else
                boss->bossAttack = (boss->bossAttack + 1) % maxAtk;
            boss->state = ENEMY_ATTACK;
            boss->stateTime = 0.0f;
            boss->attackConnected = false;
            boss->attackDirection = toward;
            return;
        }
        if (distance > 86.0f) {
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
        if (t >= 0.45f && t <= 0.62f) MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, 225.0f * dt));
        if (!boss->attackConnected && t >= 0.48f) {
            boss->attackConnected = true;
            if (distance <= 158.0f &&
                Vector2DotProduct(toward, boss->attackDirection) >= -0.25f) {
                HurtPlayer(group, playerHealth, dodging, (int)(20.0f * boss->power));
            }
        }
        if (t >= 0.88f) {
            boss->state = ENEMY_IDLE;
            boss->stateTime = 0.0f;
            boss->attackCooldown = boss->guardianPhase == GUARDIAN_PHASE_TWO ? 0.45f : 0.65f;
        }
    } else if (boss->bossAttack == VILLAGE_CHARGE) {
        if (t >= 0.55f && t < 1.15f) {
            MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, boss->speed * 3.0f * dt));
            if (!boss->attackConnected && Vector2Distance(boss->position, player) <= 62.0f) {
                boss->attackConnected = true;
                HurtPlayer(group, playerHealth, dodging, (int)(22.0f * boss->power));
            }
        }
        if (t >= 1.45f) {
            boss->state = ENEMY_IDLE;
            boss->stateTime = 0.0f;
            boss->attackCooldown = boss->guardianPhase == GUARDIAN_PHASE_TWO ? 0.55f : 0.75f;
        }
    } else {
        if (!boss->attackConnected && t >= 0.72f) {
            boss->attackConnected = true;
            if (distance <= 185.0f) {
                HurtPlayer(group, playerHealth, dodging, (int)(20.0f * boss->power));
            }
        }
        if (t >= 1.15f) {
            boss->state = ENEMY_IDLE;
            boss->stateTime = 0.0f;
            boss->attackCooldown = 0.60f;
        }
    }
}

static void DrawVillageBossDefeat(const EnemyGroup *group, const Enemy *enemy) {
    Texture2D tex = group->villageBossCharge;
    float cw = (float)tex.width / 6.0f, ch = (float)tex.height / 3.0f;
    float t = enemy->state == ENEMY_FALLEN ? 1.35f : enemy->stateTime;
    int weapon = group->villageBossDeathWeapon; /* 0: katana, 1: bible, 2: gun */
    float drawW = 104.0f, drawH = 104.0f * (ch / cw);
    float baseX = enemy->position.x, baseY = enemy->position.y;

    if (weapon == 1) {
        int frame = enemy->state == ENEMY_FALLEN ? 3 : (int)fminf(t / 0.85f * 4.0f, 3.0f);
        Rectangle src = {frame * cw, 2.0f * ch, cw, ch};
        Rectangle dst = {baseX - drawW * 0.5f, baseY - drawH * 0.88f, drawW, drawH};
        DrawTexturePro(tex, src, dst, (Vector2){0}, 0.0f, WHITE);
        return;
    }

    int frame = enemy->state == ENEMY_FALLEN ? 5 : (int)fminf(t / 1.35f * 6.0f, 5.0f);
    if (weapon == 0 || weapon == 2) {
        float pool = fminf(1.0f, t / 0.95f);
        Color darkBlood = (Color){92, 8, 12, 225};
        Color brightBlood = (Color){156, 16, 22, 215};
        DrawEllipse((int)baseX, (int)baseY - 4, 18.0f + 38.0f * pool, 9.0f + 16.0f * pool, darkBlood);
        DrawEllipse((int)(baseX - 8.0f), (int)baseY - 2, 10.0f + 22.0f * pool, 5.0f + 9.0f * pool, brightBlood);
        if (weapon == 0 && t >= 0.35f) {
            float hp = fminf(1.0f, (t - 0.35f) / 0.55f);
            DrawEllipse((int)(baseX + 48.0f), (int)(baseY + 4.0f), 8.0f + 10.0f * hp, 4.0f + 5.0f * hp, darkBlood);
        }
    }

    bool headless = (weapon == 0 && t >= 0.06f) || (weapon == 2 && t >= 0.02f);
    static const float headCut[6] = {0.28f, 0.30f, 0.36f, 0.44f, 0.52f, 0.56f};
    float cut = headless ? headCut[frame] : 0.0f;
    Rectangle src = {frame * cw, (2.0f + cut) * ch, cw, ch * (1.0f - cut)};
    Rectangle dst = {baseX - drawW * 0.5f, baseY - drawH * 0.88f + drawH * cut,
                     drawW, drawH * (1.0f - cut)};
    DrawTexturePro(tex, src, dst, (Vector2){0}, 0.0f, WHITE);
    if (headless && frame <= 3) {
        DrawEllipse((int)baseX, (int)(dst.y + 3.0f), 8.0f, 4.0f, (Color){135, 12, 18, 235});
    }

    if (weapon == 0) {
        if (t >= 0.06f && t < 0.85f) {
            float sp = (t - 0.06f) / 0.79f;
            for (int k = 0; k < 7; k++) {
                float a = -2.1f + (float)k * 0.28f;
                Vector2 p = {baseX + cosf(a) * (10.0f + 40.0f * sp),
                             dst.y + 4.0f + sinf(a) * (22.0f * sp) + 52.0f * sp * sp};
                DrawCircleV(p, 3.8f * (1.0f - sp * 0.55f), (Color){178, 16, 22, 240});
            }
        }
        if (t >= 0.25f) {
            Vector2 headPos = {baseX + 34.0f, baseY + 4.0f};
            Texture2D wtex = group->villageBossWalk;
            float wcw = (float)wtex.width / 6.0f, wch = (float)wtex.height / 4.0f;
            Rectangle headSrc = {wcw * 0.28f, wch * 0.04f, wcw * 0.44f, wch * 0.24f};
            float headW = drawW * 0.40f, headH = (drawW * (wch / wcw)) * 0.22f;
            Rectangle headDst = {headPos.x, headPos.y, headW, headH};
            DrawEllipse((int)headPos.x, (int)(headPos.y + 3.0f), 11.0f, 5.0f, (Color){115, 10, 14, 225});
            DrawTexturePro(wtex, headSrc, headDst, (Vector2){headW * 0.5f, headH * 0.5f}, 72.0f, WHITE);
        }
    } else if (weapon == 2 && t < 0.72f) {
        float ep = t / 0.72f;
        if (t < 0.16f) DrawCircleV((Vector2){baseX, baseY - 68.0f}, 24.0f * (1.0f - t / 0.16f), Fade(RED, 0.75f));
        for (int k = 0; k < 14; k++) {
            float ang = (float)k * (2.0f * PI / 14.0f) + 0.2f;
            float spd = 48.0f + (float)((k * 17) % 45);
            Vector2 p = {baseX + cosf(ang) * spd * ep,
                         baseY - 68.0f + sinf(ang) * spd * ep * 0.75f + 80.0f * ep * ep};
            Color c = (k % 4 == 0) ? (Color){225, 205, 185, 240} : (Color){165, 14, 20, 240};
            DrawCircleV(p, (k % 3 == 0 ? 4.2f : 2.8f) * (1.0f - ep * 0.5f), c);
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
                if (enemy->bossAttack == VILLAGE_CHARGE && enemy->stateTime < 0.55f) {
                    Vector2 end = Vector2Add(enemy->position,
                        Vector2Scale(enemy->attackDirection, 280.0f));
                    DrawLineEx(enemy->position, end, 4.0f, Fade(MAROON, 0.55f));
                } else if (enemy->bossAttack == VILLAGE_SEAL_PULSE && enemy->stateTime < 0.72f) {
                    float p = enemy->stateTime / 0.72f;
                    DrawCircleV(enemy->position, 185.0f, Fade(GOLD, 0.14f + 0.12f * p));
                    DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                    185.0f, Fade(GOLD, 0.75f));
                }
            }
            Texture2D tex = group->villageBossWalk;
            int rows = 4, row = enemy->facing, frame = 0;
            bool flip = false;
            if (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN) {
                DrawVillageBossDefeat(group, enemy);
                continue;
            } else if (enemy->state == ENEMY_ATTACK && enemy->bossAttack == VILLAGE_CHARGE) {
                tex = group->villageBossCharge;
                rows = 3;
                row = (enemy->facing == 0 || enemy->facing == 1) ? 1 : 0;
                flip = (enemy->facing == 2);
                frame = enemy->stateTime < 0.55f ? 0
                    : (enemy->stateTime < 1.15f ? 1 + (int)(enemy->stateTime * 10.0f) % 3
                                                : 4 + (int)fminf((enemy->stateTime - 1.15f) * 4.0f, 1.0f));
            } else if (enemy->state == ENEMY_ATTACK) {
                tex = group->villageBossAttack;
                row = (enemy->facing == 2) ? 2 : enemy->facing;
                if (enemy->facing == 2) { row = 2; flip = true; }
                frame = enemy->stateTime < 0.48f
                    ? (int)fminf(enemy->stateTime / 0.48f * 2.0f, 1.0f)
                    : (enemy->stateTime < 0.70f ? 2 + (int)((enemy->stateTime - 0.48f) / 0.22f * 2.0f)
                                                : 4 + (int)fminf((enemy->stateTime - 0.70f) / 0.18f * 2.0f, 1.0f));
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
        float drawW = 78.0f, drawH = 78.0f * (ch / cw);
        Rectangle dst = {enemy->position.x - drawW * 0.5f,
                         enemy->position.y - drawH * 0.88f, drawW, drawH};
        Color tint = enemy->state == ENEMY_HURT ? (Color){255, 165, 165, 255}
            : (enemy->state == ENEMY_DIE ? Fade(WHITE, fmaxf(0.0f, 1.0f - enemy->stateTime / 0.45f)) : WHITE);
        DrawTexturePro(vtex, src, dst, (Vector2){0}, 0.0f, tint);
        if (EnemyCanBeHit(enemy) && enemy->health < enemy->maxHealth) {
            int healthY = (int)enemy->position.y - 85;
            DrawRectangle((int)enemy->position.x - 24, healthY, 48, 5, DARKGRAY);
            DrawRectangle((int)enemy->position.x - 24, healthY,
                          48 * enemy->health / enemy->maxHealth, 5, RED);
        }
    }
}
