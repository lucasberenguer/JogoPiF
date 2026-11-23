#include "game_text.h"
#include "hud.h"
#include <math.h>

#define HUD_WIDTH 2082.0f
#define HUD_HEIGHT 640.0f

static const Rectangle bossFrameSource = {0, 96, 2172, 530};
static const float bossFrameTop = 31;
static Texture2D bossHealthFrame;
static bool bossHealthFrameAttempted;

static float BossHealthHudScale(void) {
    return fminf(600, GetScreenWidth() * .56f) / bossFrameSource.width;
}

int BossHealthHudHeight(void) {
    return (int)ceilf(bossFrameTop + bossFrameSource.height * BossHealthHudScale() + 4);
}

void DrawBossHealthHud(const char *name, const char *phase, int health, int maxHealth,
                       Color fill, const float *phaseMarkers, int markerCount) {
    if (maxHealth <= 0) return;
    if (!bossHealthFrameAttempted) {
        bossHealthFrameAttempted = true;
        bossHealthFrame = LoadTexture("assets/ui/boss_health_frame.png");
        if (bossHealthFrame.id) SetTextureFilter(bossHealthFrame, TEXTURE_FILTER_BILINEAR);
    }

    float scale = BossHealthHudScale();
    float width = bossFrameSource.width * scale;
    float x = (GetScreenWidth() - width) * .5f;
    Rectangle track = {x + 249 * scale, bossFrameTop + (299 - bossFrameSource.y) * scale,
                       1674 * scale, 125 * scale};
    float ratio = fmaxf(0, fminf(1, (float)health / maxHealth));
    DrawRectangleRounded(track, .45f, 8, (Color){24, 13, 17, 255});
    if (ratio > 0) {
        Rectangle filled = {track.x, track.y, track.width * ratio, track.height};
        DrawRectangleGradientEx(filled, ColorBrightness(fill, .27f), fill,
                                fill, ColorBrightness(fill, .27f));
    }
    for (int i = 0; i < markerCount; i++) {
        float markerX = track.x + track.width * phaseMarkers[i];
        DrawRectangleRec((Rectangle){markerX - 1, track.y, 2, track.height}, Fade(GOLD, .68f));
    }
    if (bossHealthFrame.id) {
        DrawTexturePro(bossHealthFrame, bossFrameSource,
            (Rectangle){x, bossFrameTop, width, bossFrameSource.height * scale},
            (Vector2){0}, 0, WHITE);
    } else {
        DrawRectangleLinesEx(track, 2, (Color){170, 105, 84, 255});
    }
    Rectangle title = {x, 4, width, 31};
    Rectangle shadow = title;
    shadow.y += 2;
    DrawGameTextIn(name, shadow, 30, BLACK, true);
    DrawGameTextIn(name, title, 30, (Color){255, 221, 183, 255}, true);

    const char *status = phase && *phase ? TextFormat("%d/%d  |  %s", health, maxHealth, phase)
                                         : TextFormat("%d/%d", health, maxHealth);
    Rectangle label = {track.x + track.width * .10f, track.y + 2, track.width * .80f, track.height - 4};
    shadow = label;
    shadow.y += 1;
    DrawGameTextIn(status, shadow, 17, BLACK, false);
    DrawGameTextIn(status, label, 17, RAYWHITE, false);
}

void UnloadBossHealthHud(void) {
    if (bossHealthFrame.id) UnloadTexture(bossHealthFrame);
    bossHealthFrame = (Texture2D){0};
    bossHealthFrameAttempted = false;
}

bool InitHud(Hud *hud) {
    hud->base = LoadTexture("assets/hud_base.png");
    hud->medium = LoadTexture("assets/hud_media.png");
    hud->critical = LoadTexture("assets/hud_critica.png");
    hud->defeated = LoadTexture("assets/hud_derrota.png");
    hud->healthFill = LoadTexture("assets/hud_vida.png");
    hud->dodgeFill = LoadTexture("assets/hud_esquiva.png");
    if (hud->base.id == 0 || hud->medium.id == 0 || hud->critical.id == 0 ||
        hud->defeated.id == 0 || hud->healthFill.id == 0 || hud->dodgeFill.id == 0 ||
        hud->base.width != (int) HUD_WIDTH || hud->base.height != (int) HUD_HEIGHT ||
        hud->medium.width != (int) HUD_WIDTH || hud->medium.height != (int) HUD_HEIGHT ||
        hud->critical.width != (int) HUD_WIDTH || hud->critical.height != (int) HUD_HEIGHT ||
        hud->defeated.width != (int) HUD_WIDTH || hud->defeated.height != (int) HUD_HEIGHT ||
        hud->healthFill.width != (int) HUD_WIDTH ||
        hud->healthFill.height != (int) HUD_HEIGHT ||
        hud->dodgeFill.width != (int) HUD_WIDTH ||
        hud->dodgeFill.height != (int) HUD_HEIGHT) {
        TraceLog(LOG_ERROR, "Nao foi possivel carregar os assets da HUD");
        UnloadHud(hud);
        return false;
    }
    SetTextureFilter(hud->base, TEXTURE_FILTER_POINT);
    SetTextureFilter(hud->medium, TEXTURE_FILTER_POINT);
    SetTextureFilter(hud->critical, TEXTURE_FILTER_POINT);
    SetTextureFilter(hud->defeated, TEXTURE_FILTER_POINT);
    SetTextureFilter(hud->healthFill, TEXTURE_FILTER_POINT);
    SetTextureFilter(hud->dodgeFill, TEXTURE_FILTER_POINT);
    return true;
}

static void DrawBar(const Texture2D *texture, float start, float length,
                    float ratio, float x, float y, float scale) {
    if (ratio <= 0.0f) return;
    if (ratio > 1.0f) ratio = 1.0f;
    float visible = length * ratio;
    DrawTexturePro(*texture,
        (Rectangle) { start, 0.0f, visible, HUD_HEIGHT },
        (Rectangle) { x + start * scale, y, visible * scale, HUD_HEIGHT * scale },
        (Vector2) { 0, 0 }, 0.0f, WHITE);
}

static void DrawCenteredLabel(const char *label, float centerX, float top,
                              int fontSize) {
    int width = MeasureGameText(label, fontSize);
    int x = (int) (centerX - width / 2.0f);
    int y = (int) top;
    DrawGameText(label, x - 1, y, fontSize, BLACK);
    DrawGameText(label, x + 1, y, fontSize, BLACK);
    DrawGameText(label, x, y - 1, fontSize, BLACK);
    DrawGameText(label, x, y + 1, fontSize, BLACK);
    DrawGameText(label, x, y, fontSize, RAYWHITE);
}

void DrawHud(const Hud *hud, int health, int maxHealth,
             float dodgeCooldown, float dodgeRecovery, int topOffset) {
    float width = fminf(650.0f, GetScreenWidth() * 0.38f);
    float scale = width / HUD_WIDTH;
    float x = GetScreenWidth() - width - 16.0f;
    float y = 12.0f + topOffset;
    float healthRatio = maxHealth > 0 ? (float) health / (float) maxHealth : 0.0f;
    const Texture2D *frame = &hud->base;
    if (healthRatio <= 0.0f) frame = &hud->defeated;
    else if (healthRatio <= 0.35f) frame = &hud->critical;
    else if (healthRatio <= 0.60f) frame = &hud->medium;
    DrawTexturePro(*frame,
        (Rectangle) { 0, 0, HUD_WIDTH, HUD_HEIGHT },
        (Rectangle) { x, y, width, HUD_HEIGHT * scale },
        (Vector2) { 0, 0 }, 0.0f, WHITE);

    int fontSize = (int) fmaxf(16.0f, 62.0f * scale);
    if (healthRatio <= 0.0f) {
        DrawCenteredLabel("DERROTA", x + 930.0f * scale,
                          y + 228.0f * scale - 1.0f, fontSize);
        return;
    }
    float dodgeRatio = dodgeRecovery > 0.0f
        ? 1.0f - dodgeCooldown / dodgeRecovery : 1.0f;
    DrawBar(&hud->healthFill, 285.0f, 1260.0f, healthRatio, x, y, scale);
    DrawBar(&hud->dodgeFill, 340.0f, 1110.0f, dodgeRatio, x, y, scale);

    DrawCenteredLabel(TextFormat("%d/%d", health, maxHealth),
                      x + 930.0f * scale, y + 228.0f * scale - 1.0f, fontSize);
    const char *dodgeLabel = dodgeCooldown > 0.0f
        ? TextFormat("%.1fs", dodgeCooldown) : "ESQUIVA PRONTA";
    DrawCenteredLabel(dodgeLabel, x + 945.0f * scale,
                      y + 373.0f * scale - 1.0f, fontSize);
}

void UnloadHud(Hud *hud) {
    if (hud->base.id != 0) UnloadTexture(hud->base);
    if (hud->medium.id != 0) UnloadTexture(hud->medium);
    if (hud->critical.id != 0) UnloadTexture(hud->critical);
    if (hud->defeated.id != 0) UnloadTexture(hud->defeated);
    if (hud->healthFill.id != 0) UnloadTexture(hud->healthFill);
    if (hud->dodgeFill.id != 0) UnloadTexture(hud->dodgeFill);
    *hud = (Hud) { 0 };
}
