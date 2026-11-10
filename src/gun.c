#include "gun.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define SHOT_SPEED 760.0f
#define SHOT_LIFE 0.85f
#define SHOT_RADIUS 8.0f
#define SHOT_DAMAGE 5
#define SHOT_INTERVAL 0.20f

static void AddImpact(Gun *gun, Vector2 at) {
    for (int i=0; i<GUN_MAX_IMPACTS; i++) {
        if (gun->impacts[i].remaining <= 0.0f) {
            gun->impacts[i] = (GunImpact){at, 0.18f};
            return;
        }
    }
}

bool InitGun(Gun *gun) {
    *gun = (Gun){0};
    gun->projectile = LoadTexture("assets/arma_fogo_projetil.png");
    gun->impact = LoadTexture("assets/arma_fogo_impacto.png");
    gun->body = LoadTexture("assets/arma_fogo_disparo.png");
    gun->idleBody = LoadTexture("assets/arma_fogo_idle_novo.png");
    gun->walkBody = LoadTexture("assets/arma_fogo_caminhada.png");
    if (!gun->projectile.id || !gun->impact.id ||
        !gun->body.id || !gun->idleBody.id || !gun->walkBody.id) {
        UnloadGun(gun);
        return false;
    }
    SetTextureFilter(gun->projectile, TEXTURE_FILTER_POINT);
    SetTextureFilter(gun->impact, TEXTURE_FILTER_POINT);
    SetTextureFilter(gun->body, TEXTURE_FILTER_POINT);
    SetTextureFilter(gun->idleBody, TEXTURE_FILTER_POINT);
    SetTextureFilter(gun->walkBody, TEXTURE_FILTER_POINT);
    return true;
}

void UnloadGun(Gun *gun) {
    if (gun->projectile.id) UnloadTexture(gun->projectile);
    if (gun->impact.id) UnloadTexture(gun->impact);
    if (gun->body.id) UnloadTexture(gun->body);
    if (gun->idleBody.id) UnloadTexture(gun->idleBody);
    if (gun->walkBody.id) UnloadTexture(gun->walkBody);
    *gun = (Gun){0};
}

void ResetGun(Gun *gun) {
    memset(gun->shots, 0, sizeof(gun->shots));
    memset(gun->impacts, 0, sizeof(gun->impacts));
    gun->poseRemaining = 0.0f;
    gun->shotCooldown = 0.0f;
    gun->aim = (Vector2){1.0f, 0.0f};
}

void UpdateGun(Gun *gun, const Clearing *clearing, Vector2 player,
               Vector2 aimWorld, float dt, bool triggerPressed) {
    gun->poseRemaining = fmaxf(0.0f, gun->poseRemaining-dt);
    gun->shotCooldown = fmaxf(0.0f, gun->shotCooldown-dt);
    for (int i=0; i<GUN_MAX_IMPACTS; i++)
        gun->impacts[i].remaining = fmaxf(0.0f, gun->impacts[i].remaining-dt);
    Vector2 origin = {player.x, player.y-35.0f};
    Vector2 delta = Vector2Subtract(aimWorld, origin);
    if (Vector2LengthSqr(delta) > 4.0f) gun->aim = Vector2Normalize(delta);
    gun->muzzle = Vector2Add(origin, Vector2Scale(gun->aim, 32.0f));

    int newShot = -1;
    if (triggerPressed && gun->shotCooldown <= 0.0f) {
        for (int i=0; i<GUN_MAX_SHOTS; i++) {
            if (gun->shots[i].life <= 0.0f) {
                gun->shots[i] = (GunShot){gun->muzzle, gun->aim, SHOT_LIFE,
                                         GUN_POSE_DURATION * 2.0f / GUN_POSE_FRAMES};
                newShot = i;
                gun->poseRemaining = GUN_POSE_DURATION;
                gun->shotCooldown = SHOT_INTERVAL;
                break;
            }
        }
    }
    for (int i=0; i<GUN_MAX_SHOTS; i++) {
        GunShot *shot = &gun->shots[i];
        if (shot->life <= 0.0f) continue;
        float activeDt = i == newShot ? 0.0f : dt;
        if (shot->delay > 0.0f) {
            float anticipation = fminf(activeDt, shot->delay);
            shot->delay -= anticipation;
            activeDt -= anticipation;
            if (shot->delay > 0.0f || activeDt <= 0.0f) continue;
        }
        activeDt = fminf(activeDt, shot->life);
        Vector2 travel = Vector2Scale(shot->direction, SHOT_SPEED*activeDt);
        int steps = (int)(Vector2Length(travel)/7.0f)+1;
        Vector2 step = Vector2Scale(travel, 1.0f/steps);
        for (int j=0; j<steps; j++) {
            Vector2 next = Vector2Add(shot->position, step);
            if (IsClearingBlocked(clearing, next)) {
                AddImpact(gun, shot->position);
                shot->life = 0.0f;
                break;
            }
            shot->position = next;
        }
        if (shot->life > 0.0f) shot->life = fmaxf(0.0f, shot->life-activeDt);
    }
}

void DrawGun(const Gun *gun) {
    for (int i=0; i<GUN_MAX_SHOTS; i++) {
        const GunShot *shot = &gun->shots[i];
        if (shot->life <= 0.0f || shot->delay > 0.0f) continue;
        Vector2 tail = Vector2Subtract(shot->position, Vector2Scale(shot->direction, 18.0f));
        DrawLineEx(tail, shot->position, 2.0f, Fade(GOLD, 0.45f));
        float angle = atan2f(shot->direction.y, shot->direction.x)*RAD2DEG;
        DrawTexturePro(gun->projectile,
            (Rectangle){0,0,(float)gun->projectile.width,(float)gun->projectile.height},
            (Rectangle){shot->position.x,shot->position.y,38,20},
            (Vector2){19,10},angle,WHITE);
    }
    for (int i=0; i<GUN_MAX_IMPACTS; i++) {
        const GunImpact *impact = &gun->impacts[i];
        if (impact->remaining <= 0.0f) continue;
        DrawTexturePro(gun->impact,
            (Rectangle){0,0,(float)gun->impact.width,(float)gun->impact.height},
            (Rectangle){impact->position.x-24,impact->position.y-24,48,48},
            (Vector2){0},0,Fade(WHITE, impact->remaining/0.18f));
    }
}

int GunHitCircle(Gun *gun, Vector2 enemy, float enemyRadius) {
    float limit = SHOT_RADIUS + enemyRadius;
    for (int i=0; i<GUN_MAX_SHOTS; i++) {
        GunShot *shot = &gun->shots[i];
        if (shot->life <= 0.0f || shot->delay > 0.0f) continue;
        if (Vector2DistanceSqr(shot->position, enemy) <= limit*limit) {
            AddImpact(gun, shot->position);
            shot->life = 0.0f;
            return SHOT_DAMAGE;
        }
    }
    return 0;
}
