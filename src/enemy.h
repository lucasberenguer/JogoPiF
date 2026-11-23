#ifndef ENEMY_H
#define ENEMY_H
#include "raylib.h"
#include "clearing.h"
#include "necromancer.h"

#define ENEMY_COUNT 36
enum {
    GUARDIAN_ROOT_DAMAGE = 30, GUARDIAN_WAVE_DAMAGE = 36,
    GUARDIAN_CURSE_DAMAGE = 42, GUARDIAN_MELEE_DAMAGE = 26
};
#define ENEMY_PROJECTILE_COUNT 48
#define ENEMY_ROOT_COUNT 8
#define CEMETERY_PROJECTILE_COUNT 32
typedef enum GuardianAttack {
    GUARDIAN_ROOTS, GUARDIAN_ROOT_WAVE, GUARDIAN_CURSE, GUARDIAN_ATTACK_COUNT
} GuardianAttack;
enum { COVEIRO_ROOTS, COVEIRO_SHOVEL, COVEIRO_CHARGE, COVEIRO_ATTACK_COUNT };
enum { IGRIS_MELEE, IGRIS_ECLIPSE, IGRIS_ERUPTION, IGRIS_SPEARS, IGRIS_VORTEX, IGRIS_SKILL_COUNT };
enum { IGRIS_IDLE_ART, IGRIS_WALK_ART, IGRIS_MELEE_ART, IGRIS_ECLIPSE_ART, IGRIS_ECLIPSE_FX,
       IGRIS_ERUPTION_ART, IGRIS_ERUPTION_FX, IGRIS_SPEARS_ART, IGRIS_SPEARS_FX,
       IGRIS_VORTEX_ART, IGRIS_VORTEX_FX, IGRIS_FINAL_IDLE, IGRIS_FINAL_WALK,
       IGRIS_FINAL_MELEE, IGRIS_Z_DOWN, IGRIS_Z_UP, IGRIS_Z_LEFT, IGRIS_Z_RIGHT,
       IGRIS_GRAB_ART, IGRIS_GRAB_FX, IGRIS_ECHO_ART, IGRIS_ECHO_FX, IGRIS_ART_COUNT };
#define IGRIS_TITLE "IGRIS — O COMANDANTE"
#define IGRIS_FINAL_HEALTH_PERCENT 25
enum { BONE_CLAW, BONE_RANGED, BONE_RAIN, BONE_DASH, BONE_BLAST, BONE_ATTACK_COUNT };
enum { BONE_IDLE_SHEET, BONE_WALK_SHEET, BONE_CLAW_SHEET, BONE_RANGED_SHEET,
       BONE_RAIN_SHEET, BONE_RAIN_FX_SHEET, BONE_DASH_SHEET, BONE_BLAST_SHEET,
       BONE_BLAST_FX_SHEET, BONE_SHEET_COUNT };
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
    bool boneShot, umbraSpear;
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
enum { VILLAGE_HORDE_INITIAL = 30, VILLAGE_HORDE_REINFORCEMENTS = 34 };
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
    float damageScale; /* Ajuste de dano do mapa; 0 = sem ajuste (valor antigo, truncado). */
    Vector2 attackDirection;
    EnemyType type;
    TemplarRole templarRole;
    int necromancerForm;
    bool necromancerWraith;
    bool villageLeader;
    bool villageBrute;
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
    bool igrisFinalForm;
    Vector2 igrisZPath[4];
    Vector2 igrisEchoFrom[3], igrisEchoTo[3];
    unsigned igrisEchoHits;
    bool guardianMelee;
    bool coveiroPhaseOpening;
    float guardianMeleeCooldown;
    float guardianDodgeCooldown, guardianDodgeTime;
    Vector2 guardianDodgeDirection;
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
    Texture2D necromancerBlade, necromancerEffects, necromancerThrone, necromancerPortrait;
    NecromancerFight necromancer;
    Texture2D igrisArt[IGRIS_ART_COUNT], templarEffects, templarDust;
    Texture2D templarIdle[TEMPLAR_MARSHAL], templarWalk[TEMPLAR_MARSHAL], templarAttack[TEMPLAR_MARSHAL];
    Texture2D bonelandGhoul, bonelandCultist;
    Texture2D boneLordSheets[BONE_SHEET_COUNT];
    Texture2D sealMarker;
    Texture2D bossSprites, bossWalk, bossAttack, roots, rootWave, curse, guardianMelee;
    Texture2D guardianSpawn, guardianDefeat;
    Texture2D bossPortrait, cemeteryPortrait, boneLordPortrait;
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
    int villageBossDeathWeapon;
    Texture2D villageLetter;
    Texture2D villageBruteWalk, villageBruteAttack;
    Texture2D villageBossWalk, villageBossAttack, villageBossCharge;
    Texture2D villageZombie[VILLAGE_ZOMBIE_ATLAS_COUNT];
    Texture2D cemeteryZombie[3], cemeteryLantern[3], cemeterySpecial[3], cemeterySpell;
    Texture2D coveiroIdle,coveiroWalk,coveiroMelee,coveiroEmerge,coveiroFall,boss[3],effects[3];
    EnemyImpact impacts[16], templarBlood;
    CemeteryProjectile cemeteryProjectiles[CEMETERY_PROJECTILE_COUNT];
    int loadedMap;
} EnemyGroup;

bool InitEnemies(EnemyGroup *group);
bool EnsureEnemyMapAssets(EnemyGroup *group, int mapId);
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
bool IgrisHoldingPlayer(const EnemyGroup *group);
void ApplyIgrisGrab(const EnemyGroup *group, const Clearing *map, Vector2 *player, float dt);
void DamageEnemyFrom(Enemy *enemy, int damage, Vector2 source);
void KnockbackEnemy(Enemy *enemy, const Clearing *clearing,
                    Vector2 source, float distance);
void MoveEnemy(const Clearing *clearing, Enemy *enemy, Vector2 delta);
void DrawEnemyHealthBar(const Enemy *enemy, float top);
bool EnemyCanBeHit(const Enemy *enemy);
Vector2 EnemyCenter(const Enemy *enemy);
int EnemyOptionalDropChance(EnemyType type);
bool EnemyShouldDrop(EnemyType type, int roll);
int EnemyAnimationFrame(const Enemy *enemy);
void DrawEnemyDebug(const EnemyGroup *group);
typedef struct CemeteryPose { int atlas,columns,rows,row,frame; bool flip; } CemeteryPose;
enum { CEM_BOSS_WALK,CEM_BOSS_SLAM,CEM_BOSS_THROW,CEM_BOSS_CHARGE,CEM_BOSS_MELEE,
       CEM_ZOMBIE_IDLE,CEM_ZOMBIE_WALK,CEM_ZOMBIE_ATTACK,
       CEM_LANTERN_IDLE,CEM_LANTERN_WALK,CEM_LANTERN_ATTACK,
       CEM_SPECIAL_IDLE,CEM_SPECIAL_WALK,CEM_SPECIAL_ATTACK,
       CEM_BOSS_IDLE,CEM_BOSS_EMERGE,CEM_BOSS_FALL };
#define CEM_ZOMBIE_HIT_TIME .41f
#define CEM_LANTERN_RELEASE_TIME .55f
#define CEM_SPECIAL_IMPACT_TIME .52f
CemeteryPose CemeteryAnimationPose(const Enemy *enemy);
void CemeteryUpdateBoss(EnemyGroup *, const Clearing *, Enemy *, Vector2, float, bool, int *);
void CemeteryUpdateSpecial(EnemyGroup *, const Clearing *, Enemy *, Vector2, float, bool, int *);
void CemeteryUpdateProjectiles(EnemyGroup *, const Clearing *, Vector2, float, bool, int *);
void CemeteryDrawEnemies(const EnemyGroup *,Vector2,bool);
void DrawEnemyProjectiles(const EnemyGroup *);
void StartVillageHorde(EnemyGroup *group, const Clearing *clearing,
                       Vector2 player);
void VillageUpdateHorde(EnemyGroup *group, const Clearing *clearing,
                        Vector2 player, float dt);
void VillageUpdateBruteAttack(EnemyGroup *group, const Clearing *map, Enemy *brute,
                              Vector2 player, bool dodging, int *health);
void VillageUpdateBoss(EnemyGroup *group, const Clearing *map, Enemy *boss,
                       Vector2 player, float dt, bool dodging, int *playerHealth);
void VillageDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground);
#endif
