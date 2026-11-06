#ifndef HUD_H
#define HUD_H

#include "raylib.h"

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
             float dodgeCooldown, float dodgeRecovery, int topOffset);
void UnloadHud(Hud *hud);

#endif
