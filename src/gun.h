#ifndef GUN_H
#define GUN_H
#include "raylib.h"
#include "clearing.h"

#define GUN_MAX_SHOTS 48
#define GUN_MAX_IMPACTS 16
#define GUN_POSE_DURATION 0.32f
#define GUN_POSE_FRAMES 8
typedef struct GunShot {
    Vector2 position;
    Vector2 direction;
    float life;
    float delay;
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
} Gun;

bool InitGun(Gun *gun);
void UnloadGun(Gun *gun);
void ResetGun(Gun *gun);
void UpdateGun(Gun *gun, const Clearing *clearing, Vector2 player,
               Vector2 aimWorld, float deltaTime, bool triggerPressed);
void DrawGun(const Gun *gun);
int GunHitCircle(Gun *gun, Vector2 enemy, float enemyRadius);
#endif
