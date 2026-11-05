#ifndef ENCOUNTER_H
#define ENCOUNTER_H
#include "enemy.h"
#include "run_progress.h"

typedef enum EncounterPhase { ENCOUNTER_FIGHT, ENCOUNTER_REWARD, ENCOUNTER_SEAL,
                             ENCOUNTER_CLEAR, ENCOUNTER_DEFEAT } EncounterPhase;
typedef struct Reward { int kind; int weapon; int amount; } Reward;
typedef struct Encounter {
    RunProgress progress;
    EncounterPhase phase;
    Reward rewards[3];
    int rewardCount, sealEnemy, question, answer;
    bool rerolled;
    char notice[120];
} Encounter;

void BeginEncounter(Encounter *encounter, EnemyGroup *enemies, const Clearing *clearing,
                    RunProgress progress, Vector2 player);
bool UpdateEncounter(Encounter *encounter, EnemyGroup *enemies, const Clearing *clearing,
                     Vector2 player, int *health);
bool EncounterPaused(const Encounter *encounter);
float EncounterDamage(const Encounter *encounter);
bool ChooseEncounterReward(Encounter *encounter, EnemyGroup *enemies, const Clearing *clearing,
                           Vector2 player, int *health, int choice);
bool OpenEncounterSeal(Encounter *encounter, const EnemyGroup *enemies, Vector2 player);
bool AnswerEncounterSeal(Encounter *encounter, EnemyGroup *enemies, int *health, int choice);
void DrawEncounter(const Encounter *encounter, const EnemyGroup *enemies);
#endif
