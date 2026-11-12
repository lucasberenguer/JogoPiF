#ifndef ENEMY_H
#define ENEMY_H
#include "raylib.h"
#include "clearing.h"

#define ENEMY_COUNT 24
#define ENEMY_PROJECTILE_COUNT 48
#define ENEMY_ROOT_COUNT 8
#define CEMETERY_PROJECTILE_COUNT 32
typedef enum GuardianAttack {
    GUARDIAN_ROOTS, GUARDIAN_ROOT_WAVE, GUARDIAN_CURSE, GUARDIAN_ATTACK_COUNT
} GuardianAttack;
enum { COVEIRO_ROOTS, COVEIRO_SHOVEL, COVEIRO_CHARGE, COVEIRO_ATTACK_COUNT };
typedef enum GuardianPhase {
    GUARDIAN_PHASE_ONE, GUARDIAN_PHASE_TRANSITION, GUARDIAN_PHASE_TWO
} GuardianPhase;
typedef struct EnemyRoot {
    Vector2 position;
    float age;
    bool active;
    bool hit;
    GuardianAttack attack;
} EnemyRoot;
typedef struct EnemyProjectile {
    Vector2 position;
    Vector2 direction;
    float remaining;
    int damage;
} EnemyProjectile;

typedef struct EnemyImpact { Vector2 position; float remaining; } EnemyImpact;
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
    GuardianPhase guardianPhase;
    bool guardianMelee;
    float guardianMeleeCooldown;
    int bossAttack;
    int attackBurst;
    Vector2 attackTarget;
} Enemy;
typedef struct CemeteryProjectile {
    bool active;
    Vector2 position,velocity;
    float life;
    int damage;
    bool returning, hitPlayer;
} CemeteryProjectile;
typedef struct EnemyGroup {
    Texture2D sprites;
    Texture2D backSprites;
    Texture2D frontSprites;
    Texture2D rangedSprites;
    Texture2D bossSprites, bossWalk, bossAttack, roots, rootWave, curse, guardianMelee;
    Texture2D guardianSpawn, guardianDefeat;
    Texture2D bossPortrait, cemeteryPortrait;
    Enemy list[ENEMY_COUNT];
    EnemyProjectile projectiles[ENEMY_PROJECTILE_COUNT];
    EnemyRoot rootAttacks[ENEMY_ROOT_COUNT];
    float playerHitCooldown;
    bool cemetery;
    Texture2D skeleton,mage,boss[3],effects[3];
    Texture2D skeletonFront,skeletonBack,coveiroWalk,greenFire;
    EnemyImpact impacts[16];
    CemeteryProjectile cemeteryProjectiles[CEMETERY_PROJECTILE_COUNT];
} EnemyGroup;

bool InitEnemies(EnemyGroup *group);
void UnloadEnemies(EnemyGroup *group);
void ResetEnemies(EnemyGroup *group, const Clearing *clearing);
void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower);
void StartArenaBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player,
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
void MoveEnemy(const Clearing *clearing, Enemy *enemy, Vector2 delta);
bool EnemyCanBeHit(const Enemy *enemy);
Vector2 EnemyCenter(const Enemy *enemy);
int EnemyOptionalDropChance(EnemyType type);
bool EnemyShouldDrop(EnemyType type, int roll);
int EnemyAnimationFrame(const Enemy *enemy);
void DrawEnemyDebug(const EnemyGroup *group);
typedef struct CemeteryPose { int atlas,columns,rows,row,frame; bool flip; } CemeteryPose;
enum { CEM_SKELETON_SIDE,CEM_SKELETON_FRONT,CEM_SKELETON_BACK,CEM_MAGE,
       CEM_BOSS_WALK,CEM_BOSS_SLAM,CEM_BOSS_THROW,CEM_BOSS_CHARGE };
CemeteryPose CemeteryAnimationPose(const Enemy *enemy);
void CemeteryUpdateBoss(EnemyGroup *, const Clearing *, Enemy *, Vector2, float, bool, int *);
void CemeteryUpdateProjectiles(EnemyGroup *, const Clearing *, Vector2, float, bool, int *);
void CemeteryDrawEnemies(const EnemyGroup *,Vector2,bool);
void DrawEnemyProjectiles(const EnemyGroup *);
#endif
