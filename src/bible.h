#ifndef BIBLE_H
#define BIBLE_H
#include "raylib.h"

typedef struct Bible {
    Texture2D sprites;
    float angle;
    float animationTime;
    float boostRemaining;
    float boostCooldown;
} Bible;

bool InitBible(Bible *bible);
void UnloadBible(Bible *bible);
void ResetBible(Bible *bible);
void UpdateBible(Bible *bible, float deltaTime, bool activate);
void DrawBible(const Bible *bible, Vector2 player, bool foreground);
bool BibleTouchesCircle(const Bible *bible, Vector2 player,
                        Vector2 enemy, float enemyRadius);
int BibleContactDamage(void);
#endif
