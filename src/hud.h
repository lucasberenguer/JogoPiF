#ifndef HUD_H
#define HUD_H

#include "raylib.h"

#define PRACTICE_FOOTER_HEIGHT 28

typedef struct Hud {
    Texture2D base;
    Texture2D medium;
    Texture2D critical;
    Texture2D defeated;
    Texture2D healthFill;
    Texture2D dodgeFill;
} Hud;

bool InitHud(Hud *hud);
void DrawHud(const Hud *hud, int health, int maxHealth,
             float dodgeCooldown, float dodgeRecovery, double runSeconds);
void UnloadHud(Hud *hud);
void DrawBossHealthHud(const char *name, const char *phase, int health, int maxHealth,
                       Color fill, const float *phaseMarkers, int markerCount, int bottomOffset,
                       int transitionMarker, float transitionTime);

#endif
