#ifndef ENEMY_H
#define ENEMY_H
#include "raylib.h"
#include "clearing.h"

#define ENEMY_COUNT 24
#define ENEMY_PROJECTILE_COUNT 48
#define ENEMY_ROOT_COUNT 8
typedef struct EnemyRoot {
    Vector2 position;
    float age;
    bool active;
    bool hit;
} EnemyRoot;
typedef struct EnemyProjectile {
    Vector2 position;
    Vector2 direction;
    float remaining;
    int damage;
} EnemyProjectile;
typedef enum EnemyType {
    ENEMY_COMMON_MELEE, ENEMY_COMMON_RANGED, ENEMY_SPECIAL,
    ENEMY_ELITE, ENEMY_BOSS
} EnemyType;
typedef enum EnemyState {
    ENEMY_IDLE, ENEMY_WALK, ENEMY_ATTACK, ENEMY_HURT, ENEMY_DIE, ENEMY_GONE,
    ENEMY_SPAWNING, ENEMY_FALLEN
} EnemyState;
typedef struct Enemy {
    Vector2 position; /* ponto dos pes */
    int health;
    int maxHealth;
    float speed;
    float power;
    Vector2 attackDirection;
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
    Texture2D rangedSprites;
    Texture2D bossSprites, bossAttack, roots;
    Enemy list[ENEMY_COUNT];
    EnemyProjectile projectiles[ENEMY_PROJECTILE_COUNT];
    EnemyRoot rootAttacks[ENEMY_ROOT_COUNT];
    float playerHitCooldown;
} EnemyGroup;

bool InitEnemies(EnemyGroup *group);
void UnloadEnemies(EnemyGroup *group);
void ResetEnemies(EnemyGroup *group, const Clearing *clearing);
void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower);
void StartClearingBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player,
                       float power);
void ClearEnemyAttacks(EnemyGroup *group);
int EnemiesRemaining(const EnemyGroup *group);
float EnemyHurtRadius(const Enemy *enemy);
void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float deltaTime, bool playerDodging,
                   int *playerHealth, int playerMaxHealth);
void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground);
void DamageEnemy(Enemy *enemy, int damage);
void KnockbackEnemy(Enemy *enemy, const Clearing *clearing,
                    Vector2 source, float distance);
bool EnemyCanBeHit(const Enemy *enemy);
Vector2 EnemyCenter(const Enemy *enemy);
int EnemyOptionalDropChance(EnemyType type);
bool EnemyShouldDrop(EnemyType type, int roll);
int EnemyAnimationFrame(const Enemy *enemy);
void DrawEnemyDebug(const EnemyGroup *group);
#endif
