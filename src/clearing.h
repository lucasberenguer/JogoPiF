#ifndef CLEARING_H
#define CLEARING_H

#include <stdbool.h>
#include "raylib.h"

enum { CLEARING_ALTAR_RADIUS = 72 };

typedef struct MapObstacle {
    Vector2 vertices[16];
    int count;
} MapObstacle;

typedef struct Clearing {
    Texture2D background;
    Texture2D collisionGuide;
    Texture2D foreground;
    Color *collisionPixels;
    Color *foregroundMaskPixels;
    Rectangle playableBounds;
    Vector2 boundary[192];
    int boundaryCount;
    Vector2 spawn;
    Vector2 pedestal;
    bool cemetery;
    Rectangle obstacles[20];
    int obstacleCount;
    MapObstacle shapes[24];
    int shapeCount;
    Rectangle exit;
} Clearing;

void ConfigureClearingBoundary(Clearing *clearing);
void ConfigureCemeteryBoundary(Clearing *map);
bool InitClearing(Clearing *clearing);
bool InitCemetery(Clearing *clearing);
void DrawClearing(const Clearing *clearing);
void DrawClearingAltarGlow(const Clearing *clearing, float time);
void DrawClearingForeground(const Clearing *clearing, Vector2 player);
void DrawClearingCollisionGuide(const Clearing *clearing);
bool IsClearingBlocked(const Clearing *clearing, Vector2 point);
bool IsMapActorBlocked(const Clearing *map, Vector2 point);
bool MapHasClearPath(const Clearing *map, Vector2 from, Vector2 to);
bool FindFreeMapPosition(const Clearing *map, Vector2 origin, Vector2 *result);
void MoveMapActor(const Clearing *map, Vector2 *position, Vector2 delta);
void MoveMapPlayer(const Clearing *map, Vector2 *position, Vector2 delta);
void UnloadClearing(Clearing *clearing);

#endif
