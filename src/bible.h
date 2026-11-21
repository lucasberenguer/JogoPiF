#ifndef BIBLE_H
#define BIBLE_H
#include "raylib.h"
#include "run_progress.h"
#define BIBLE_ACTIVATION_DURATION 0.64f
#define BIBLE_ACTIVATION_FRAMES 8
#define BIBLE_MAX_POOLS 4
#define BIBLE_BASE_BOOKS 4
#define BIBLE_L2_BOOKS 5
#define BIBLE_L4_BOOKS 6
#define BIBLE_BASE_ORBIT_RADIUS 90.0f
#define BIBLE_L2_ORBIT_RADIUS 102.0f
#define BIBLE_HIT_COOLDOWN 0.45f
#define BIBLE_L1_KNOCKBACK 36.0f
#define BIBLE_BASE_BOOST_DURATION 1.25f
#define BIBLE_L3_BOOST_DURATION 1.65f
#define BIBLE_BASE_BOOST_COOLDOWN 4.0f
#define BIBLE_L3_BOOST_COOLDOWN 2.8f
#define BIBLE_POOL_DAMAGE 8

typedef struct HolyWaterPool {
    Vector2 position;
    float remaining;
    float tickTimer;
    bool tickReady;
} HolyWaterPool;

typedef struct Bible {
    Texture2D sprites;
    Texture2D activation;
    Texture2D idle;
    Texture2D walk;
    float angle;
    float animationTime;
    float boostRemaining;
    float boostCooldown;
    float activationRemaining;
    int level;
    float holyWaterCooldown;
    HolyWaterPool pools[BIBLE_MAX_POOLS];
} Bible;

bool InitBible(Bible *bible);
void UnloadBible(Bible *bible);
void ResetBible(Bible *bible);
void UpdateBible(Bible *bible, float deltaTime, bool activate, int level,
                 bool inCombat, Vector2 player);
void DrawBiblePools(const Bible *bible);
void DrawBible(const Bible *bible, Vector2 player, bool foreground);
void DrawBibleActivation(const Bible *bible, Vector2 player);
bool BibleTouchesCircle(const Bible *bible, Vector2 player,
                        Vector2 enemy, float enemyRadius);
bool BibleBlocksCircle(const Bible *bible, Vector2 player,
                       Vector2 point, float radius);
bool BibleHolyWaterHits(const Bible *bible, Vector2 enemy, float enemyRadius);
int BibleContactDamage(void);
#endif
