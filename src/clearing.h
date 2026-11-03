#ifndef CLEARING_H
#define CLEARING_H

#include <stdbool.h>
#include "raylib.h"

typedef struct Clearing {
    Texture2D background;
    Texture2D collisionGuide;
    Texture2D foreground;
    Color *collisionPixels;
    Color *foregroundMaskPixels;
    Rectangle playableBounds;
    Vector2 spawn;
    Vector2 pedestal;
} Clearing;

bool InitClearing(Clearing *clearing);
void DrawClearing(const Clearing *clearing);
void DrawClearingForeground(const Clearing *clearing, Vector2 player);
void DrawClearingCollisionGuide(const Clearing *clearing);
bool IsClearingBlocked(const Clearing *clearing, Vector2 point);
void UnloadClearing(Clearing *clearing);

#endif
