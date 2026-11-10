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
    const char *nextAreaName;
    bool areaUnlock;
    bool areaEntry;
} StagePresentation;
void BeginStagePresentation(StagePresentation *p, int wave, const char *mapName, const char *bossName);
void QueueAreaUnlock(StagePresentation *p, const char *mapName);
void BeginAreaEntry(StagePresentation *p);
void UpdateStagePresentation(StagePresentation *p, float dt);
void DrawStagePresentation(const StagePresentation *p, Texture2D bossSprite, int columns, int rows);
#endif
