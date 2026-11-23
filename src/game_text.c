#include "game_text.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define GAME_FONT_BASE_SIZE 64
#define GAME_FONT_PADDING 4
#define CUTSCENE_FONT_PATH "assets/fonts/CrimsonText-SemiBold.ttf"
#define LOGIC_SYMBOLS_FONT_PATH "assets/fonts/LogicSymbols.ttf"

static Font cutsceneFont;
static Font gothicFont;

static Font LoadGameFont(const char *path, const char *donorSymbolsPath, bool readableTitle) {
    if (!path || !FileExists(path)) return (Font){0};
    // ASCII, Latin-1 (incluindo acentos do português) e pontuação editorial.
    int codepoints[256];
    int count = 0;
    for (int cp = 32; cp <= 126; cp++) codepoints[count++] = cp;
    for (int cp = 160; cp <= 255; cp++) codepoints[count++] = cp;
    const int punctuation[] = {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026};
    for (unsigned int i = 0; i < sizeof(punctuation)/sizeof(*punctuation); i++)
        codepoints[count++] = punctuation[i];

    int dataSize = 0;
    unsigned char *fileData = LoadFileData(path, &dataSize);
    if (!fileData) return (Font){0};

    GlyphInfo *baseGlyphs = LoadFontData(fileData, dataSize, GAME_FONT_BASE_SIZE, codepoints, count, FONT_DEFAULT);
    UnloadFileData(fileData);
    if (!baseGlyphs) return (Font){0};

    // O S da Unifraktur parece um G. Troca só essa inicial no atlas gótico.
    if (readableTitle) {
        int size = 0;
        unsigned char *data = LoadFileData(CUTSCENE_FONT_PATH, &size);
        if (data) {
            int letter = 'S';
            GlyphInfo *glyph = LoadFontData(data, size, GAME_FONT_BASE_SIZE * 5 / 4, &letter, 1, FONT_DEFAULT);
            UnloadFileData(data);
            if (glyph) {
                if (glyph->image.data) {
                    const GlyphInfo *original = &baseGlyphs[letter - 32];
                    glyph->offsetY = original->offsetY + original->image.height - glyph->image.height;
                    UnloadImage(baseGlyphs[letter - 32].image);
                    baseGlyphs[letter - 32] = *glyph;
                    MemFree(glyph);
                } else {
                    UnloadFontData(glyph, 1);
                }
            }
        }
    }

    GlyphInfo *mergedGlyphs = baseGlyphs;
    int totalCount = count;

    if (donorSymbolsPath && donorSymbolsPath[0] && FileExists(donorSymbolsPath)) {
        int donorSize = 0;
        unsigned char *donorData = LoadFileData(donorSymbolsPath, &donorSize);
        if (donorData) {
            int logicCps[2] = {0x2227, 0x2228}; // ∧ e ∨
            GlyphInfo *donorGlyphs = LoadFontData(donorData, donorSize, GAME_FONT_BASE_SIZE, logicCps, 2, FONT_DEFAULT);
            UnloadFileData(donorData);
            if (donorGlyphs) {
                if (donorGlyphs[0].image.data != NULL && donorGlyphs[1].image.data != NULL) {
                    GlyphInfo *combined = (GlyphInfo *)MemAlloc((unsigned int)(count + 2) * sizeof(GlyphInfo));
                    if (combined) {
                        memcpy(combined, baseGlyphs, (size_t)count * sizeof(GlyphInfo));
                        combined[count] = donorGlyphs[0];
                        combined[count + 1] = donorGlyphs[1];
                        MemFree(baseGlyphs);
                        MemFree(donorGlyphs);
                        mergedGlyphs = combined;
                        totalCount = count + 2;
                    } else {
                        UnloadFontData(donorGlyphs, 2);
                    }
                } else {
                    UnloadFontData(donorGlyphs, 2);
                }
            }
        }
    }

    Font font = {0};
    font.baseSize = GAME_FONT_BASE_SIZE;
    font.glyphCount = totalCount;
    font.glyphPadding = GAME_FONT_PADDING;
    font.glyphs = mergedGlyphs;

    Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize, font.glyphPadding, 0);
    if (atlas.data) {
        font.texture = LoadTextureFromImage(atlas);
        for (int i = 0; i < font.glyphCount; i++) {
            UnloadImage(font.glyphs[i].image);
            font.glyphs[i].image = ImageFromImage(atlas, font.recs[i]);
        }
        UnloadImage(atlas);
    }

    if (!font.texture.id || font.texture.id == GetFontDefault().texture.id) {
        UnloadFontData(font.glyphs, font.glyphCount);
        if (font.recs) MemFree(font.recs);
        return (Font){0};
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

void LoadGameFontsWithSymbolsPath(const char *symbolsPath) {
    UnloadGameFonts();
    gothicFont = LoadGameFont("assets/fonts/UnifrakturMaguntia-Book.ttf", symbolsPath, true);
    cutsceneFont = LoadGameFont(CUTSCENE_FONT_PATH, gothicFont.texture.id ? NULL : symbolsPath, false);
}

void LoadGameFonts(void) {
    LoadGameFontsWithSymbolsPath(LOGIC_SYMBOLS_FONT_PATH);
}

void UnloadGameFonts(void) {
    if (cutsceneFont.texture.id) UnloadFont(cutsceneFont);
    if (gothicFont.texture.id) UnloadFont(gothicFont);
    cutsceneFont = (Font){0};
    gothicFont = (Font){0};
}

static Font TextFont(GameTextStyle style) {
    if (style != GAME_TEXT_CUTSCENE && gothicFont.texture.id) return gothicFont;
    return cutsceneFont.texture.id ? cutsceneFont : GetFontDefault();
}

bool GameTextHasGlyph(int codepoint) {
    Font font = TextFont(GAME_TEXT_BODY);
    if (!font.texture.id || !font.glyphs || !font.recs) return false;
    for (int i = 0; i < font.glyphCount; i++) {
        if (font.glyphs[i].value == codepoint && font.glyphs[i].image.data &&
            font.recs[i].width > 0 && font.recs[i].height > 0) {
            return true;
        }
    }
    return false;
}

static float TextSize(float size, GameTextStyle style) {
    return style == GAME_TEXT_TITLE ? size : size*1.10f;
}

static float TextSpacing(float size, GameTextStyle style) {
    return size * (style == GAME_TEXT_TITLE ? .034f : .020f);
}

Vector2 MeasureGameTextEx(const char *text, float size, GameTextStyle style) {
    size = TextSize(size, style);
    return MeasureTextEx(TextFont(style), text, size, TextSpacing(size, style));
}

void DrawGameTextEx(const char *text, Vector2 position, float size, Color color, GameTextStyle style) {
    size = TextSize(size, style);
    DrawTextEx(TextFont(style), text, position, size, TextSpacing(size, style), color);
}

int MeasureGameText(const char *text, int size) {
    return (int)ceilf(MeasureGameTextEx(text, (float)size, GAME_TEXT_BODY).x);
}

void DrawGameText(const char *text, int x, int y, int size, Color color) {
    DrawGameTextEx(text, (Vector2){x,y}, (float)size, color, GAME_TEXT_BODY);
}

void DrawGameTextIn(const char *text, Rectangle bounds, float size, Color color, GameTextStyle style) {
    Vector2 metrics = MeasureGameTextEx(text, size, style);
    if (metrics.x <= 0 || metrics.y <= 0) return;
    float scale = fminf(1.0f, fminf(bounds.width/metrics.x, bounds.height/metrics.y));
    Vector2 position = {bounds.x + (bounds.width - metrics.x*scale)*.5f,
                        bounds.y + (bounds.height - metrics.y*scale)*.5f};
    DrawGameTextEx(text, position, size*scale, color, style);
}
