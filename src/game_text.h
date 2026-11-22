#ifndef GAME_TEXT_H
#define GAME_TEXT_H

#include "raylib.h"
#include <stdbool.h>

void LoadGameFonts(void);
void LoadGameFontsWithSymbolsPath(const char *symbolsPath);
void UnloadGameFonts(void);
bool GameTextHasGlyph(int codepoint);
void DrawGameText(const char *text, int x, int y, int size, Color color);
int MeasureGameText(const char *text, int size);
Vector2 MeasureGameTextEx(const char *text, float size, bool title);
void DrawGameTextEx(const char *text, Vector2 position, float size, Color color, bool title);
void DrawGameTextIn(const char *text, Rectangle bounds, float size, Color color, bool title);

#endif
