#ifndef ENCOUNTER_H
#define ENCOUNTER_H
#include "enemy.h"
#include "run_progress.h"
#include "presentation.h"
#include "logica.h"

enum { ENCOUNTER_BOSS_HUD_HEIGHT = 58 };

typedef struct EncounterSealDifficultyRow {
    int map;
    bool hasSpecialVF;
    int specialMin, specialMax;
    bool hasBossMC;
    int bossMin, bossMax;
} EncounterSealDifficultyRow;

typedef enum EncounterPhase { ENCOUNTER_FIGHT, ENCOUNTER_REWARD, ENCOUNTER_SEAL,
                             ENCOUNTER_VILLAGE_CINEMATIC, ENCOUNTER_LETTER, ENCOUNTER_ANGEL_CINEMATIC, ENCOUNTER_CHAPEL_TEXT, ENCOUNTER_BASEMENT_PILLAR,
                             ENCOUNTER_CLEAR, ENCOUNTER_DEFEAT } EncounterPhase;
typedef enum RewardRarity { RARITY_COMMON, RARITY_RARE, RARITY_EPIC, RARITY_COUNT } RewardRarity;
typedef enum RewardKind {
    REWARD_DAMAGE,
    REWARD_HEALTH,
    REWARD_SPEED,
    REWARD_SWAP,
    REWARD_WEAPON_UPGRADE,
    REWARD_EPIC_DAMAGE,
    REWARD_EPIC_STATUS,
    REWARD_EPIC_FULL_HEAL,
    REWARD_KIND_COUNT
} RewardKind;
typedef struct Reward {
    int kind;
    int rarity;
    int weapon;
    int amount;
    int bonus[2];
} Reward;
typedef struct Encounter {
    RunProgress progress;
    StagePresentation presentation;
    EncounterPhase phase;
    Reward rewards[3];
    int rewardCount, sealEnemy, answer;
    LogicSealQuestion sealQuestion;
    bool rerolled;
    bool villageHordeStarted;
    bool chapelPortalOpen;
    bool chapelAltarDone;
    bool basementSolved[4];
    int basementTarget;
    bool basementComplete;
    int chapelTextId;
    bool practice; /* Temporary test entry must not overwrite the normal run. */
    bool guardianIntroPending;
    char notice[120];
} Encounter;

void BeginEncounter(Encounter *encounter, EnemyGroup *enemies, const Clearing *clearing,
                    RunProgress progress, Vector2 player);
bool UpdateEncounter(Encounter *encounter, EnemyGroup *enemies, const Clearing *clearing,
                     Vector2 player, int *health);
bool EncounterPaused(const Encounter *encounter);
bool OpenAngelRevival(const Encounter *e, unsigned seed, LogicMCQuestion *question);
bool AnswerAngelRevival(Encounter *e, const LogicMCQuestion *question, int choice, int *health);
void CompleteAngelCinematic(Encounter *e, int health);
void CompleteVillageCinematic(Encounter *encounter);
void CompleteGuardianIntro(Encounter *encounter, bool cinematicPlayed);
bool GuardianLiberationPending(const Encounter *encounter);
bool GuardianPassagePending(const Encounter *encounter);
void CompleteGuardianLiberation(Encounter *encounter);
bool BeginGuardianPassage(Encounter *encounter, const Clearing *map, Vector2 player, bool interact);
bool CompleteGuardianPassage(Encounter *encounter, EnemyGroup *enemies, Clearing *map,
                             Vector2 *player, int health);
bool ClearingAltarOpen(const Encounter *encounter, const Clearing *clearing);
bool CanEnterCemetery(const Encounter *encounter, const Clearing *clearing,
                      Vector2 player, bool interactPressed);
bool CanEnterVillage(const Encounter *encounter, const Clearing *clearing,
                     Vector2 player, bool interactPressed);
bool CanEnterChapel(const Encounter *encounter, const Clearing *clearing,
                    Vector2 player, bool interactPressed);
bool CanEnterBasement(const Encounter *encounter, const Clearing *clearing,
                      Vector2 player, bool interactPressed);
bool CanEnterInvertedChapel(const Encounter *encounter, const Clearing *clearing,
                            Vector2 player);
bool CanEnterTemplarField(const Encounter *encounter, const Clearing *clearing,
                          Vector2 player);
bool EncounterBossVisible(const Encounter *encounter, const EnemyGroup *enemies);
float EncounterDamage(const Encounter *encounter);
bool RerollEncounterReward(Encounter *encounter, int slot);
void UnloadEncounterRewardUi(void);
bool ChooseEncounterReward(Encounter *encounter, EnemyGroup *enemies, const Clearing *clearing,
                           Vector2 player, int *health, int choice);
bool OpenEncounterSeal(Encounter *encounter, const EnemyGroup *enemies, Vector2 player);
bool AnswerEncounterSeal(Encounter *encounter, EnemyGroup *enemies, int *health, int choice);
bool EncounterSealRange(int map, bool isBoss, int *outMin, int *outMax);
int EncounterSealTableCount(void);
const EncounterSealDifficultyRow *EncounterSealTableRow(int index);
void DrawEncounter(const Encounter *encounter, const EnemyGroup *enemies);
#endif
