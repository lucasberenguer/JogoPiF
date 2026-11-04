#include "enemy.h"
#include "raymath.h"
#include <math.h>

#define ENEMY_MAX_HEALTH 60
#define ENEMY_SPEED 92.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12

bool InitEnemies(EnemyGroup *group) {
    *group = (EnemyGroup){0};
    group->sprites = LoadTexture("assets/inimigo_encapuzado.png");
    group->backSprites = LoadTexture("assets/inimigo_encapuzado_costas.png");
    group->frontSprites = LoadTexture("assets/inimigo_encapuzado_frente.png");
    if (!group->sprites.id || !group->backSprites.id || !group->frontSprites.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->backSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->frontSprites, TEXTURE_FILTER_POINT);
    return true;
}

void UnloadEnemies(EnemyGroup *group) {
    if (group->sprites.id) UnloadTexture(group->sprites);
    if (group->backSprites.id) UnloadTexture(group->backSprites);
    if (group->frontSprites.id) UnloadTexture(group->frontSprites);
    *group = (EnemyGroup){0};
}

void ResetEnemies(EnemyGroup *group, const Clearing *clearing) {
    group->playerHitCooldown = 0.0f;
    const Vector2 spawns[ENEMY_COUNT] = {
        {355, 330}, {1270, 340}, {440, 655}, {1280, 675}
    };
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        *enemy = (Enemy){0};
        enemy->position = spawns[i];
        if (IsClearingBlocked(clearing, enemy->position)) {
            for (int step=1; step<=12; step++) {
                Vector2 candidate = {spawns[i].x + (i%2 ? -1 : 1)*step*20.0f,
                                     spawns[i].y + step*8.0f};
                if (!IsClearingBlocked(clearing, candidate)) {
                    enemy->position = candidate;
                    break;
                }
            }
        }
        enemy->health = ENEMY_MAX_HEALTH;
        enemy->state = ENEMY_IDLE;
        enemy->facing = i%2 ? 2 : 3;
        enemy->lastKatanaSwing = -1;
        enemy->attackCooldown = 0.5f + i*0.22f;
    }
}

bool EnemyCanBeHit(const Enemy *enemy) {
    return enemy->state != ENEMY_DIE && enemy->state != ENEMY_GONE && enemy->health > 0;
}

Vector2 EnemyCenter(const Enemy *enemy) {
    return (Vector2){enemy->position.x, enemy->position.y-36.0f};
}

void DamageEnemy(Enemy *enemy, int damage) {
    if (!EnemyCanBeHit(enemy) || damage<=0) return;
    enemy->health -= damage;
    enemy->stateTime = 0.0f;
    if (enemy->health <= 0) {
        enemy->health = 0;
        enemy->state = ENEMY_DIE;
        enemy->pickupAvailable = true;
    } else {
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

void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float dt, bool playerDodging,
                   int *playerHealth, int playerMaxHealth) {
    group->playerHitCooldown = fmaxf(0.0f, group->playerHitCooldown-dt);
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        if (enemy->pickupAvailable && *playerHealth > 0 && *playerHealth < playerMaxHealth &&
            Vector2Distance(enemy->position, player) < 26.0f) {
            *playerHealth += 15;
            if (*playerHealth > playerMaxHealth) *playerHealth = playerMaxHealth;
            enemy->pickupAvailable = false;
        }
        if (enemy->state == ENEMY_GONE) continue;
        enemy->stateTime += dt;
        enemy->attackCooldown = fmaxf(0.0f, enemy->attackCooldown-dt);
        enemy->bibleHitCooldown = fmaxf(0.0f, enemy->bibleHitCooldown-dt);
        if (enemy->state == ENEMY_DIE) {
            if (enemy->stateTime >= 0.85f) enemy->state = ENEMY_GONE;
            continue;
        }
        if (enemy->state == ENEMY_HURT) {
            if (enemy->stateTime >= 0.32f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (enemy->state == ENEMY_ATTACK) {
            if (!enemy->attackConnected && enemy->stateTime >= 0.32f) {
                enemy->attackConnected = true;
                if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0 &&
                    Vector2Distance(enemy->position, player) <= ENEMY_ATTACK_RANGE+8.0f) {
                    *playerHealth -= ENEMY_ATTACK_DAMAGE;
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
        if (distance <= ENEMY_ATTACK_RANGE && enemy->attackCooldown <= 0.0f) {
            enemy->state = ENEMY_ATTACK;
            enemy->stateTime = 0.0f;
            enemy->attackConnected = false;
            enemy->attackCooldown = 1.35f;
        } else if (distance > ENEMY_ATTACK_RANGE*0.85f) {
            enemy->state = ENEMY_WALK;
            enemy->walkTime += dt;
            MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), ENEMY_SPEED*dt));
        } else {
            enemy->state = ENEMY_IDLE;
        }
    }
}

void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->pickupAvailable) {
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 9.0f,
                        Fade(GOLD, 0.65f));
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 6.0f, RED);
        }
        if (enemy->state == ENEMY_GONE) continue;
        int row = enemy->state == ENEMY_WALK ? 1 : enemy->state == ENEMY_ATTACK ? 2 :
                  enemy->state == ENEMY_HURT ? 3 : enemy->state == ENEMY_DIE ? 4 : 0;
        int count = row >= 2 && row != 3 ? 5 : 4;
        float speed = row == 4 ? 5.9f : row == 2 ? 8.0f : row == 3 ? 12.0f : 7.5f;
        int frame = (int)((row==1 ? enemy->walkTime : enemy->stateTime)*speed)%count;
        Texture2D tex = group->sprites;
        if (enemy->facing==1 && row<=1) tex = group->backSprites;
        if (enemy->facing==0 && row<=1) tex = group->frontSprites;
        Rectangle src = {frame*80.0f, row*80.0f,
                         enemy->facing==2 && tex.id==group->sprites.id ? -80.0f : 80.0f, 80.0f};
        Color tint = enemy->state == ENEMY_HURT ? (Color){255,160,160,255} : WHITE;
        DrawTexturePro(tex, src,
            (Rectangle){enemy->position.x-38, enemy->position.y-75, 76, 76},
            (Vector2){0},0,tint);
        if (EnemyCanBeHit(enemy) && enemy->health < ENEMY_MAX_HEALTH) {
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,48,5,DARKGRAY);
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,
                          48*enemy->health/ENEMY_MAX_HEALTH,5,RED);
        }
    }
}
