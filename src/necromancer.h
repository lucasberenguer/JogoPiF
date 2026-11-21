#ifndef NECROMANCER_H
#define NECROMANCER_H
#include "raylib.h"

struct Enemy;
struct EnemyGroup;
struct Clearing;

enum { NECROMANCER_FORMS = 3, NECROMANCER_SHOTS = 32 };
typedef struct NecromancerShot {
    Vector2 position, velocity;
    float life;
    int damage;
} NecromancerShot;

typedef struct NecromancerFight {
    NecromancerShot shots[NECROMANCER_SHOTS];
    Vector2 marks[3];
    Vector2 previousPlayer, lastPlayer;
    float sampleClock;
    int rotation;
} NecromancerFight;

bool LoadNecromancerSprites(struct EnemyGroup *group);
void UnloadNecromancerSprites(struct EnemyGroup *group);
void StartNecromancer(struct EnemyGroup *group, float power);
void DamageNecromancer(struct Enemy *boss, int damage);
void UpdateNecromancer(struct EnemyGroup *group, const struct Clearing *map,
                       Vector2 player, float dt, bool dodging, int *health);
void DrawNecromancer(const struct EnemyGroup *group, Vector2 player, bool foreground);
#endif
