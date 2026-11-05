#include "bible.h"
#include <math.h>

#define BIBLE_COUNT 4
#define BIBLE_RADIUS 90.0f
#define BIBLE_HIT_RADIUS 17.0f
#define BIBLE_PI 3.14159265358979323846f

static Vector2 BookCenter(const Bible *b, Vector2 player, int index) {
    float angle = b->angle + index * (2.0f * BIBLE_PI / BIBLE_COUNT);
    return (Vector2){player.x + cosf(angle) * BIBLE_RADIUS,
                     player.y - 34.0f + sinf(angle) * BIBLE_RADIUS};
}

bool InitBible(Bible *b) {
    *b = (Bible){0};
    b->sprites = LoadTexture("assets/biblia_orbita.png");
    b->activation = LoadTexture("assets/biblia_ativacao_nova.png");
    b->idle = LoadTexture("assets/biblia_idle.png");
    b->walk = LoadTexture("assets/biblia_caminhada.png");
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
}

void UpdateBible(Bible *b, float dt, bool activate) {
    if (activate && b->boostCooldown <= 0.0f) {
        b->boostRemaining = 1.25f;
        b->boostCooldown = 5.0f;
        b->activationRemaining = BIBLE_ACTIVATION_DURATION;
    }
    bool boosted = b->boostRemaining > 0.0f;
    b->angle += (boosted ? 4.2f : 1.35f) * dt;
    if (b->angle >= 2.0f * BIBLE_PI) b->angle -= 2.0f * BIBLE_PI;
    b->animationTime += dt;
    if (b->boostRemaining > 0.0f) b->boostRemaining = fmaxf(0.0f, b->boostRemaining - dt);
    if (b->boostCooldown > 0.0f) b->boostCooldown = fmaxf(0.0f, b->boostCooldown - dt);
    if (b->activationRemaining > 0.0f)
        b->activationRemaining = fmaxf(0.0f, b->activationRemaining - dt);
}

void DrawBible(const Bible *b, Vector2 player, bool foreground) {
    Vector2 orbitCenter = {player.x, player.y - 34.0f};
    if (!foreground) {
        for (int i=0; i<72; i++) {
            float a = i * (2.0f * BIBLE_PI / 72.0f);
            float next = (i+1) * (2.0f * BIBLE_PI / 72.0f);
            Vector2 p = {orbitCenter.x + cosf(a)*BIBLE_RADIUS,
                         orbitCenter.y + sinf(a)*BIBLE_RADIUS};
            Vector2 q = {orbitCenter.x + cosf(next)*BIBLE_RADIUS,
                         orbitCenter.y + sinf(next)*BIBLE_RADIUS};
            DrawLineEx(p, q, 1.2f, Fade(GOLD, 0.18f));
        }
    }
    for (int i=0; i<BIBLE_COUNT; i++) {
        Vector2 center = BookCenter(b, player, i);
        if ((center.y >= orbitCenter.y) != foreground) continue;
        float angle = b->angle + i * (2.0f * BIBLE_PI / BIBLE_COUNT);
        for (int j=1; j<=3; j++) {
            float trailAngle = angle - j * 0.11f;
            Vector2 trail = {orbitCenter.x + cosf(trailAngle)*BIBLE_RADIUS,
                             orbitCenter.y + sinf(trailAngle)*BIBLE_RADIUS};
            DrawCircleV(trail, j==1 ? 2.8f : 1.6f,
                        Fade(GOLD, (b->boostRemaining>0.0f ? 0.55f : 0.32f)/j));
        }
        int frame = ((int)(b->animationTime * 9.0f) + i * 2) % 8;
        float size = 72.0f;
        DrawTexturePro(b->sprites, (Rectangle){frame*80.0f,0,80,80},
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
    for (int i=0; i<BIBLE_COUNT; i++) {
        Vector2 book = BookCenter(b, player, i);
        float dx = book.x - enemy.x, dy = book.y - enemy.y;
        if (dx*dx + dy*dy <= limit*limit) return true;
    }
    return false;
}

int BibleContactDamage(void) { return 12; }
