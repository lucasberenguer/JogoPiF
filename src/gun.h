#ifndef GUN_H
#define GUN_H
#include "raylib.h"
#include "clearing.h"
#include "run_progress.h"

#define GUN_MAX_SHOTS 96
#define GUN_MAX_IMPACTS 16
#define GUN_POSE_DURATION 0.32f
#define GUN_POSE_FRAMES 8
#define GUN_BASE_SHOT_INTERVAL 0.20f
#define GUN_L1_SHOT_INTERVAL 0.14f
#define GUN_BASE_SHOT_SPEED 760.0f
#define GUN_L1_SHOT_SPEED 860.0f
#define GUN_L3_SPREAD_DEG 5.0f
#define GUN_L5_DUAL_OFFSET 13.0f
#define GUN_BASE_MAX_HITS 1
#define GUN_L4_MAX_HITS 2

typedef struct GunShot {
    Vector2 position;
    Vector2 direction;
    float life;
    float delay;
    int hitsRemaining;
    int firstHitEnemy;
} GunShot;
typedef struct GunImpact {
    Vector2 position;
    float remaining;
} GunImpact;
typedef struct Gun {
    Texture2D projectile;
    Texture2D impact;
    Texture2D body;
    Texture2D idleBody;
    Texture2D walkBody;
    GunShot shots[GUN_MAX_SHOTS];
    GunImpact impacts[GUN_MAX_IMPACTS];
    Vector2 muzzle;
    Vector2 aim;
    float poseRemaining;
    float shotCooldown;
    int level;
} Gun;

bool InitGun(Gun *gun);
void UnloadGun(Gun *gun);
void ResetGun(Gun *gun);
void UpdateGun(Gun *gun, const Clearing *clearing, Vector2 player,
               Vector2 aimWorld, float deltaTime, bool triggerPressed, int level);
void DrawGun(const Gun *gun);
int GunHitEnemyCircle(Gun *gun, int enemyIndex, Vector2 enemy, float enemyRadius);
int GunHitCircle(Gun *gun, Vector2 enemy, float enemyRadius);
#endif
