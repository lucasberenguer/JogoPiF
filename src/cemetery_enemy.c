#include "enemy.h"
#include "raymath.h"
#include <math.h>

#define COVEIRO_WINDUP .8f

static bool HitPlayer(EnemyGroup *group, bool dodging, int *health, int damage) {
    if (dodging || group->playerHitCooldown > 0 || *health <= 0) return false;
    *health = (int)fmaxf(0, *health - damage);
    group->playerHitCooldown = .65f;
    return true;
}

static void ThrowShovel(EnemyGroup *group, const Enemy *boss, Vector2 direction) {
    for (int i = 0; i < CEMETERY_PROJECTILE_COUNT; i++) {
        CemeteryProjectile *shot = &group->cemeteryProjectiles[i];
        if (shot->active) continue;
        *shot = (CemeteryProjectile){true, boss->position, Vector2Scale(direction, 345), 2,
                                     (int)(26 * boss->power)};
        return;
    }
}

void CemeteryUpdateProjectiles(EnemyGroup *group, const Clearing *map, Vector2 player,
                               float dt, bool dodging, int *health) {
    for (int i = 0; i < CEMETERY_PROJECTILE_COUNT; i++) {
        CemeteryProjectile *shot = &group->cemeteryProjectiles[i];
        if (!shot->active) continue;
        shot->life -= dt;
        Vector2 delta = Vector2Scale(shot->velocity, dt);
        int steps = (int)(Vector2Length(delta)/4)+1;
        for (int s = 0; s < steps && shot->active; s++) {
            shot->position = Vector2Add(shot->position, Vector2Scale(delta, 1.0f/steps));
            if (IsClearingBlocked(map, shot->position) || shot->life <= 0) shot->active = false;
            else if (Vector2Distance(shot->position, player) < 29) {
                HitPlayer(group, dodging, health, shot->damage);
                shot->active = false;
            }
        }
    }
}

void CemeteryUpdateBoss(EnemyGroup *group, const Clearing *map, Enemy *boss, Vector2 player,
                        float dt, bool dodging, int *health) {
    bool phaseTwo = boss->guardianPhase == GUARDIAN_PHASE_TWO;
    if (boss->state == ENEMY_ATTACK) {
        float t = boss->stateTime;
        if (t >= COVEIRO_WINDUP && !boss->attackConnected) {
            if (boss->bossAttack == COVEIRO_SHOVEL) {
                boss->attackConnected = true;
                ThrowShovel(group, boss, boss->attackDirection);
                if (phaseTwo) {
                    ThrowShovel(group, boss, Vector2Rotate(boss->attackDirection, .25f));
                    ThrowShovel(group, boss, Vector2Rotate(boss->attackDirection, -.25f));
                }
            } else if (boss->bossAttack == COVEIRO_ROOTS && t < 1.2f &&
                       Vector2Distance(player, boss->attackTarget) < (phaseTwo ? 115 : 95)) {
                boss->attackConnected = HitPlayer(group, dodging, health, (int)(28 * boss->power));
            }
        }
        if (boss->bossAttack == COVEIRO_CHARGE && t >= COVEIRO_WINDUP && t < 1.35f) {
            MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, (phaseTwo ? 590 : 465) * dt));
            if (!boss->attackConnected && Vector2Distance(player, boss->position) < 58 &&
                MapHasClearPath(map, boss->position, player))
                boss->attackConnected = HitPlayer(group, dodging, health, (int)(24 * boss->power));
        }
        if (t >= 2.0f) { boss->state = ENEMY_IDLE; boss->stateTime = 0; }
        return;
    }
    if (*health <= 0) return;
    Vector2 offset = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(offset);
    boss->facing = fabsf(offset.x) > fabsf(offset.y) ? (offset.x < 0 ? 2 : 3) : (offset.y < 0 ? 1 : 0);
    if (distance <= 560 && boss->attackCooldown <= 0) {
        boss->bossAttack = (boss->bossAttack + 1) % COVEIRO_ATTACK_COUNT;
        boss->state = ENEMY_ATTACK;
        boss->stateTime = 0;
        boss->attackConnected = false;
        boss->attackTarget = player;
        boss->attackDirection = distance > .01f ? Vector2Normalize(offset) : (Vector2){0, 1};
        boss->attackCooldown = phaseTwo ? 2.15f : 2.85f;
    } else if (distance > 150) {
        boss->state = ENEMY_WALK;
        MoveEnemy(map, boss, Vector2Scale(Vector2Normalize(offset), (phaseTwo ? 150 : 100) * dt));
    } else boss->state = ENEMY_IDLE;
}

void CemeteryDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE) {
            if (enemy->pickupAvailable) {
                Vector2 drop = {enemy->position.x, enemy->position.y - 9};
                DrawCircleV(drop, 9, Fade(GOLD, .65f));
                DrawCircleV(drop, 6, RED);
            }
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            if (enemy->stateTime >= 0)
                DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                24 + 10 * sinf(enemy->stateTime*8), GOLD);
            continue;
        }

        bool boss = enemy->type == ENEMY_BOSS;
        bool ranged = enemy->type == ENEMY_COMMON_RANGED;
        bool fallen = enemy->state == ENEMY_FALLEN;
        int cols = boss ? 9 : ranged ? 4 : 5;
        int rows = boss ? 4 : ranged ? 4 : 5;
        Texture2D tex = boss ? group->boss[enemy->bossAttack] : ranged ? group->mage : group->skeleton;
        int frame = boss ? (enemy->state == ENEMY_ATTACK ? (int)(enemy->stateTime/2.0f*cols) : 0) :
                    fallen ? cols-1 : EnemyAnimationFrame(enemy);
        if (frame >= cols) frame = cols-1;
        int row = boss ? enemy->facing : (enemy->state == ENEMY_DIE || fallen) ? rows-1 :
                  enemy->state == ENEMY_ATTACK ? 2 : enemy->state == ENEMY_WALK ? 1 : 0;
        float cw = (float)tex.width/cols, ch = (float)tex.height/rows;
        float height = boss ? 150 : 80;
        float width = height * cw/ch;
        Color tint = enemy->state == ENEMY_HURT ? RED : enemy->type == ENEMY_SPECIAL ? VIOLET : WHITE;
        if (boss && (fallen || enemy->state == ENEMY_DIE)) tint = Fade(tint, .45f);
        if (boss && enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
            float radius = 50 + 120 * enemy->stateTime;
            DrawCircleV(enemy->position, radius, Fade(VIOLET, .30f * (1 - enemy->stateTime)));
            DrawCircleLines((int)enemy->position.x, (int)enemy->position.y, radius, VIOLET);
        } else if (boss && enemy->guardianPhase == GUARDIAN_PHASE_TWO && enemy->health > 0) {
            DrawCircleV(enemy->position, 43, Fade(VIOLET, .26f));
        }
        DrawTexturePro(tex, (Rectangle){frame*cw, row*ch, cw, ch},
            (Rectangle){enemy->position.x-width/2, enemy->position.y-height, width, height},
            (Vector2){0}, 0, tint);

        if (fallen) DrawText("E: selar", (int)enemy->position.x-30, (int)enemy->position.y+10, 16, GOLD);
        if (enemy->type == ENEMY_SPECIAL && enemy->state == ENEMY_ATTACK && enemy->stateTime < .5f)
            DrawCircleLines((int)enemy->position.x, (int)enemy->position.y, 55, ORANGE);
        if (!boss && EnemyCanBeHit(enemy) && enemy->health < enemy->maxHealth) {
            int x = (int)enemy->position.x-24, y = (int)(enemy->position.y-height)-8;
            DrawRectangle(x, y, 48, 5, DARKGRAY);
            DrawRectangle(x, y, 48*enemy->health/enemy->maxHealth, 5, RED);
        }
        if (boss && enemy->state == ENEMY_ATTACK) {
            Color warning = enemy->stateTime < COVEIRO_WINDUP ? Fade(VIOLET, .45f) : VIOLET;
            bool phaseTwo = enemy->guardianPhase == GUARDIAN_PHASE_TWO;
            if (enemy->bossAttack == COVEIRO_ROOTS && enemy->stateTime < 1.2f)
                DrawCircleLines((int)enemy->attackTarget.x, (int)enemy->attackTarget.y, phaseTwo ? 115 : 95, warning);
            if (enemy->bossAttack == COVEIRO_CHARGE && enemy->stateTime < COVEIRO_WINDUP)
                DrawLineEx(enemy->position, Vector2Add(enemy->position,
                    Vector2Scale(enemy->attackDirection, phaseTwo ? 325 : 256)), 5, warning);
        }
    }
}

static void DrawEffect(Texture2D tex, Vector2 position, int frame, float size) {
    float w = (float)tex.width/4, h = (float)tex.height/2;
    DrawTexturePro(tex, (Rectangle){(frame%4)*w,(frame/4)*h,w,h},
        (Rectangle){position.x-size/2,position.y-size*.75f,size,size*.75f},
        (Vector2){0}, 0, WHITE);
}

void DrawEnemyProjectiles(const EnemyGroup *group) {
    if (!group->cemetery) return;
    const Enemy *boss = &group->list[0];
    if (boss->type == ENEMY_BOSS && boss->state == ENEMY_ATTACK && boss->stateTime >= COVEIRO_WINDUP) {
        int frame = (int)fminf((boss->stateTime-COVEIRO_WINDUP)/1.2f*8, 7);
        if (boss->bossAttack == COVEIRO_ROOTS)
            DrawEffect(group->effects[0], boss->attackTarget, frame, boss->guardianPhase == GUARDIAN_PHASE_TWO ? 230 : 190);
        if (boss->bossAttack == COVEIRO_CHARGE)
            DrawEffect(group->effects[2], boss->position, frame, 140);
    }
    for (int i = 0; i < CEMETERY_PROJECTILE_COUNT; i++) {
        const CemeteryProjectile *shot = &group->cemeteryProjectiles[i];
        if (shot->active) DrawEffect(group->effects[1], shot->position, (int)(GetTime()*16)%8, 84);
    }
}
