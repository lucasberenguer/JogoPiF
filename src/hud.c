#include "game_text.h"
#include "hud.h"
#include "speedrun.h"
#include <math.h>

#define HUD_WIDTH 2082.0f
#define HUD_HEIGHT 640.0f

void DrawBossHealthHud(const char *name, const char *phase, int health, int maxHealth,
                       Color fill, const float *phaseMarkers, int markerCount, int bottomOffset) {
    if (maxHealth <= 0) return;

    const Color bronze = {179, 133, 93, 255};
    float width = floorf(fminf(800, GetScreenWidth() * .78f));
    float x = floorf((GetScreenWidth() - width) * .5f);
    float y = GetScreenHeight() - bottomOffset - 46;
    Rectangle track = {x, y, width, 30};
    DrawRectangleRec((Rectangle){x-3, y-3, width+6, 38}, Fade(BLACK, .83f));
    DrawRectangleRec(track, (Color){24, 15, 21, 255});
    float ratio = fmaxf(0, fminf(1, (float)health / maxHealth));
    if (ratio > 0) {
        Rectangle filled = {x+2, y+2, (width-4)*ratio, 26};
        Color highlight = ColorBrightness(fill, .24f);
        DrawRectangleGradientEx(filled, highlight, fill, fill, highlight);
    }
    DrawRectangleLinesEx(track, 1, bronze);
    for (int i = 0; i < markerCount; i++) {
        float markerX = x+2 + (width-4)*phaseMarkers[i];
        DrawRectangleRec((Rectangle){markerX-1, y+2, 2, 5}, bronze);
        DrawRectangleRec((Rectangle){markerX-1, y+23, 2, 5}, bronze);
    }
    for (int side = -1; side <= 1; side += 2) {
        float edge = side < 0 ? x : x+width;
        float cy = y+15;
        DrawPoly((Vector2){edge,cy}, 4, 13, 0, (Color){27,17,23,255});
        DrawPolyLinesEx((Vector2){edge,cy},4,12,0,1.5f,bronze);
        DrawCircleLines((int)edge,(int)cy,4,bronze);
        DrawLine((int)edge,(int)cy-7,(int)edge,(int)cy+7,bronze);
        DrawLine((int)edge-6,(int)cy,(int)edge+6,(int)cy,bronze);
        DrawPixel((int)edge,(int)cy,(Color){245,94,91,255});
        DrawLineEx((Vector2){edge+side*13,cy},(Vector2){edge+side*24,cy},1,bronze);
    }
    DrawLineEx((Vector2){x+13,y-2},(Vector2){x+52,y-2},1,Fade(bronze,.7f));
    DrawLineEx((Vector2){x+width-52,y-2},(Vector2){x+width-13,y-2},1,Fade(bronze,.7f));

    Rectangle title = {x, y-30, width, 29};
    Rectangle shadow = title;
    shadow.y += 2;
    DrawGameTextIn(name, shadow, 25, BLACK, GAME_TEXT_TITLE);
    DrawGameTextIn(name, title, 25, (Color){255, 221, 183, 255}, GAME_TEXT_TITLE);

    const char *status = phase && *phase ? TextFormat("%d/%d  |  %s", health, maxHealth, phase)
                                         : TextFormat("%d/%d", health, maxHealth);
    Rectangle label = {x+12, y+2, width-24, 26};
    shadow = label;
    shadow.y += 1;
    DrawGameTextIn(status, shadow, 19, BLACK, GAME_TEXT_BODY);
    DrawGameTextIn(status, label, 19, RAYWHITE, GAME_TEXT_BODY);
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
    DrawGameText(label, x, y + 1, fontSize, BLACK);
    DrawGameText(label, x, y, fontSize, RAYWHITE);
}

void DrawHud(const Hud *hud, int health, int maxHealth,
             float dodgeCooldown, float dodgeRecovery, double runSeconds) {
    float width = fminf(650.0f, GetScreenWidth() * 0.38f);
    float scale = width / HUD_WIDTH;
    float x = GetScreenWidth() - width - 16.0f;
    float y = 12.0f;
    float healthRatio = maxHealth > 0 ? (float) health / (float) maxHealth : 0.0f;
    const Texture2D *frame = &hud->base;
    if (healthRatio <= 0.0f) frame = &hud->defeated;
    else if (healthRatio <= 0.35f) frame = &hud->critical;
    else if (healthRatio <= 0.60f) frame = &hud->medium;
    DrawTexturePro(*frame,
        (Rectangle) { 0, 0, HUD_WIDTH, HUD_HEIGHT },
        (Rectangle) { x, y, width, HUD_HEIGHT * scale },
        (Vector2) { 0, 0 }, 0.0f, WHITE);

    int fontSize = (int) fmaxf(12.0f, 62.0f * scale);
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
        ? TextFormat("%.1fs", dodgeCooldown) : "Esquiva pronta";
    DrawCenteredLabel(dodgeLabel, x + 945.0f * scale,
                      y + 373.0f * scale - 1.0f, fontSize);
    char timeLabel[40];
    SpeedrunFormat(timeLabel,sizeof(timeLabel),runSeconds);
    DrawCenteredLabel(timeLabel,x + 945.0f*scale,
                      y + HUD_HEIGHT*scale + 3.0f, fmaxf(16.0f,fontSize));
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
