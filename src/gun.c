#include "gun.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define SHOT_SPEED GUN_BASE_SHOT_SPEED
#define SHOT_LIFE 0.85f
#define SHOT_RADIUS 8.0f
#define SHOT_DAMAGE WeaponBaseDamage(0)
#define SHOT_INTERVAL GUN_BASE_SHOT_INTERVAL

static void AddImpact(Gun *gun, Vector2 at) {
    for (int i=0; i<GUN_MAX_IMPACTS; i++) {
        if (gun->impacts[i].remaining <= 0.0f) {
            gun->impacts[i] = (GunImpact){at, 0.18f};
            return;
        }
    }
}

static Texture2D LoadGunBody(const char *path, GunBodyAtlas *fit, int columns, int rows) {
    Image image = LoadImage(path);
    Texture2D texture = {0};
    if (image.data && image.width == columns*64 && image.height == rows*80) {
        Color *pixels = LoadImageColors(image);
        if (pixels) {
            for (int row = 0; row < rows; row++) {
                for (int col = 0; col < columns; col++) {
                    int top = 80, bottom = 0, left = 64, right = 0;
                    for (int y = 0; y < 80; y++) {
                        const Color *line = pixels + (row*80+y)*image.width+col*64;
                        for (int x = 10; x < 54; x++) {
                            if (line[x].a < 144) continue;
                            // Measure the hair above the barrel and the body around its center.
                            if (y >= 12 && y < 25) {
                                if (x < left) left = x;
                                if (x+1 > right) right = x+1;
                            }
                            if (x < 18 || x >= 47) continue;
                            if (y < top) top = y;
                            if (y+1 > bottom) bottom = y+1;
                        }
                    }
                    if (right > left && bottom > top)
                        fit->frames[row][col] = (GunBodyFrame){right-left,top,bottom};
                }
            }
            UnloadImageColors(pixels);
            texture = LoadTextureFromImage(image);
        }
    }
    if (image.data) UnloadImage(image);
    return texture;
}

bool InitGun(Gun *gun) {
    *gun = (Gun){0};
    gun->projectile = LoadTexture("assets/arma_fogo_projetil.png");
    gun->impact = LoadTexture("assets/arma_fogo_impacto.png");
    gun->body = LoadGunBody("assets/protagonista/fluido/arma_fogo_disparo.png",&gun->shotFit,8,4);
    gun->idleBody = LoadGunBody("assets/protagonista/fluido/arma_fogo_idle_novo.png",&gun->idleFit,4,12);
    gun->walkBody = LoadGunBody("assets/protagonista/fluido/arma_fogo_caminhada.png",&gun->walkFit,GUN_WALK_FRAMES,12);
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

Rectangle GunBodyDestination(const Gun *gun, Rectangle source, Vector2 player,
                             int facing, bool walking, bool firing) {
    int idleRow = facing < 2 ? facing : 5-facing;
    const GunBodyFrame reference = gun->idleFit.frames[idleRow][0];
    const GunBodyAtlas *fit = firing ? &gun->shotFit : walking ? &gun->walkFit : &gun->idleFit;
    GunBodyFrame frame = fit->frames[(int)(source.y/80)][(int)(source.x/64)];
    float width = facing < 2 ? 68 : 76;
    const float baseScaleY = 75.0f/80;
    if (!reference.headWidth || !frame.headWidth)
        return (Rectangle){player.x-width*.5f,player.y-72,width,75};
    width *= (float)reference.headWidth/frame.headWidth;
    float scaleY = baseScaleY*(reference.bottom-reference.top)/(frame.bottom-frame.top);
    float ground = player.y-72+reference.bottom*baseScaleY;
    return (Rectangle){player.x-width*.5f,ground-frame.bottom*scaleY,width,80*scaleY};
}

float GunAdvanceWalkPose(float pose, Vector2 traveled, Vector2 facing) {
    float distance = Vector2Length(traveled);
    float stride = distance * (10.0f / PLAYER_BASE_SPEED);
    if (Vector2DotProduct(traveled, facing) < -.35f * distance) stride = -stride;
    pose = fmodf(pose + stride, GUN_WALK_FRAMES);
    return pose < 0.0f ? pose + GUN_WALK_FRAMES : pose;
}

void UnloadGun(Gun *gun) {
    if (gun->projectile.id) UnloadTexture(gun->projectile);
    if (gun->impact.id) UnloadTexture(gun->impact);
    if (gun->body.id) UnloadTexture(gun->body);
    if (gun->idleBody.id) UnloadTexture(gun->idleBody);
    if (gun->walkBody.id) UnloadTexture(gun->walkBody);
    *gun = (Gun){0};
}

static Vector2 RotateDir(Vector2 dir, float radians) {
    float c = cosf(radians), s = sinf(radians);
    return (Vector2){dir.x * c - dir.y * s, dir.x * s + dir.y * c};
}

static void SpawnGunShot(Gun *gun, Vector2 pos, Vector2 dir, float delay,
                         int maxHits, bool spawned[GUN_MAX_SHOTS]) {
    for (int i = 0; i < GUN_MAX_SHOTS; i++) {
        if (gun->shots[i].life <= 0.0f) {
            gun->shots[i] = (GunShot){pos, dir, SHOT_LIFE, delay, maxHits, -1};
            spawned[i] = true;
            return;
        }
    }
}

static void SpawnSpreadShots(Gun *gun, Vector2 pos, Vector2 aim, int level,
                             float delay, bool spawned[GUN_MAX_SHOTS]) {
    int maxHits = level >= 4 ? GUN_L4_MAX_HITS : GUN_BASE_MAX_HITS;
    if (level < 3) {
        SpawnGunShot(gun, pos, aim, delay, maxHits, spawned);
    } else {
        SpawnGunShot(gun, pos, RotateDir(aim, -GUN_L3_SPREAD_DEG * DEG2RAD), delay, maxHits, spawned);
        SpawnGunShot(gun, pos, RotateDir(aim, GUN_L3_SPREAD_DEG * DEG2RAD), delay, maxHits, spawned);
    }
}

void ResetGun(Gun *gun) {
    memset(gun->shots, 0, sizeof(gun->shots));
    memset(gun->impacts, 0, sizeof(gun->impacts));
    gun->poseRemaining = 0.0f;
    gun->shotCooldown = 0.0f;
    gun->aim = (Vector2){1.0f, 0.0f};
    gun->level = 0;
}

void UpdateGun(Gun *gun, const Clearing *clearing, Vector2 player,
               Vector2 aimWorld, float dt, bool triggerPressed, int level) {
    gun->level = level;
    gun->poseRemaining = fmaxf(0.0f, gun->poseRemaining-dt);
    gun->shotCooldown = fmaxf(0.0f, gun->shotCooldown-dt);
    for (int i=0; i<GUN_MAX_IMPACTS; i++)
        gun->impacts[i].remaining = fmaxf(0.0f, gun->impacts[i].remaining-dt);
    Vector2 origin = {player.x, player.y-35.0f};
    Vector2 delta = Vector2Subtract(aimWorld, origin);
    if (Vector2LengthSqr(delta) > 4.0f) gun->aim = Vector2Normalize(delta);
    gun->muzzle = Vector2Add(origin, Vector2Scale(gun->aim, 32.0f));

    bool spawned[GUN_MAX_SHOTS] = {false};
    float interval = level >= 1 ? GUN_L1_SHOT_INTERVAL : SHOT_INTERVAL;
    float speed = level >= 1 ? GUN_L1_SHOT_SPEED : SHOT_SPEED;
    if (triggerPressed && gun->shotCooldown <= 0.0f) {
        float delay = GUN_POSE_DURATION * 2.0f / GUN_POSE_FRAMES;
        if (level >= 5) {
            Vector2 perp = {-gun->aim.y, gun->aim.x};
            Vector2 muzzleA = Vector2Add(gun->muzzle, Vector2Scale(perp, GUN_L5_DUAL_OFFSET));
            Vector2 muzzleB = Vector2Subtract(gun->muzzle, Vector2Scale(perp, GUN_L5_DUAL_OFFSET));
            SpawnSpreadShots(gun, muzzleA, gun->aim, level, delay, spawned);
            SpawnSpreadShots(gun, muzzleB, gun->aim, level, delay, spawned);
        } else {
            SpawnSpreadShots(gun, gun->muzzle, gun->aim, level, delay, spawned);
        }
        gun->poseRemaining = GUN_POSE_DURATION;
        gun->shotCooldown = interval;
    }
    for (int i=0; i<GUN_MAX_SHOTS; i++) {
        GunShot *shot = &gun->shots[i];
        if (shot->life <= 0.0f) continue;
        float activeDt = spawned[i] ? 0.0f : dt;
        if (shot->delay > 0.0f) {
            float anticipation = fminf(activeDt, shot->delay);
            shot->delay -= anticipation;
            activeDt -= anticipation;
            if (shot->delay > 0.0f || activeDt <= 0.0f) continue;
        }
        activeDt = fminf(activeDt, shot->life);
        Vector2 travel = Vector2Scale(shot->direction, speed*activeDt);
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

int GunHitEnemyCircle(Gun *gun, int enemyIndex, Vector2 enemy, float enemyRadius) {
    float limit = SHOT_RADIUS + enemyRadius;
    int hits = 0;
    for (int i=0; i<GUN_MAX_SHOTS; i++) {
        GunShot *shot = &gun->shots[i];
        if (shot->life <= 0.0f || shot->delay > 0.0f) continue;
        if (enemyIndex >= 0 && shot->firstHitEnemy == enemyIndex) continue;
        if (Vector2DistanceSqr(shot->position, enemy) <= limit*limit) {
            AddImpact(gun, shot->position);
            if (shot->hitsRemaining <= 1) {
                shot->life = 0.0f;
                shot->hitsRemaining = 0;
            } else {
                shot->hitsRemaining--;
                shot->firstHitEnemy = enemyIndex;
            }
            hits++;
        }
    }
    return hits * SHOT_DAMAGE;
}

int GunHitCircle(Gun *gun, Vector2 enemy, float enemyRadius) {
    return GunHitEnemyCircle(gun, -1, enemy, enemyRadius);
}
