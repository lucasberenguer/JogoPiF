#include "game_text.h"
#include "hud.h"
#include <math.h>

#define HUD_WIDTH 2082.0f
#define HUD_HEIGHT 640.0f

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
