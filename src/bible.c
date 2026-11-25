#include "game_audio.h"
#include "bible.h"
#include <math.h>
#include <string.h>

#define BIBLE_HIT_RADIUS 17.0f
#define BIBLE_POOL_RADIUS 62.0f
#define BIBLE_POOL_DURATION 3.0f
#define BIBLE_POOL_INTERVAL 3.5f
#define BIBLE_POOL_TICK 0.45f
#define BIBLE_PI 3.14159265358979323846f

static int BibleBookCount(int level) {
    if (level >= 4) return BIBLE_L4_BOOKS;
    if (level >= 2) return BIBLE_L2_BOOKS;
    return BIBLE_BASE_BOOKS;
}

static float BibleOrbitRadius(int level) {
    return level >= 2 ? BIBLE_L2_ORBIT_RADIUS : BIBLE_BASE_ORBIT_RADIUS;
}

static Vector2 BookCenter(const Bible *b, Vector2 player, int index) {
    int count = BibleBookCount(b->level);
    float radius = BibleOrbitRadius(b->level);
    float angle = b->angle + index * (2.0f * BIBLE_PI / count);
    return (Vector2){player.x + cosf(angle) * radius,
                     player.y - 34.0f + sinf(angle) * radius};
}

bool InitBible(Bible *b) {
    *b = (Bible){0};
    b->sprites = LoadTexture("assets/protagonista/fluido/biblia_orbita.png");
    b->activation = LoadTexture("assets/protagonista/fluido/biblia_ativacao_nova.png");
    b->idle = LoadTexture("assets/protagonista/fluido/biblia_idle.png");
    b->walk = LoadTexture("assets/protagonista/fluido/biblia_caminhada.png");
    if (!b->sprites.id || !b->activation.id || !b->idle.id || !b->walk.id) { UnloadBible(b); return false; }
    SetTextureFilter(b->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(b->activation, TEXTURE_FILTER_POINT);
    SetTextureFilter(b->idle, TEXTURE_FILTER_POINT);
    SetTextureFilter(b->walk, TEXTURE_FILTER_POINT);
    return true;
}

void UnloadBible(Bible *b) {
    if (b->sprites.id) UnloadTexture(b->sprites);
    if (b->activation.id) UnloadTexture(b->activation);
    if (b->idle.id) UnloadTexture(b->idle);
    if (b->walk.id) UnloadTexture(b->walk);
    *b = (Bible){0};
}

void ResetBible(Bible *b) {
    b->angle = 0.0f;
    b->animationTime = 0.0f;
    b->boostRemaining = 0.0f;
    b->boostCooldown = 0.0f;
    b->activationRemaining = 0.0f;
    b->level = 0;
    b->holyWaterCooldown = 0.6f;
    memset(b->pools, 0, sizeof(b->pools));
}

void UpdateBible(Bible *b, float dt, bool activate, int level,
                 bool inCombat, Vector2 player) {
    b->level = level;
    if (activate && b->boostCooldown <= 0.0f) {
        b->boostRemaining = level >= 3 ? BIBLE_L3_BOOST_DURATION : BIBLE_BASE_BOOST_DURATION;
        b->boostCooldown = level >= 3 ? BIBLE_L3_BOOST_COOLDOWN : BIBLE_BASE_BOOST_COOLDOWN;
        b->activationRemaining = BIBLE_ACTIVATION_DURATION;
    }
    bool boosted = b->boostRemaining > 0.0f;
    b->angle += (boosted ? 4.8f : 1.6f) * dt;
    if (b->angle >= 2.0f * BIBLE_PI) b->angle -= 2.0f * BIBLE_PI;
    b->animationTime += dt;
    if (b->boostRemaining > 0.0f) b->boostRemaining = fmaxf(0.0f, b->boostRemaining - dt);
    if (b->boostCooldown > 0.0f) b->boostCooldown = fmaxf(0.0f, b->boostCooldown - dt);
    if (b->activationRemaining > 0.0f)
        b->activationRemaining = fmaxf(0.0f, b->activationRemaining - dt);

    if (level >= 5 && inCombat) {
        b->holyWaterCooldown -= dt;
        if (b->holyWaterCooldown <= 0.0f) {
            int slot = 0;
            for (int i = 0; i < BIBLE_MAX_POOLS; i++) {
                if (b->pools[i].remaining <= 0.0f) { slot = i; break; }
                if (b->pools[i].remaining < b->pools[slot].remaining) slot = i;
            }
            b->pools[slot] = (HolyWaterPool){player, BIBLE_POOL_DURATION, 0.0f, false};
            EmitSfx(SFX_HOLY_WATER,player,1,1);
            b->holyWaterCooldown = BIBLE_POOL_INTERVAL;
        }
    }
    for (int i = 0; i < BIBLE_MAX_POOLS; i++) {
        HolyWaterPool *p = &b->pools[i];
        p->tickReady = false;
        if (p->remaining <= 0.0f) continue;
        p->remaining = fmaxf(0.0f, p->remaining - dt);
        p->tickTimer -= dt;
        if (p->tickTimer <= 0.0f) {
            p->tickReady = true;
            p->tickTimer = BIBLE_POOL_TICK;
        }
    }
}

void DrawBiblePools(const Bible *b) {
    for (int i = 0; i < BIBLE_MAX_POOLS; i++) {
        const HolyWaterPool *p = &b->pools[i];
        if (p->remaining <= 0.0f) continue;
        float alpha = fminf(1.0f, p->remaining / 0.6f);
        float pulse = 1.0f + 0.05f * sinf(b->animationTime * 6.0f + (float)i);
        float r = BIBLE_POOL_RADIUS * pulse;
        DrawCircleV(p->position, r, Fade((Color){60, 175, 255, 255}, 0.24f * alpha));
        DrawCircleLines((int)p->position.x, (int)p->position.y, r,
                        Fade((Color){155, 230, 255, 255}, 0.65f * alpha));
        DrawCircleLines((int)p->position.x, (int)p->position.y, r * 0.62f,
                        Fade(WHITE, 0.35f * alpha));
    }
}

void DrawBible(const Bible *b, Vector2 player, bool foreground) {
    Vector2 orbitCenter = {player.x, player.y - 34.0f};
    int count = BibleBookCount(b->level);
    float radius = BibleOrbitRadius(b->level);
    if (!foreground) {
        for (int i=0; i<72; i++) {
            float a = i * (2.0f * BIBLE_PI / 72.0f);
            float next = (i+1) * (2.0f * BIBLE_PI / 72.0f);
            Vector2 p = {orbitCenter.x + cosf(a)*radius,
                         orbitCenter.y + sinf(a)*radius};
            Vector2 q = {orbitCenter.x + cosf(next)*radius,
                         orbitCenter.y + sinf(next)*radius};
            DrawLineEx(p, q, 1.2f, Fade(GOLD, 0.18f));
        }
    }
    for (int i=0; i<count; i++) {
        Vector2 center = BookCenter(b, player, i);
        if ((center.y >= orbitCenter.y) != foreground) continue;
        float angle = b->angle + i * (2.0f * BIBLE_PI / count);
        for (int j=1; j<=3; j++) {
            float trailAngle = angle - j * 0.11f;
            Vector2 trail = {orbitCenter.x + cosf(trailAngle)*radius,
                             orbitCenter.y + sinf(trailAngle)*radius};
            DrawCircleV(trail, j==1 ? 2.8f : 1.6f,
                        Fade(GOLD, (b->boostRemaining>0.0f ? 0.55f : 0.32f)/j));
        }
        int frame = (int)(b->animationTime*27) % 24;
        frame = (frame + i*6) % 24;
        float size = 72.0f;
        DrawTexturePro(b->sprites, (Rectangle){(frame/3)*80.0f,(frame%3)*80.0f,80,80},
                       (Rectangle){center.x-size/2,center.y-size/2,size,size},
                       (Vector2){0},0,WHITE);
    }
}

void DrawBibleActivation(const Bible *b, Vector2 player) {
    if (b->activationRemaining <= 0.0f) return;
    float progress = 1.0f - b->activationRemaining / BIBLE_ACTIVATION_DURATION;
    Vector2 center = {player.x, player.y - 35.0f};
    float radius = 32.0f + 55.0f * progress;
    Color glow = Fade(GOLD, 0.75f * (1.0f - progress));
    DrawCircleLines((int)center.x, (int)center.y, radius, glow);
    DrawCircleLines((int)center.x, (int)center.y, radius + 4.0f,
                    Fade(WHITE, 0.42f * (1.0f - progress)));
    for (int i = 0; i < 8; i++) {
        float angle = i * (2.0f * BIBLE_PI / 8.0f) + b->angle;
        Vector2 point = {center.x + cosf(angle) * radius,
                         center.y + sinf(angle) * radius};
        DrawCircleV(point, 2.5f, glow);
    }
}

bool BibleTouchesCircle(const Bible *b, Vector2 player,
                        Vector2 enemy, float enemyRadius) {
    float limit = BIBLE_HIT_RADIUS + enemyRadius;
    int count = BibleBookCount(b->level);
    for (int i=0; i<count; i++) {
        Vector2 book = BookCenter(b, player, i);
        float dx = book.x - enemy.x, dy = book.y - enemy.y;
        if (dx*dx + dy*dy <= limit*limit) return true;
    }
    return false;
}

bool BibleBlocksCircle(const Bible *b, Vector2 player,
                       Vector2 point, float radius) {
    if (b->level < 4 || b->boostRemaining <= 0.0f) return false;
    return BibleTouchesCircle(b, player, point, radius + 8.0f);
}

bool BibleHolyWaterHits(const Bible *b, Vector2 enemy, float enemyRadius) {
    float limit = BIBLE_POOL_RADIUS + enemyRadius;
    for (int i = 0; i < BIBLE_MAX_POOLS; i++) {
        const HolyWaterPool *p = &b->pools[i];
        if (p->remaining <= 0.0f || !p->tickReady) continue;
        float dx = p->position.x - enemy.x, dy = p->position.y - enemy.y;
        if (dx*dx + dy*dy <= limit*limit) return true;
    }
    return false;
}

int BibleContactDamage(void) { return WeaponBaseDamage(1); }
