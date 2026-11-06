#ifndef PRESENTATION_H
#define PRESENTATION_H
#include "raylib.h"
/* Shared by maps: presentation time never advances the combat simulation. */
typedef struct StagePresentation {
    float remaining, duration;
    float portraitCenterX;
    int wave;
    bool boss;
    const char *mapName, *bossName;
    const char *announcement;
} StagePresentation;
void BeginStagePresentation(StagePresentation *p, int wave, const char *mapName, const char *bossName);
void UpdateStagePresentation(StagePresentation *p, float dt);
void DrawStagePresentation(const StagePresentation *p, Texture2D bossSprite, int columns, int rows);
#endif
