#ifndef ENEMY_H
#define ENEMY_H
#include "raylib.h"
#include "clearing.h"
#include "necromancer.h"

#define ENEMY_COUNT 30
#define ENEMY_PROJECTILE_COUNT 48
#define ENEMY_ROOT_COUNT 8
#define CEMETERY_PROJECTILE_COUNT 32
typedef enum GuardianAttack {
    GUARDIAN_ROOTS, GUARDIAN_ROOT_WAVE, GUARDIAN_CURSE, GUARDIAN_ATTACK_COUNT
} GuardianAttack;
enum { COVEIRO_ROOTS, COVEIRO_SHOVEL, COVEIRO_CHARGE, COVEIRO_ATTACK_COUNT };
enum { MARSHAL_SLASH, MARSHAL_CHARGE, MARSHAL_RETURN, MARSHAL_RUPTURE };
enum { BONE_CHARGE, BONE_CLAW, BONE_DASH, BONE_ATTACK_COUNT };
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

typedef struct EnemyImpact { Vector2 position; float remaining; Vector2 direction; } EnemyImpact;
typedef enum EnemyType {
    ENEMY_COMMON_MELEE, ENEMY_COMMON_RANGED, ENEMY_SPECIAL,
    ENEMY_ELITE, ENEMY_BOSS
} EnemyType;
typedef enum TemplarRole {
    TEMPLAR_NONE, TEMPLAR_SHIELD, TEMPLAR_EXECUTIONER, TEMPLAR_CROSSBOW,
    TEMPLAR_STANDARD, TEMPLAR_MARSHAL
} TemplarRole;
typedef enum EnemyState {
    ENEMY_IDLE, ENEMY_WALK, ENEMY_ATTACK, ENEMY_HURT, ENEMY_DIE, ENEMY_GONE,
    ENEMY_SPAWNING, ENEMY_FALLEN
} EnemyState;
enum { VILLAGE_ZOMBIE_IDLE, VILLAGE_ZOMBIE_EMERGE, VILLAGE_ZOMBIE_WALK,
       VILLAGE_ZOMBIE_ATTACK, VILLAGE_ZOMBIE_ATLAS_COUNT };
#define VILLAGE_ZOMBIE_EMERGE_TIME 1.0f
#define VILLAGE_ZOMBIE_HIT_TIME .31f
typedef struct Enemy {
    Vector2 position; /* ponto dos pes */
    int health;
    int maxHealth;
    float speed;
    float power;
    Vector2 attackDirection;
    EnemyType type;
    TemplarRole templarRole;
    int necromancerForm;
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
    Texture2D clearingMelee[3], clearingArcher[3], clearingSpecial[3];
    Texture2D necromancerWalk[NECROMANCER_FORMS], necromancerSkills[NECROMANCER_FORMS];
    Texture2D necromancerThrone, necromancerPortrait;
    NecromancerFight necromancer;
    Texture2D templarWalk[TEMPLAR_MARSHAL + 1], templarAttack[TEMPLAR_MARSHAL + 1];
    Texture2D bonelandGhoul, bonelandCultist;
    Texture2D boneLordWalk, boneLordPounce, boneLordClaw, boneLordDash, boneLordBite;
    Texture2D sealMarker;
    Texture2D bossSprites, bossWalk, bossAttack, roots, rootWave, curse, guardianMelee;
    Texture2D guardianSpawn, guardianDefeat;
    Texture2D bossPortrait, cemeteryPortrait, boneLordPortrait, marshalPortrait;
    Enemy list[ENEMY_COUNT];
    EnemyProjectile projectiles[ENEMY_PROJECTILE_COUNT];
    EnemyRoot rootAttacks[ENEMY_ROOT_COUNT];
    float playerHitCooldown;
    bool boneland;
    bool practiceInvincible;
    bool cemetery;
    bool village;
    int villageSpawnRemaining;
    float villageSpawnTimer;
    float villageHordePower;
    int villageBossDeathWeapon;
    Texture2D villageLetter;
    Texture2D villageBossWalk, villageBossAttack, villageBossCharge;
    Texture2D villageZombie[VILLAGE_ZOMBIE_ATLAS_COUNT];
    Texture2D cemeteryZombie[3], cemeteryLantern[3], cemeterySpell;
    Texture2D coveiroWalk,coveiroMelee,boss[3],effects[3];
    EnemyImpact impacts[16], templarBlood;
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
void DrawHumanBoss(const EnemyGroup *group, const Enemy *enemy, bool foreground);
void DrawHealthPickup(Vector2 position);
void DrawBossGroundSeal(Texture2D sealMarker, Vector2 position);
void DamageEnemy(Enemy *enemy, int damage);
void DamageEnemyFrom(Enemy *enemy, int damage, Vector2 source);
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
enum { CEM_BOSS_WALK,CEM_BOSS_SLAM,CEM_BOSS_THROW,CEM_BOSS_CHARGE,CEM_BOSS_MELEE,
       CEM_ZOMBIE_IDLE,CEM_ZOMBIE_WALK,CEM_ZOMBIE_ATTACK,
       CEM_LANTERN_IDLE,CEM_LANTERN_WALK,CEM_LANTERN_ATTACK };
#define CEM_ZOMBIE_HIT_TIME .41f
#define CEM_LANTERN_RELEASE_TIME .55f
CemeteryPose CemeteryAnimationPose(const Enemy *enemy);
void CemeteryUpdateBoss(EnemyGroup *, const Clearing *, Enemy *, Vector2, float, bool, int *);
void CemeteryUpdateProjectiles(EnemyGroup *, const Clearing *, Vector2, float, bool, int *);
void CemeteryDrawEnemies(const EnemyGroup *,Vector2,bool);
void DrawEnemyProjectiles(const EnemyGroup *);
void StartVillageHorde(EnemyGroup *group, const Clearing *clearing,
                       Vector2 player, float power);
void VillageUpdateHorde(EnemyGroup *group, const Clearing *clearing,
                        Vector2 player, float dt);
void VillageUpdateBoss(EnemyGroup *group, const Clearing *map, Enemy *boss,
                       Vector2 player, float dt, bool dodging, int *playerHealth);
void VillageDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground);
#endif
