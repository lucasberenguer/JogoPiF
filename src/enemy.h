#ifndef ENEMY_H
#define ENEMY_H
#include "raylib.h"
#include "clearing.h"

#define ENEMY_COUNT 4
typedef enum EnemyType {
    ENEMY_COMMON_MELEE, ENEMY_COMMON_RANGED, ENEMY_SPECIAL,
    ENEMY_ELITE, ENEMY_BOSS
} EnemyType;
typedef enum EnemyState {
    ENEMY_IDLE, ENEMY_WALK, ENEMY_ATTACK, ENEMY_HURT, ENEMY_DIE, ENEMY_GONE
} EnemyState;
typedef struct Enemy {
    Vector2 position; /* ponto dos pes */
    int health;
    EnemyType type;
    EnemyState state;
    int facing;
    int lastKatanaSwing;
    float stateTime;
    float attackCooldown;
    float bibleHitCooldown;
    float walkTime;
    bool attackConnected;
    bool pickupAvailable;
    bool dropRolled;
} Enemy;
typedef struct EnemyGroup {
    Texture2D sprites;
    Texture2D backSprites;
    Texture2D frontSprites;
    Enemy list[ENEMY_COUNT];
    float playerHitCooldown;
} EnemyGroup;

bool InitEnemies(EnemyGroup *group);
void UnloadEnemies(EnemyGroup *group);
void ResetEnemies(EnemyGroup *group, const Clearing *clearing);
void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float deltaTime, bool playerDodging,
                   int *playerHealth, int playerMaxHealth);
void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground);
void DamageEnemy(Enemy *enemy, int damage);
bool EnemyCanBeHit(const Enemy *enemy);
Vector2 EnemyCenter(const Enemy *enemy);
int EnemyOptionalDropChance(EnemyType type);
bool EnemyShouldDrop(EnemyType type, int roll);
int EnemyAnimationFrame(const Enemy *enemy);
void DrawEnemyDebug(const EnemyGroup *group);
#endif
