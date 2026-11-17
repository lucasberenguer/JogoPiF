#include "game_text.h"
#include <math.h>

static Font bodyFont;
static Font titleFont;

static Font LoadGameFont(const char *path) {
    if (!FileExists(path)) return (Font){0};
    // ASCII, Latin-1 (incluindo acentos do português) e pontuação editorial.
    int codepoints[256];
    int count = 0;
    for (int cp = 32; cp <= 126; cp++) codepoints[count++] = cp;
    for (int cp = 160; cp <= 255; cp++) codepoints[count++] = cp;
    const int punctuation[] = {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026};
    for (unsigned int i = 0; i < sizeof(punctuation)/sizeof(*punctuation); i++)
        codepoints[count++] = punctuation[i];
    Font font = LoadFontEx(path, 64, codepoints, count);
    if (font.texture.id == GetFontDefault().texture.id) return (Font){0};
    if (font.texture.id) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

void LoadGameFonts(void) {
    bodyFont = LoadGameFont("assets/fonts/CrimsonText-SemiBold.ttf");
    titleFont = LoadGameFont("assets/fonts/UnifrakturMaguntia-Book.ttf");
}

void UnloadGameFonts(void) {
    if (bodyFont.texture.id) UnloadFont(bodyFont);
    if (titleFont.texture.id) UnloadFont(titleFont);
    bodyFont = (Font){0};
    titleFont = (Font){0};
}

static Font TextFont(bool title) {
    if (title && titleFont.texture.id) return titleFont;
    return bodyFont.texture.id ? bodyFont : GetFontDefault();
}

static float TextSize(float size, bool title) {
    return title ? size : size*1.10f;
}

static float TextSpacing(float size, bool title) {
    return size * (title ? .034f : .020f);
}

Vector2 MeasureGameTextEx(const char *text, float size, bool title) {
    size = TextSize(size, title);
    return MeasureTextEx(TextFont(title), text, size, TextSpacing(size, title));
}

void DrawGameTextEx(const char *text, Vector2 position, float size, Color color, bool title) {
    size = TextSize(size, title);
    DrawTextEx(TextFont(title), text, position, size, TextSpacing(size, title), color);
}

int MeasureGameText(const char *text, int size) {
    return (int)ceilf(MeasureGameTextEx(text, (float)size, false).x);
}

void DrawGameText(const char *text, int x, int y, int size, Color color) {
    DrawGameTextEx(text, (Vector2){x,y}, (float)size, color, false);
}

void DrawGameTextIn(const char *text, Rectangle bounds, float size, Color color, bool title) {
    Vector2 metrics = MeasureGameTextEx(text, size, title);
    if (metrics.x <= 0 || metrics.y <= 0) return;
    float scale = fminf(1.0f, fminf(bounds.width/metrics.x, bounds.height/metrics.y));
    Vector2 position = {bounds.x + (bounds.width - metrics.x*scale)*.5f,
                        bounds.y + (bounds.height - metrics.y*scale)*.5f};
    DrawGameTextEx(text, position, size*scale, color, title);
}
