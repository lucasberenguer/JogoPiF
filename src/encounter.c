#include "game_text.h"
#include "encounter.h"
#include "menu.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>

#define COMMON_DAMAGE_BONUS 0.15f
#define COMMON_HEALTH_BONUS 20
#define COMMON_SPEED_BONUS 15.0f
#define EPIC_STATUS_DAMAGE_BONUS 0.25f
#define EPIC_STATUS_SPEED_BONUS 60.0f
#define EPIC_STATUS_MIN_TENS 3
#define EPIC_STATUS_MAX_TENS 6
#define EPIC_FULL_HEAL_MAX_HP_BONUS 50
#define RARITY_WEIGHT_COMMON 70
#define RARITY_WEIGHT_RARE 25
#define RARITY_WEIGHT_EPIC 5
#define MAX_CAMPAIGN_MAPS 9
#define UI_CARD_FRAME_COMMON_PATH "assets/ui/card_frame_common.png"
#define UI_CARD_FRAME_RARE_PATH "assets/ui/card_frame_rare.png"
#define UI_CARD_FRAME_EPIC_PATH "assets/ui/card_frame_epic.png"
#define UI_REROLL_BUTTON_PATH "assets/ui/reroll_button.png"
#define UI_CARD_NPATCH_SLICE 16

static const char *const cardFramePaths[RARITY_COUNT] = {
    UI_CARD_FRAME_COMMON_PATH,
    UI_CARD_FRAME_RARE_PATH,
    UI_CARD_FRAME_EPIC_PATH
};

typedef struct RewardUiAssets {
    bool attempted;
    Texture2D frames[RARITY_COUNT];
    Texture2D rerollButton;
} RewardUiAssets;

static RewardUiAssets rewardUiAssets = {0};

static void EnsureRewardUiAssetsLoaded(void) {
    if (rewardUiAssets.attempted || !IsWindowReady()) return;
    rewardUiAssets.attempted = true;
    for (int i = 0; i < RARITY_COUNT; i++) {
        if (FileExists(cardFramePaths[i])) {
            rewardUiAssets.frames[i] = LoadTexture(cardFramePaths[i]);
            if (rewardUiAssets.frames[i].id)
                SetTextureFilter(rewardUiAssets.frames[i], TEXTURE_FILTER_POINT);
        }
    }
    if (FileExists(UI_REROLL_BUTTON_PATH)) {
        rewardUiAssets.rerollButton = LoadTexture(UI_REROLL_BUTTON_PATH);
        if (rewardUiAssets.rerollButton.id)
            SetTextureFilter(rewardUiAssets.rerollButton, TEXTURE_FILTER_POINT);
    }
}

static const int mapRarityWeights[MAX_CAMPAIGN_MAPS][RARITY_COUNT] = {
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
    {RARITY_WEIGHT_COMMON, RARITY_WEIGHT_RARE, RARITY_WEIGHT_EPIC},
};
static const char boneLordName[] = "Rasga-Ossos";
typedef struct SealQuestion {
    const char *expression;
    const char *choices[3]; /* correct first; the slot is shuffled */
} SealQuestion;

static const SealQuestion bossQuestions[] = {
    {"P e (Q ou nao Q)", {"P", "nao P", "Q"}},
    {"nao (P ou Q)", {"nao P e nao Q", "nao P ou nao Q", "P e Q"}},
    {"(P e Q) ou (nao P e Q)", {"Q", "P", "P ou Q"}},
    {"nao (P e Q)", {"nao P ou nao Q", "nao P e nao Q", "P ou Q"}},
};

static const SealQuestion finalQuestions[] = {
    {"(P ou Q) e (P ou não Q)", {"P", "Q", "P ou Q"}},
    {"não (P e (Q ou R))", {"não P ou (não Q e não R)", "não P e (não Q ou não R)", "não P ou não Q ou não R"}},
    {"(P e Q) ou (P e não Q) ou (não P e R)", {"P ou R", "P e R", "Q ou R"}},
    {"(P e Q) ou (não P e não Q)", {"P e Q têm o mesmo valor", "P e Q têm valores diferentes", "P ou Q"}},
};

static const char *WeaponTitle(int weapon) {
    return weapon == 0 ? "Arma de fogo" : weapon == 1 ? "Biblia" : "Katana";
}

static void StartStage(Encounter *e, EnemyGroup *enemies, const Clearing *clearing, Vector2 player) {
    if (e->progress.stage == CLEARING_COMPLETE_STAGE) {
        for (int i=0; i<ENEMY_COUNT; i++) enemies->list[i] = (Enemy){.state=ENEMY_GONE};
        ClearEnemyAttacks(enemies);
        if (clearing->necromancerArena) enemies->list[0].necromancerForm=3;
        e->phase = ENCOUNTER_CLEAR;
    } else {
        e->phase = ENCOUNTER_FIGHT;
        const char *areaTitle = clearing->necromancerArena ? "Topo do Palácio" :
            clearing->templarField ? "Campo de Batalha dos Templários"
            : clearing->boneland ? "Terra dos Ossos" : clearing->invertedChapel ? "Capela Invertida"
            : (clearing->basement ? "Porão da Capela"
            : (clearing->chapel ? "Capela do Anjo"
            : (clearing->village ? "Chã da Tristeza"
            : (clearing->cemetery ? "Cemitério das Raízes" : "Clareira do Selo"))));
        const char *bossTitle = clearing->necromancerArena ? "Necromante" :
            clearing->templarField ? "Marechal Carmesim"
            : clearing->boneland ? boneLordName : (clearing->village || clearing->chapel || clearing->basement || clearing->invertedChapel) ? NULL
            : (clearing->cemetery ? "Coveiro das Raízes" : "Guardião da Clareira");
        int wave = e->progress.stage;
        if (clearing->necromancerArena || clearing->village || clearing->chapel || clearing->basement || clearing->invertedChapel)
            wave = 0;
        else if ((clearing->boneland || clearing->templarField) && wave == CLEARING_BOSS_STAGE)
            wave = (clearing->boneland ? BONELAND_WAVES : TEMPLAR_FIELD_WAVES) + 1;
        BeginStagePresentation(&e->presentation, wave, areaTitle,
            e->progress.stage==CLEARING_BOSS_STAGE ? bossTitle : NULL);
        if (clearing->necromancerArena) {
            e->presentation.areaSubtitle = "O Grande Selo Profano";
            e->presentation.portraitCenterX = .72f;
        }
        if (clearing->boneland) {
            e->presentation.waveTotal = BONELAND_WAVES + 1;
            e->presentation.portraitCenterX = .74f;
        }
        if (clearing->templarField) {
            e->presentation.waveTotal = TEMPLAR_FIELD_WAVES + 1;
            e->presentation.areaSubtitle = "A última defesa do palácio";
            if (wave <= TEMPLAR_FIELD_WAVES)
                e->presentation.announcement = wave == 1 ? "A Linha de Frente" : "O Último Estandarte";
            else
                ShowWaveBeforeBoss(&e->presentation);
        }
        if (clearing->cemetery || clearing->village) e->presentation.portraitCenterX=.73f;
        e->guardianIntroPending=e->progress.map == 0 &&
                                e->progress.stage==CLEARING_BOSS_STAGE &&
                                !e->progress.guardianIntroSeen;
        if (e->progress.stage == CLEARING_BOSS_STAGE) StartArenaBoss(enemies, clearing, player, e->progress.bossPower);
        else StartEnemyWave(enemies, clearing, e->progress.stage, player, e->progress.damage);
    }
}

void BeginEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                    RunProgress progress, Vector2 player) {
    progress.map = ClearingMapId(clearing);
    if (progress.map > 0) progress.guardianAftermath = GUARDIAN_PASSAGE_COMPLETE;
    if ((clearing->village || clearing->chapel || clearing->basement || clearing->invertedChapel) && progress.stage < CLEARING_BOSS_STAGE)
        progress.stage = CLEARING_BOSS_STAGE;
    *e = (Encounter){.progress=progress, .sealEnemy=-1};
    e->chapelAltarDone=progress.angelSceneCompleted;
    StartStage(e, enemies, clearing, player);
    if (clearing->village && progress.stage==CLEARING_BOSS_STAGE && progress.villageAftermath!=VILLAGE_DUEL) {
        enemies->list[0].health=0;
        enemies->list[0].state=ENEMY_FALLEN;
        e->phase=progress.villageAftermath==VILLAGE_SCENE_PENDING ? ENCOUNTER_VILLAGE_CINEMATIC : ENCOUNTER_LETTER;
        e->presentation=(StagePresentation){0};
    } else BeginAreaEntry(&e->presentation);
}

bool EncounterPaused(const Encounter *e) {
    return e->guardianIntroPending || GuardianLiberationPending(e) || GuardianPassagePending(e) ||
           e->presentation.remaining>0 || e->phase == ENCOUNTER_REWARD ||
           e->phase == ENCOUNTER_SEAL || e->phase == ENCOUNTER_VILLAGE_CINEMATIC || e->phase == ENCOUNTER_LETTER ||
           e->phase == ENCOUNTER_ANGEL_CINEMATIC || e->phase == ENCOUNTER_CHAPEL_TEXT || e->phase == ENCOUNTER_BASEMENT_PILLAR ||
           e->phase == ENCOUNTER_DEFEAT;
}

void CompleteAngelCinematic(Encounter *e, int health) {
    if (e->phase!=ENCOUNTER_ANGEL_CINEMATIC || e->progress.angelSceneCompleted) return;
    e->chapelAltarDone=true;
    e->progress.angelSceneCompleted=true;
    e->progress.angelBlessingUses=3;
    e->progress.health=health;
    e->phase=ENCOUNTER_FIGHT;
    snprintf(e->notice,sizeof(e->notice),"Bênção recebida. A passagem à esquerda está aberta.");
    if (!e->practice) SaveClearingCheckpoint(&e->progress);
}

void CompleteVillageCinematic(Encounter *e) {
    if (e->phase!=ENCOUNTER_VILLAGE_CINEMATIC) return;
    e->progress.villageAftermath=VILLAGE_SCENE_SEEN;
    e->phase=ENCOUNTER_LETTER;
    if (!e->practice) SaveClearingCheckpoint(&e->progress);
}

void CompleteGuardianIntro(Encounter *e, bool cinematicPlayed) {
    if (!e->guardianIntroPending) return;
    e->guardianIntroPending=false;
    if (cinematicPlayed) e->presentation=(StagePresentation){0};
    if (e->progress.map==1) return;
    e->progress.guardianIntroSeen=true;
    if (!e->practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING,"Nao foi possivel salvar a conclusao da cutscene do Guardiao.");
}

bool ClearingAltarOpen(const Encounter *e, const Clearing *clearing) {
    return e->progress.map == 0 && !clearing->cemetery &&
           e->progress.guardianAftermath>=GUARDIAN_ALTAR_READY && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE;
}

bool GuardianLiberationPending(const Encounter *e) {
    return e->progress.map==0 && e->progress.stage==CLEARING_COMPLETE_STAGE &&
           e->progress.guardianAftermath==GUARDIAN_LIBERATION_PENDING;
}

bool GuardianPassagePending(const Encounter *e) {
    return e->progress.map==0 && e->progress.stage==CLEARING_COMPLETE_STAGE &&
           e->progress.guardianAftermath==GUARDIAN_PASSAGE_PENDING;
}

static void SaveGuardianProgress(const Encounter *e) {
    if (!e->practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING,"Nao foi possivel salvar o progresso da purificacao.");
}

void CompleteGuardianLiberation(Encounter *e) {
    if (!GuardianLiberationPending(e)) return;
    e->progress.guardianAftermath=GUARDIAN_ALTAR_READY;
    e->presentation=(StagePresentation){0};
    snprintf(e->notice,sizeof(e->notice),"Investigue o altar da clareira");
    SaveGuardianProgress(e);
}

bool BeginGuardianPassage(Encounter *e, const Clearing *map, Vector2 player, bool interact) {
    if (e->progress.guardianAftermath!=GUARDIAN_ALTAR_READY ||
        !CanEnterCemetery(e,map,player,interact)) return false;
    e->progress.guardianAftermath=GUARDIAN_PASSAGE_PENDING;
    SaveGuardianProgress(e);
    return true;
}

bool CompleteGuardianPassage(Encounter *e, EnemyGroup *enemies, Clearing *map,
                             Vector2 *player, int health) {
    if (!GuardianPassagePending(e)) return false;
    Clearing next={0};
    if (!InitCemetery(&next)) {
        UnloadClearing(&next);
        e->progress.guardianAftermath=GUARDIAN_ALTAR_READY;
        snprintf(e->notice,sizeof(e->notice),"Passagem indisponível. Interaja com o altar para tentar novamente.");
        SaveGuardianProgress(e);
        return false;
    }
    RunProgress progress=e->progress;
    progress.map=1;
    progress.stage=1;
    progress.health=health;
    progress.bossPower=1;
    progress.guardianAftermath=GUARDIAN_PASSAGE_COMPLETE;
    bool practice=e->practice;
    UnloadClearing(map);
    *map=next;
    *player=map->spawn;
    BeginEncounter(e,enemies,map,progress,*player);
    e->practice=practice;
    SaveGuardianProgress(e);
    return true;
}

bool CanEnterCemetery(const Encounter *e, const Clearing *clearing,
                      Vector2 player, bool interactPressed) {
    return interactPressed && ClearingAltarOpen(e, clearing) && !EncounterPaused(e) &&
           CheckCollisionPointCircle(player, clearing->pedestal, CLEARING_ALTAR_RADIUS);
}

bool CanEnterVillage(const Encounter *e, const Clearing *clearing,
                     Vector2 player, bool interactPressed) {
    (void)interactPressed;
    return clearing->cemetery && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

bool CanEnterChapel(const Encounter *e, const Clearing *clearing,
                    Vector2 player, bool interactPressed) {
    (void)interactPressed;
    return clearing->village && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

bool CanEnterBasement(const Encounter *e, const Clearing *clearing,
                      Vector2 player, bool interactPressed) {
    (void)interactPressed;
    return clearing->chapel && e->chapelAltarDone && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

bool CanEnterInvertedChapel(const Encounter *e, const Clearing *clearing,
                            Vector2 player) {
    return clearing->basement && e->basementComplete && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

bool CanEnterTemplarField(const Encounter *e, const Clearing *clearing, Vector2 player) {
    return clearing->boneland && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

float EncounterDamage(const Encounter *e) {
    int weapon = e->progress.weapon;
    int level = (weapon >= 0 && weapon < 3) ? e->progress.weaponLevel[weapon] : 0;
    float weaponBonus = 0.0f;
    if (weapon == 1 && level >= 1) weaponBonus = BIBLE_L1_DAMAGE_BONUS;
    else if (weapon == 0 && level >= 2) weaponBonus = GUN_L2_DAMAGE_BONUS;
    return e->progress.damage * (1.0f + WEAPON_LEVEL_DAMAGE_BONUS * level +
                                 EPIC_DAMAGE_STACK_BONUS * e->progress.epicDamageStacks +
                                 weaponBonus);
}

bool EncounterBossVisible(const Encounter *e, const EnemyGroup *enemies) {
    return !e->guardianIntroPending && e->presentation.remaining <= 0 && e->progress.stage == CLEARING_BOSS_STAGE &&
           enemies->list[0].type == ENEMY_BOSS && enemies->list[0].health > 0;
}

static int CurrentRunWeaponLevel(const Encounter *e) {
    int w = e->progress.weapon;
    return (w >= 0 && w < 3) ? e->progress.weaponLevel[w] : e->progress.weaponLevel[0];
}

static int AvailableKindsForRarity(const Encounter *e, int rarity, const bool used[REWARD_KIND_COUNT],
                                   int outKinds[4]) {
    int count = 0;
    if (rarity == RARITY_COMMON) {
        static const int commonKinds[3] = {REWARD_DAMAGE, REWARD_HEALTH, REWARD_SPEED};
        for (int i = 0; i < 3; i++)
            if (!used[commonKinds[i]]) outKinds[count++] = commonKinds[i];
    } else if (rarity == RARITY_RARE) {
        if (!used[REWARD_SWAP]) outKinds[count++] = REWARD_SWAP;
    } else if (rarity == RARITY_EPIC) {
        if (CurrentRunWeaponLevel(e) < MAX_WEAPON_LEVEL && !used[REWARD_WEAPON_UPGRADE])
            outKinds[count++] = REWARD_WEAPON_UPGRADE;
        if (!used[REWARD_EPIC_DAMAGE]) outKinds[count++] = REWARD_EPIC_DAMAGE;
        if (!used[REWARD_EPIC_STATUS]) outKinds[count++] = REWARD_EPIC_STATUS;
        if (!used[REWARD_EPIC_FULL_HEAL]) outKinds[count++] = REWARD_EPIC_FULL_HEAL;
    }
    return count;
}

static Reward MakeReward(const Encounter *e, int rarity, int kind) {
    Reward r = {.kind = kind, .rarity = rarity, .weapon = e->progress.weapon};
    if (kind == REWARD_SWAP) {
        r.weapon = (e->progress.weapon + GetRandomValue(1, 2)) % 3;
        int first = GetRandomValue(0, 2);
        int second = (first + GetRandomValue(1, 2)) % 3;
        r.bonus[0] = first;
        r.bonus[1] = second;
    } else if (kind == REWARD_EPIC_STATUS) {
        r.amount = GetRandomValue(EPIC_STATUS_MIN_TENS, EPIC_STATUS_MAX_TENS) * 10;
    }
    return r;
}

static Reward RollSingleNormalReward(const Encounter *e, const bool used[REWARD_KIND_COUNT]) {
    int mapIndex = (e->progress.map >= 0 && e->progress.map < MAX_CAMPAIGN_MAPS) ? e->progress.map : 0;
    int availCount[RARITY_COUNT] = {0};
    int availKinds[RARITY_COUNT][4] = {{0}};
    int totalWeight = 0;
    for (int r = 0; r < RARITY_COUNT; r++) {
        availCount[r] = AvailableKindsForRarity(e, r, used, availKinds[r]);
        if (availCount[r] > 0) totalWeight += mapRarityWeights[mapIndex][r];
    }
    int chosenRarity = RARITY_COMMON;
    if (totalWeight > 0) {
        int roll = GetRandomValue(0, totalWeight - 1);
        int acc = 0;
        for (int r = 0; r < RARITY_COUNT; r++) {
            if (availCount[r] <= 0) continue;
            acc += mapRarityWeights[mapIndex][r];
            if (roll < acc) {
                chosenRarity = r;
                break;
            }
        }
    }
    int kindIndex = availCount[chosenRarity] > 1 ? GetRandomValue(0, availCount[chosenRarity] - 1) : 0;
    return MakeReward(e, chosenRarity, availKinds[chosenRarity][kindIndex]);
}

static void RollRewards(Encounter *e) {
    e->rewardCount=3;
    if (e->progress.stage == CLEARING_WAVES) {
        int epicPool[4] = {0};
        int epicCount = 0;
        if (CurrentRunWeaponLevel(e) < MAX_WEAPON_LEVEL)
            epicPool[epicCount++] = REWARD_WEAPON_UPGRADE;
        epicPool[epicCount++] = REWARD_EPIC_DAMAGE;
        epicPool[epicCount++] = REWARD_EPIC_STATUS;
        epicPool[epicCount++] = REWARD_EPIC_FULL_HEAL;
        for (int i = 0; i < 3; i++) {
            int pick = i + (epicCount - i > 1 ? GetRandomValue(0, epicCount - 1 - i) : 0);
            int kind = epicPool[pick];
            epicPool[pick] = epicPool[i];
            epicPool[i] = kind;
            e->rewards[i] = MakeReward(e, RARITY_EPIC, kind);
        }
        return;
    }
    bool used[REWARD_KIND_COUNT]={false};
    for (int i=0; i<3; i++) {
        e->rewards[i]=RollSingleNormalReward(e,used);
        used[e->rewards[i].kind]=true;
    }
}

static Rectangle ChoiceRect(int index, int count) {
    float w=fminf(330, (GetScreenWidth()-80.0f)/count-16);
    return (Rectangle){GetScreenWidth()/2.0f-(count*w+(count-1)*16)/2+index*(w+16),
                       GetScreenHeight()/2.0f-30,w,140};
}

static Rectangle RerollButtonRect(int index, int count) {
    Rectangle card = ChoiceRect(index, count);
    float bw = fminf(150.0f, card.width - 20.0f);
    return (Rectangle){card.x + (card.width - bw) * 0.5f, card.y + card.height + 10.0f, bw, 34.0f};
}

bool RerollEncounterReward(Encounter *e, int slot) {
    if (e->phase != ENCOUNTER_REWARD || e->progress.stage == CLEARING_WAVES ||
        e->rerolled || slot < 0 || slot >= e->rewardCount) return false;
    bool used[REWARD_KIND_COUNT] = {false};
    for (int i = 0; i < e->rewardCount; i++) {
        if (e->rewards[i].kind >= 0 && e->rewards[i].kind < REWARD_KIND_COUNT)
            used[e->rewards[i].kind] = true;
    }
    e->rewards[slot] = RollSingleNormalReward(e, used);
    e->rerolled = true;
    return true;
}

static int ChoiceInput(int count) {
    for (int i=0; i<count; i++) {
        if (IsKeyPressed(KEY_ONE+i)) return i;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(),ChoiceRect(i,count))) return i;
    }
    return -1;
}

static void ApplyCommonBuffKind(Encounter *e, int kind, int *health) {
    if (kind == REWARD_DAMAGE) {
        e->progress.damage += COMMON_DAMAGE_BONUS;
    } else if (kind == REWARD_HEALTH) {
        e->progress.maxHealth += COMMON_HEALTH_BONUS;
        *health += COMMON_HEALTH_BONUS;
    } else if (kind == REWARD_SPEED) {
        e->progress.speed += COMMON_SPEED_BONUS;
    }
}

static void ApplyReward(Encounter *e, Reward reward, int *health) {
    e->notice[0]=0;
    switch (reward.kind) {
        case REWARD_DAMAGE:
        case REWARD_HEALTH:
        case REWARD_SPEED:
            ApplyCommonBuffKind(e, reward.kind, health);
            break;
        case REWARD_SWAP: {
            int currentLevel = CurrentRunWeaponLevel(e);
            e->progress.weapon = reward.weapon;
            SetRunWeaponLevel(&e->progress, currentLevel);
            ApplyCommonBuffKind(e, reward.bonus[0], health);
            ApplyCommonBuffKind(e, reward.bonus[1], health);
            break;
        }
        case REWARD_WEAPON_UPGRADE: {
            int currentLevel = CurrentRunWeaponLevel(e);
            if (currentLevel < MAX_WEAPON_LEVEL)
                SetRunWeaponLevel(&e->progress, currentLevel + 1);
            break;
        }
        case REWARD_EPIC_DAMAGE:
            e->progress.epicDamageStacks++;
            break;
        case REWARD_EPIC_STATUS: {
            int added = reward.amount > 0 ? reward.amount : EPIC_STATUS_MIN_TENS * 10;
            e->progress.maxHealth += added;
            *health += added;
            e->progress.damage += EPIC_STATUS_DAMAGE_BONUS;
            e->progress.speed += EPIC_STATUS_SPEED_BONUS;
            break;
        }
        case REWARD_EPIC_FULL_HEAL:
            e->progress.maxHealth += EPIC_FULL_HEAL_MAX_HP_BONUS;
            *health = e->progress.maxHealth;
            break;
    }
}

bool ChooseEncounterReward(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                           Vector2 player, int *health, int choice) {
    if (e->phase!=ENCOUNTER_REWARD || choice<0 || choice>=e->rewardCount) return false;
    ApplyReward(e,e->rewards[choice],health);
    bool lastWave = (clearing->boneland && e->progress.stage == BONELAND_WAVES) ||
                    (clearing->templarField && e->progress.stage == TEMPLAR_FIELD_WAVES);
    e->progress.stage = lastWave ? CLEARING_BOSS_STAGE : e->progress.stage + 1;
    e->progress.health=*health;
    if (!e->practice) SaveClearingCheckpoint(&e->progress);
    StartStage(e,enemies,clearing,player);
    if (clearing->cemetery && e->progress.stage==CLEARING_BOSS_STAGE)
        e->guardianIntroPending=true;
    return true;
}

bool OpenEncounterSeal(Encounter *e, const EnemyGroup *enemies, Vector2 player) {
    if (e->phase!=ENCOUNTER_FIGHT || e->presentation.remaining>0) return false;
    for (int i=0;i<ENEMY_COUNT;i++) if (enemies->list[i].state==ENEMY_FALLEN &&
        Vector2Distance(enemies->list[i].position,player)<95) {
        e->sealEnemy=i;
        e->question=e->progress.map==NECROMANCER_MAP ?
            e->progress.finalSealFailures % 4 : GetRandomValue(0,3);
        e->answer=GetRandomValue(0,2);
        e->phase=ENCOUNTER_SEAL;
        return true;
    }
    return false;
}

bool AnswerEncounterSeal(Encounter *e, EnemyGroup *enemies, int *health, int choice) {
    if (e->phase!=ENCOUNTER_SEAL || e->sealEnemy<0 || e->sealEnemy>=ENEMY_COUNT) return false;
    Enemy *enemy=&enemies->list[e->sealEnemy];
    int count=enemy->type==ENEMY_BOSS?3:2;
    if (choice<0 || choice>=count) return false;
    int correct=enemy->type==ENEMY_BOSS?e->answer:(e->question==0 || e->question==3?0:1);
    ClearEnemyAttacks(enemies);
    if (e->progress.map==NECROMANCER_MAP) {
        if (choice!=correct) e->progress.finalSealFailures++;
        bool finalAttempt=e->progress.finalSealFailures>=3;
        bool savedByAngel=choice!=correct && finalAttempt && e->progress.angelBlessingUses>0;
        if (savedByAngel) e->progress.angelBlessingUses--;
        if (choice==correct || savedByAngel) {
            enemy->state=ENEMY_GONE;
            e->progress.stage=CLEARING_COMPLETE_STAGE;
            e->progress.finalSealFailures=0;
            e->progress.health=*health;
            e->phase=ENCOUNTER_CLEAR;
            BeginStagePresentation(&e->presentation,0,
                savedByAngel?"O Anjo completou o selo final":"O Grande Selo Profano foi restaurado",NULL);
            e->presentation.announcement="O Necromante foi selado";
            e->presentation.duration=e->presentation.remaining=3.5f;
            if (!e->practice) SaveClearingCheckpoint(&e->progress);
        } else if (finalAttempt) {
            int startWeapon = (e->progress.initialWeapon >= 0 && e->progress.initialWeapon < 3)
                ? e->progress.initialWeapon : e->progress.weapon;
            if (!e->practice) SaveMenuCheckpoint(startWeapon);
            e->phase=ENCOUNTER_DEFEAT;
            *health=0;
        } else {
            e->progress.bossPower=fminf(100,e->progress.bossPower*1.2f);
            e->progress.health=*health;
            StartNecromancer(enemies,e->progress.bossPower);
            e->phase=ENCOUNTER_FIGHT;
            snprintf(e->notice,sizeof(e->notice),"Selo incorreto. O Necromante voltou mais forte.");
            if (!e->practice) SaveClearingCheckpoint(&e->progress);
        }
        return true;
    }
    bool protectedSeal=choice!=correct && e->progress.angelBlessingUses>0 &&
        *health<=e->progress.maxHealth/3;
    if (protectedSeal) {
        e->progress.angelBlessingUses--;
        e->progress.health=*health;
        snprintf(e->notice,sizeof(e->notice),"A bênção protegeu o selo. Restam %d usos.",e->progress.angelBlessingUses);
        if (!e->practice) SaveClearingCheckpoint(&e->progress);
    }
    if (choice==correct || protectedSeal) {
        enemy->state=ENEMY_GONE;
        if (enemy->type==ENEMY_BOSS && e->progress.map==0 &&
            e->progress.stage==CLEARING_BOSS_STAGE) {
            e->progress.guardianAftermath=GUARDIAN_LIBERATION_PENDING;
            e->progress.guardianIntroSeen=true;
            e->guardianIntroPending=false;
            e->progress.stage=CLEARING_COMPLETE_STAGE;
            e->progress.health=*health;
            e->phase=ENCOUNTER_CLEAR;
            e->presentation=(StagePresentation){0};
            e->notice[0]=0;
            SaveGuardianProgress(e);
            return true;
        }
    }
    else {
        enemy->maxHealth=(int)fminf(65000,enemy->maxHealth*1.2f);
        enemy->health=enemy->maxHealth;
        enemy->power*=1.15f;
        enemy->state=ENEMY_IDLE; enemy->stateTime=0; enemy->attackCooldown=1;
        snprintf(e->notice,sizeof(e->notice),"Selo incorreto: o inimigo voltou mais forte");
        if (enemy->type==ENEMY_BOSS) {
            enemy->guardianPhase = GUARDIAN_PHASE_ONE;
            enemy->guardianMelee = false;
            enemy->guardianMeleeCooldown = 0;
            enemy->attackBurst = 0;
            enemy->bossAttack = enemy->templarRole == TEMPLAR_MARSHAL ? MARSHAL_SLASH :
                enemies->boneland ? BONE_DASH :
                (enemies->cemetery ? COVEIRO_ATTACK_COUNT : GUARDIAN_ATTACK_COUNT) - 1;
            if (enemy->templarRole == TEMPLAR_MARSHAL) enemy->attackCooldown = .25f;
            e->progress.bossPower=fminf(100,e->progress.bossPower*1.2f);
            e->progress.health=*health;
            if (!e->practice) SaveClearingCheckpoint(&e->progress);
        }
    }
    e->phase=ENCOUNTER_FIGHT;
    return true;
}

bool UpdateEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                     Vector2 player, int *health) {
    if (*health<=0) { e->phase=ENCOUNTER_DEFEAT; return false; }
    if (e->guardianIntroPending || GuardianLiberationPending(e) || GuardianPassagePending(e) ||
        e->presentation.remaining>0) return false;
    if (e->progress.map == 3) {
        if (e->phase == ENCOUNTER_CHAPEL_TEXT) {
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                e->phase = ENCOUNTER_FIGHT;
            }
            return false;
        }
        if (IsKeyPressed(KEY_E)) {
            float w = (float)clearing->background.width, h = (float)clearing->background.height;
            if (Vector2Distance(player, (Vector2){w * 0.50f, h * 0.44f}) <= 125.0f) {
                if (!e->chapelAltarDone) e->phase=ENCOUNTER_ANGEL_CINEMATIC;
            } else if (Vector2Distance(player, (Vector2){w * 0.365f, h * 0.26f}) <= 95.0f) {
                e->chapelTextId = 1; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.63f, h * 0.26f}) <= 95.0f) {
                e->chapelTextId = 2; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.85f, h * 0.35f}) <= 115.0f) {
                e->chapelTextId = 3; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.22f, h * 0.34f}) <= 95.0f) {
                e->chapelTextId = 4; e->phase = ENCOUNTER_CHAPEL_TEXT;
            }
        }
        return false;
    }
    if (e->progress.map == 4) {
        if (e->phase == ENCOUNTER_CHAPEL_TEXT) {
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                e->phase = ENCOUNTER_FIGHT;
            }
            return false;
        }
        if (e->phase == ENCOUNTER_BASEMENT_PILLAR) {
            if (IsKeyPressed(KEY_ESCAPE)) { e->phase = ENCOUNTER_FIGHT; return false; }
            int sel = ChoiceInput(2);
            if (sel >= 0) {
                static const int solution[4] = {0, 1, 0, 1}; /* NW=V(0), NE=F(1), SW=V(0), SE=F(1) */
                int t = e->basementTarget;
                if (sel == solution[t]) {
                    e->basementSolved[t] = true;
                    snprintf(e->notice, sizeof(e->notice), "Pilar ativado! Parte do Último Selo acendeu no chão.");
                    if (e->basementSolved[0] && e->basementSolved[1] &&
                        e->basementSolved[2] && e->basementSolved[3]) {
                        e->basementComplete = true;
                        e->phase = ENCOUNTER_CLEAR;
                        e->progress.stage = CLEARING_COMPLETE_STAGE;
                        e->notice[0] = 0;
                        BeginStagePresentation(&e->presentation, 0,
                            "Suba pela escadaria ao norte para atravessar ao Mundo Invertido", NULL);
                        e->presentation.announcement = "ÚLTIMO SELO COMPLETO";
                        e->presentation.duration = e->presentation.remaining = 2.6f;
                        return true;
                    }
                } else {
                    snprintf(e->notice, sizeof(e->notice), "As runas rejeitam essa escolha — revise as 4 placas.");
                }
                e->phase = ENCOUNTER_FIGHT;
            }
            return false;
        }
        if (IsKeyPressed(KEY_E) && !e->basementComplete) {
            float w = (float)clearing->background.width, h = (float)clearing->background.height;
            static const float tabletX[4] = {0.08f, 0.22f, 0.78f, 0.91f};
            for (int i = 0; i < 4; i++) {
                if (Vector2Distance(player, (Vector2){w * tabletX[i], h * 0.28f}) <= 90.0f) {
                    e->chapelTextId = 5 + i;
                    e->phase = ENCOUNTER_CHAPEL_TEXT;
                    return false;
                }
            }
            static const float pillarPos[4][2] = {
                {0.218f, 0.395f}, {0.782f, 0.395f}, {0.218f, 0.795f}, {0.782f, 0.795f}
            };
            for (int i = 0; i < 4; i++) {
                if (!e->basementSolved[i] &&
                    Vector2Distance(player, (Vector2){w * pillarPos[i][0], h * pillarPos[i][1]}) <= 115.0f) {
                    e->basementTarget = i;
                    e->phase = ENCOUNTER_BASEMENT_PILLAR;
                    return false;
                }
            }
        }
        return false;
    }
    if (e->progress.map == 5) {
        if (e->phase == ENCOUNTER_CHAPEL_TEXT) {
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                e->phase = ENCOUNTER_FIGHT;
            }
            return false;
        }
        if (IsKeyPressed(KEY_E)) {
            float w = (float)clearing->background.width, h = (float)clearing->background.height;
            if (Vector2Distance(player, (Vector2){w * 0.50f, h * 0.44f}) <= 125.0f) {
                e->chapelTextId = 9; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.365f, h * 0.26f}) <= 95.0f) {
                e->chapelTextId = 10; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.63f, h * 0.26f}) <= 95.0f) {
                e->chapelTextId = 11; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.85f, h * 0.35f}) <= 115.0f) {
                e->chapelTextId = 12; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.22f, h * 0.34f}) <= 95.0f) {
                e->chapelTextId = 13; e->phase = ENCOUNTER_CHAPEL_TEXT;
            }
        }
        return false;
    }
    if (e->phase==ENCOUNTER_FIGHT) {
        if (e->progress.map == 2 && !e->villageHordeStarted &&
            enemies->list[0].type == ENEMY_BOSS && enemies->list[0].health<=0) {
            ClearEnemyAttacks(enemies);
            enemies->list[0].state=ENEMY_FALLEN;
            e->progress.villageAftermath=VILLAGE_SCENE_PENDING;
            e->progress.health=*health;
            e->phase=ENCOUNTER_VILLAGE_CINEMATIC;
            if (!e->practice) SaveClearingCheckpoint(&e->progress);
            return false;
        }
        if (EnemiesRemaining(enemies)==0) {
            ClearEnemyAttacks(enemies);
            if (e->progress.stage==CLEARING_BOSS_STAGE) {
                e->progress.stage=CLEARING_COMPLETE_STAGE;
                e->progress.health=*health;
                e->phase=ENCOUNTER_CLEAR;
                e->notice[0]=0;
                const char *sub = clearing->templarField ? "A escadaria está livre"
                    : clearing->boneland ? "Siga pela passagem ao norte" : e->progress.map == 2
                    ? "Ultimo registro: \"Sobrevivi, mas falhei em salva-los...\""
                    : (e->progress.map == 1 ? "A saída à direita leva a Chã da Tristeza"
                                            : "O altar agora leva ao cemiterio");
                BeginStagePresentation(&e->presentation, 0, sub, NULL);
                e->presentation.announcement = clearing->templarField ? "Marechal Carmesim derrotado"
                    : clearing->boneland ? "Terra dos Ossos limpa" : e->progress.map == 2 ? "CHÃ DA TRISTEZA LIMPA"
                    : (e->progress.map == 1 ? "CEMITERIO LIMPO" : "CLAREIRA LIMPA");
                e->presentation.duration=e->presentation.remaining=2.5f;
                if (clearing->boneland) QueueAreaUnlock(&e->presentation, "Campo de Batalha dos Templários");
                else if (!clearing->templarField) QueueAreaUnlock(&e->presentation, e->progress.map == 2 ? "Capela do Anjo"
                    : (e->progress.map == 1 ? "Chã da Tristeza" : "Cemitério das Raízes"));
                if (!e->practice) SaveClearingCheckpoint(&e->progress);
            } else {
                e->phase=ENCOUNTER_REWARD; e->rerolled=false; RollRewards(e);
            }
        } else if (IsKeyPressed(KEY_E) && !(e->progress.map == 2 && !e->villageHordeStarted)) {
            OpenEncounterSeal(e,enemies,player);
        }
    } else if (e->phase==ENCOUNTER_LETTER) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            e->villageHordeStarted = true;
            e->phase = ENCOUNTER_FIGHT;
            StartVillageHorde(enemies, clearing, player, e->progress.damage);
            BeginStagePresentation(&e->presentation, 0, "Os aldeoes mortos erguem-se da terra!", NULL);
            e->presentation.announcement = "A MALDICAO DESPERTA";
            e->presentation.duration = e->presentation.remaining = 2.2f;
            return true;
        }
    } else if (e->phase==ENCOUNTER_REWARD) {
        if (e->progress.stage != CLEARING_WAVES && !e->rerolled &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            for (int i = 0; i < e->rewardCount; i++) {
                if (CheckCollisionPointRec(mouse, RerollButtonRect(i, e->rewardCount))) {
                    RerollEncounterReward(e, i);
                    return false;
                }
            }
        }
        int selected=ChoiceInput(e->rewardCount);
        if (selected>=0) {
            return ChooseEncounterReward(e,enemies,clearing,player,health,selected);
        }
    } else if (e->phase==ENCOUNTER_SEAL) {
        Enemy *enemy=&enemies->list[e->sealEnemy];
        int selected=ChoiceInput(enemy->type==ENEMY_BOSS?3:2);
        if (selected>=0) return AnswerEncounterSeal(e,enemies,health,selected);
    }
    return false;
}

static const char *CommonBuffShortText(int kind) {
    if (kind == REWARD_DAMAGE) return "Dano +15%";
    if (kind == REWARD_HEALTH) return "Vida max +20 (cura 20)";
    return "Velocidade +15";
}

static const char *RarityLabel(int rarity) {
    if (rarity == RARITY_RARE) return "RARO";
    if (rarity == RARITY_EPIC) return "ÉPICO";
    return "COMUM";
}

static const char *RewardText(const Encounter *e, Reward reward) {
    switch (reward.kind) {
        case REWARD_DAMAGE: return "Dano\n+15% de dano base";
        case REWARD_HEALTH: return "Vida maxima\n+20 de vida e cura 20";
        case REWARD_SPEED: return "Velocidade\n+15 de velocidade";
        case REWARD_SWAP:
            return TextFormat("Trocar por %s\n- %s\n- %s",
                              WeaponTitle(reward.weapon),
                              CommonBuffShortText(reward.bonus[0]),
                              CommonBuffShortText(reward.bonus[1]));
        case REWARD_WEAPON_UPGRADE: {
            int nextLevel = CurrentRunWeaponLevel(e) + 1;
            const char *perk = "Dano +20%";
            if (reward.weapon == 0) {
                if (nextLevel == 1) perk = "Intervalo 0,20->0,14 s, vel 760->860 px/s";
                else if (nextLevel == 2) perk = "Dano da arma +25% adicional";
                else if (nextLevel == 3) perk = "2 tiros por disparo (+-5 graus)";
                else if (nextLevel == 4) perk = "Tiros perfurantes (ate 2 inimigos)";
                else if (nextLevel == 5) perk = "Duas pistolas (4 tiros, +-13 px)";
            } else if (reward.weapon == 1) {
                if (nextLevel == 1) perk = "Empurrao 36 px, dano +15% adicional";
                else if (nextLevel == 2) perk = "Livros 4->5, orbita 90->102 px";
                else if (nextLevel == 3) perk = "Boost 1,25->1,65 s, recarga 4,0->2,8 s";
                else if (nextLevel == 4) perk = "Livros 5->6, bloqueia tiros no boost";
                else if (nextLevel == 5) perk = "Pocas de agua benta (8 dano/0,45 s)";
            } else if (reward.weapon == 2) {
                if (nextLevel == 1) perk = "Acerto reduz recarga da esquiva em 0,8 s";
                else if (nextLevel == 2) perk = "Alcance 105->123 px, recuperacao 0,45->0,40 s";
                else if (nextLevel == 3) perk = "Arco 170->194 graus, recuo 42 px";
                else if (nextLevel == 4) perk = "Onda de choque (22 dano, recarga 2,5 s)";
                else if (nextLevel == 5) perk = "Onda tripla (+-20 graus, recarga 1,8 s)";
            }
            return TextFormat("%s (%d/%d) -\n%s\nDano +20%%",
                              WeaponTitle(reward.weapon), nextLevel, MAX_WEAPON_LEVEL, perk);
        }
        case REWARD_EPIC_DAMAGE: {
            int base = WeaponBaseDamage(e->progress.weapon);
            int before = (int)ceilf(base * EncounterDamage(e));
            Encounter afterEncounter = *e;
            afterEncounter.progress.epicDamageStacks++;
            int after = (int)ceilf(base * EncounterDamage(&afterEncounter));
            return TextFormat("Poder das Armas (%s)\nGolpe principal: %d->%d de dano",
                              WeaponTitle(e->progress.weapon), before, after);
        }
        case REWARD_EPIC_STATUS:
            return TextFormat("Status\nVida +%d / dano +25%%\nVelocidade +25%%", reward.amount);
        case REWARD_EPIC_FULL_HEAL:
            return "Vida plena\nCura toda a vida\nVida maxima +50";
        default:
            return "";
    }
}

static void DrawMultilineCardText(const char *text, Rectangle area, float fontSize, Color color) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", text);
    char *lines[6];
    int lineCount = 0;
    char *cursor = buffer;
    while (cursor && *cursor && lineCount < 6) {
        lines[lineCount++] = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            break;
        }
    }
    if (lineCount == 0) return;
    float maxLineW = 1.0f;
    for (int i = 0; i < lineCount; i++) {
        float w = MeasureGameTextEx(lines[i], fontSize, false).x;
        if (w > maxLineW) maxLineW = w;
    }
    float lineStep = fontSize * 1.18f;
    float totalH = lineCount * lineStep;
    float scale = fminf(1.0f, fminf(area.width / maxLineW, area.height / totalH));
    float finalSize = fontSize * scale;
    float finalStep = lineStep * scale;
    for (int i = 0; i < lineCount; i++) {
        DrawGameTextEx(lines[i], (Vector2){area.x, area.y + i * finalStep}, finalSize, color, false);
    }
}

static void DrawRewardCardFrame(Rectangle box, int rarity, bool hovered) {
    EnsureRewardUiAssetsLoaded();
    int r = (rarity >= 0 && rarity < RARITY_COUNT) ? rarity : RARITY_COMMON;
    static const Color bgNormal[RARITY_COUNT] = {
        {38, 42, 50, 245},  /* Comum: ardósia prateada escura */
        {22, 42, 76, 245},  /* Raro: azul profundo */
        {48, 24, 72, 245}   /* Épico: roxo escuro */
    };
    static const Color bgHover[RARITY_COUNT] = {
        {58, 64, 76, 255},
        {34, 64, 112, 255},
        {74, 38, 108, 255}
    };
    static const Color outerBorder[RARITY_COUNT] = {
        {196, 204, 216, 255}, /* Comum: cinza prata claro */
        {72, 156, 255, 255},  /* Raro: azul brilhante */
        {182, 92, 255, 255}   /* Épico: roxo vibrante */
    };
    static const Color innerBorder[RARITY_COUNT] = {
        {135, 145, 160, 230},
        {130, 195, 255, 230},
        {235, 190, 90, 240}   /* Épico: filete interno dourado */
    };

    Texture2D tex = rewardUiAssets.frames[r];
    if (tex.id) {
        int slice = (int)fminf(UI_CARD_NPATCH_SLICE, fminf(tex.width / 3.0f, tex.height / 3.0f));
        NPatchInfo patch = {
            .source = {0, 0, (float)tex.width, (float)tex.height},
            .left = slice, .top = slice, .right = slice, .bottom = slice,
            .layout = NPATCH_NINE_PATCH
        };
        DrawTextureNPatch(tex, patch, box, (Vector2){0, 0}, 0.0f, hovered ? WHITE : (Color){235, 235, 235, 255});
    } else {
        DrawRectangleRec(box, hovered ? bgHover[r] : bgNormal[r]);
        DrawRectangleLinesEx(box, 3.0f, outerBorder[r]);
        Rectangle inner = {box.x + 5.0f, box.y + 5.0f, box.width - 10.0f, box.height - 10.0f};
        DrawRectangleLinesEx(inner, 1.5f, innerBorder[r]);
    }

    /* Detalhe no topo por raridade: gema e ornamento dourado no épico */
    Vector2 gemCenter = {box.x + box.width * 0.5f, box.y};
    if (r == RARITY_COMMON) {
        DrawCircleV(gemCenter, 7.0f, (Color){110, 118, 130, 255});
        DrawCircleV(gemCenter, 4.5f, (Color){220, 226, 235, 255});
    } else if (r == RARITY_RARE) {
        DrawPoly(gemCenter, 4, 9.0f, 45.0f, (Color){32, 92, 180, 255});
        DrawPoly(gemCenter, 4, 6.0f, 45.0f, (Color){120, 205, 255, 255});
        DrawPolyLinesEx(gemCenter, 4, 9.0f, 45.0f, 1.5f, (Color){200, 235, 255, 255});
    } else {
        /* Ornamento dourado nos cantos e ao redor da gema roxa do épico */
        Color gold = (Color){242, 198, 88, 255};
        DrawLineEx((Vector2){gemCenter.x - 28.0f, gemCenter.y}, (Vector2){gemCenter.x - 10.0f, gemCenter.y}, 3.0f, gold);
        DrawLineEx((Vector2){gemCenter.x + 10.0f, gemCenter.y}, (Vector2){gemCenter.x + 28.0f, gemCenter.y}, 3.0f, gold);
        DrawPoly((Vector2){gemCenter.x - 28.0f, gemCenter.y}, 4, 4.0f, 45.0f, gold);
        DrawPoly((Vector2){gemCenter.x + 28.0f, gemCenter.y}, 4, 4.0f, 45.0f, gold);
        DrawPoly(gemCenter, 4, 11.0f, 45.0f, gold);
        DrawPoly(gemCenter, 4, 7.5f, 45.0f, (Color){185, 85, 255, 255});
        DrawPoly(gemCenter, 4, 4.0f, 45.0f, (Color){245, 210, 255, 255});
        /* Cantoneiras douradas */
        float c = 12.0f;
        DrawLineEx((Vector2){box.x + 2, box.y + 2}, (Vector2){box.x + 2 + c, box.y + 2}, 2.5f, gold);
        DrawLineEx((Vector2){box.x + 2, box.y + 2}, (Vector2){box.x + 2, box.y + 2 + c}, 2.5f, gold);
        DrawLineEx((Vector2){box.x + box.width - 2, box.y + 2}, (Vector2){box.x + box.width - 2 - c, box.y + 2}, 2.5f, gold);
        DrawLineEx((Vector2){box.x + box.width - 2, box.y + 2}, (Vector2){box.x + box.width - 2, box.y + 2 + c}, 2.5f, gold);
    }

    /* Rótulo de raridade ("COMUM" / "RARO" / "ÉPICO") */
    const char *tag = RarityLabel(r);
    Color tagColor = r == RARITY_EPIC ? (Color){245, 208, 105, 255} : outerBorder[r];
    Rectangle tagBounds = {box.x + 12.0f, box.y + 9.0f, box.width - 24.0f, 18.0f};
    DrawGameTextIn(tag, tagBounds, 15.0f, tagColor, false);
}

static void DrawRerollButton(Rectangle btn, bool disabled, bool hovered) {
    EnsureRewardUiAssetsLoaded();
    Texture2D strip = rewardUiAssets.rerollButton;
    if (strip.id && strip.width >= 3) {
        int stateIndex = disabled ? 2 : (hovered ? 1 : 0);
        float frameW = strip.width / 3.0f;
        Rectangle src = {stateIndex * frameW, 0, frameW, (float)strip.height};
        DrawTexturePro(strip, src, btn, (Vector2){0, 0}, 0.0f, WHITE);
        return;
    }
    Color fill = disabled ? (Color){52, 52, 56, 230}
                          : (hovered ? (Color){126, 86, 40, 255} : (Color){86, 56, 26, 255});
    Color border = disabled ? (Color){125, 125, 132, 255}
                            : (hovered ? (Color){250, 212, 112, 255} : (Color){206, 162, 74, 255});
    Color ink = disabled ? (Color){165, 165, 172, 255} : RAYWHITE;
    DrawRectangleRec(btn, fill);
    DrawRectangleLinesEx(btn, 2.0f, border);
    Rectangle inner = {btn.x + 3.0f, btn.y + 3.0f, btn.width - 6.0f, btn.height - 6.0f};
    DrawRectangleLinesEx(inner, 1.0f, Fade(border, 0.55f));
    /* Ícone de seta circular à esquerda do botão */
    Vector2 iconCenter = {btn.x + 22.0f, btn.y + btn.height * 0.5f};
    DrawRing(iconCenter, 5.5f, 8.0f, 35.0f, 325.0f, 16, ink);
    Vector2 tip = {iconCenter.x + 6.5f, iconCenter.y - 5.5f};
    DrawTriangle((Vector2){tip.x + 3.0f, tip.y + 1.0f},
                 (Vector2){tip.x - 4.0f, tip.y - 3.0f},
                 (Vector2){tip.x - 2.5f, tip.y + 4.0f}, ink);
    Rectangle textArea = {btn.x + 34.0f, btn.y + 4.0f, btn.width - 42.0f, btn.height - 8.0f};
    DrawGameTextIn("Sortear", textArea, 17.0f, ink, false);
}

void DrawEncounter(const Encounter *e, const EnemyGroup *enemies) {
    if (e->presentation.remaining>0) {
        Texture2D bossSprite = e->progress.map == BONELAND_MAP ? enemies->boneLordPortrait
            : e->progress.map == 2 ? enemies->villageBossWalk
            : (e->progress.map == 1 ? enemies->cemeteryPortrait : enemies->bossPortrait);
        if (e->progress.map == TEMPLAR_FIELD_MAP) bossSprite = enemies->marshalPortrait;
        if (e->progress.map == NECROMANCER_MAP) bossSprite = enemies->necromancerPortrait;
        bool animatedPortrait = e->progress.map == 2;
        int cols = animatedPortrait ? 6 : 1;
        int rows = cols > 1 ? 4 : 1;
        bool gothic = e->progress.map == BONELAND_MAP || e->progress.map == TEMPLAR_FIELD_MAP ||
                      e->progress.map == NECROMANCER_MAP;
        DrawStagePresentation(&e->presentation, bossSprite, cols, rows, gothic);
        return;
    }
    bool bossVisible = EncounterBossVisible(e, enemies);
    int statusY = 205 + (bossVisible ? ENCOUNTER_BOSS_HUD_HEIGHT : 0);
    const char *mapLabel = e->progress.map == BONELAND_MAP ? "Terra dos Ossos" : e->progress.map == 5 ? "Capela Invertida"
        : (e->progress.map == 4 ? "Porão da Capela"
        : (e->progress.map == 3 ? "Capela do Anjo"
        : (e->progress.map == 2 ? "Chã da Tristeza"
        : (e->progress.map == 1 ? "Cemitério" : "Clareira"))));
    const char *clearLabel = e->progress.map == 2 ? "Chã da Tristeza limpa - siga pelo arco ao topo"
        : (e->progress.map == 1 ? "Cemitério limpo - siga pela saída à direita"
                                : "Clareira limpa - altar liberado");
    const char *bossLabel = e->progress.map == NECROMANCER_MAP ? "Necromante" : e->progress.map == BONELAND_MAP ? boneLordName
        : e->progress.map == TEMPLAR_FIELD_MAP ? "Marechal Carmesim"
        : e->progress.map == 2 ? "Líder da Vila"
        : (e->progress.map == 1 ? "Coveiro das Raízes" : "Guardião da Clareira");
    if (e->progress.map == NECROMANCER_MAP) {
        static const char *forms[]={"Forma esquelética","Forma humanoide","Anjo caído"};
        int form=enemies->list[0].necromancerForm;
        const char *status=e->phase==ENCOUNTER_CLEAR ? "Vitória • Enter para voltar ao menu" :
            enemies->list[0].state==ENEMY_FALLEN ? "Aproxime-se do selo central e pressione E" :
            form>0 ? TextFormat("Fase %d/3 • %s",form,forms[form-1]) : "Topo do Palácio";
        DrawGameText(status,24,statusY,20,GOLD);
    } else if (e->progress.map == TEMPLAR_FIELD_MAP) {
        const char *status = e->phase == ENCOUNTER_CLEAR ? "Marechal derrotado"
            : e->progress.stage == CLEARING_BOSS_STAGE ? "Encontro 3/3 - Marechal Carmesim"
            : TextFormat("Encontro %d/3 | Restantes: %d", e->progress.stage, EnemiesRemaining(enemies));
        DrawGameText(status, 24, statusY, 20, GOLD);
    } else if (e->progress.map == BONELAND_MAP) {
        const char *status = e->phase == ENCOUNTER_CLEAR ? "Terra dos Ossos limpa - siga ao norte"
            : e->progress.stage == CLEARING_BOSS_STAGE ? TextFormat("Onda %d/%d - %s",
              BONELAND_WAVES + 1, BONELAND_WAVES + 1, boneLordName)
            : TextFormat("Terra dos Ossos - onda %d/%d | Restantes: %d",
                         e->progress.stage, BONELAND_WAVES, EnemiesRemaining(enemies));
        DrawGameText(status, 24, statusY, 20, GOLD);
    } else if (e->progress.map == 3) DrawGameText(e->chapelAltarDone
        ? "Capela do Anjo - desça pela porta superior esquerda ao Porão"
        : "Capela do Anjo - aproxime-se do altar central (E)", 24, statusY, 20, GOLD);
    else if (e->progress.map == 4) {
        int solved = e->basementSolved[0] + e->basementSolved[1] + e->basementSolved[2] + e->basementSolved[3];
        DrawGameText(e->basementComplete
            ? "Último Selo completo - suba pela escadaria ao norte para o Mundo Invertido"
            : TextFormat("Porão da Capela - Pilares do Último Selo: %d/4 (E nas placas e pilares)", solved),
                     24, statusY, 20, GOLD);
    } else if (e->progress.map == 5) {
        DrawGameText("Capela Invertida (Mundo Invertido) - aproxime-se do altar e estátuas (E)", 24, statusY, 20, GOLD);
    }
    else if (e->progress.stage<=CLEARING_WAVES) DrawGameText(TextFormat("%s - onda %d/5 | Restantes: %d",
        mapLabel,e->progress.stage,EnemiesRemaining(enemies)),24,statusY,20,GOLD);
    else DrawGameText(e->phase==ENCOUNTER_CLEAR ? clearLabel :
        (e->progress.map == 2 ? (e->villageHordeStarted ?
            TextFormat("Horda da Vila - Restantes: %d", EnemiesRemaining(enemies)) :
            "Duelo - Líder da Vila")
            : TextFormat("Onda 5/5 - %s", bossLabel)),24,statusY,20,GOLD);
    if (e->notice[0]) DrawGameText(e->notice,24,statusY+27,18,RAYWHITE);
    if (bossVisible) {
        const Enemy *boss=&enemies->list[0];
        bool phaseTwo = boss->guardianPhase != GUARDIAN_PHASE_ONE;
        const char *phase = boss->guardianPhase == GUARDIAN_PHASE_TRANSITION ? "Despertando..." :
                            phaseTwo ? "Fase 2/2" : "Fase 1/2";
        if(boss->necromancerForm && boss->guardianPhase!=GUARDIAN_PHASE_TRANSITION)
            phase=TextFormat("Fase %d/3",boss->necromancerForm);
        Color healthColor=boss->necromancerForm==3 ? (Color){211,48,92,255} :
            boss->necromancerForm==2 || phaseTwo ? PURPLE : MAROON;
        int w=(int)fminf(600,GetScreenWidth()*.5f), x=(GetScreenWidth()-w)/2;
        DrawRectangle(x-8,8,w+16,46,Fade(BLACK,.78f));
        DrawRectangle(x,31,w,16,DARKGRAY);
        DrawRectangle(x,31,w*boss->health/boss->maxHealth,16,healthColor);
        if(boss->necromancerForm) {
            DrawRectangle(x+w*70/100,31,2,16,Fade(GOLD,.72f));
            DrawRectangle(x+w*35/100,31,2,16,Fade(GOLD,.72f));
        } else DrawRectangle(x+w/2,31,2,16,Fade(GOLD,.65f));
        const char *label = TextFormat("%s  %d/%d  |  %s",bossLabel,
                                       boss->health,boss->maxHealth,phase);
        DrawGameText(label,(GetScreenWidth()-MeasureGameText(label,18))/2,10,18,GOLD);
    }
    if (!EncounterPaused(e)) return;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.86f));
    if (e->phase == ENCOUNTER_CHAPEL_TEXT) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int bw = (int)fminf(760.0f, sw - 80.0f), bh = 370;
        int bx = (sw - bw) / 2, by = (sh - bh) / 2;
        DrawRectangle(bx, by, bw, bh, (Color){24, 28, 38, 245});
        DrawRectangleLinesEx((Rectangle){(float)bx, (float)by, (float)bw, (float)bh}, 2, GOLD);
        static const char *titles[] = {
            "Voz do Anjo Guardião",
            "Estátua do Guardião",
            "Estátua do Irmão Caído",
            "O Trono Partido",
            "Selo Secundário",
            "Placa I — Parede Noroeste",
            "Placa II — Esquerda da Escadaria",
            "Placa III — Direita da Escadaria",
            "Placa IV — Parede Nordeste",
            "Altar Corrompido",
            "Estátua Sacrificada (Guardião)",
            "Estátua Preservada (Necromante)",
            "Trono do Mundo Invertido",
            "Selo Adormecido"
        };
        static const char *bodies[][6] = {
            {
                "\"Mortal... você chegou além das raízes e da vila caída.",
                "Meu irmão e eu protegíamos os vivos, até ele desafiar a morte",
                "e tornar-se o Necromante. Sacrifiquei meu poder nos Selos Sagrados.",
                "Desça pela porta superior esquerda até o Porão secreto e restaure",
                "os 4 pilares do Último Selo para atravessar ao Mundo Invertido.\"",
                ""
            },
            {
                "A estátua permanece erguida, iluminada por um fio dourado.",
                "Na base lê-se: \"Ao Guardião que renunciou à própria força",
                "para selar o abismo e dar aos mortais mais um amanhecer.\"",
                "", "", ""
            },
            {
                "A face celestial está erodida e marcada por fissuras profundas.",
                "\"Aquele que negou a lei da morte partiu o primeiro selo",
                "e transformou sua própria glória em cinzas e maldição.\"",
                "", "", ""
            },
            {
                "O trono de pedra está partido e vazio há séculos.",
                "Nenhum dos dois irmãos voltou a sentar-se aqui desde a queda.",
                "", "", "", ""
            },
            {
                "Um círculo de proteção antigo e adormecido, gravado na pedra.",
                "Sua energia responde apenas quando o selo central se rompe.",
                "", "", "", ""
            },
            {
                "\"Os dois pilares superiores — Noroeste (Sup. Esq.) e",
                "Nordeste (Sup. Dir.) — jamais compartilham o mesmo valor.\"",
                "", "", "", ""
            },
            {
                "\"Se o Pilar Noroeste (Sup. Esq.) for FALSO, então o",
                "Pilar Sudoeste (Inf. Esq.) também será FALSO.\"",
                "", "", "", ""
            },
            {
                "\"Os dois pilares inferiores (Sudoeste e Sudeste) têm valores",
                "opostos entre si; e o Pilar Sudoeste (Inf. Esq.) é VERDADEIRO.\"",
                "", "", "", ""
            },
            {
                "\"Combine as três inscrições anteriores para deduzir o valor",
                "exato de cada pilar e despertar os quatro quadrantes do selo.\"",
                "", "", "", ""
            },
            {
                "Vestígios dourados quase apagados resistem nas bordas do altar.",
                "\"Mesmo do outro lado, a última centelha do Guardião ainda",
                "sustenta esta passagem contra o domínio do Necromante.\"",
                "", "", ""
            },
            {
                "Aqui no Mundo Invertido, a estátua do Guardião está em ruínas.",
                "Asas quebradas e vinhas secas mostram o preço de seu sacrifício.",
                "", "", "", ""
            },
            {
                "Intacta e livre de vinhas, a estátua do Necromante permanece",
                "imponente — sua influência domina este lado da capela.",
                "", "", "", ""
            },
            {
                "O trono partido continua vazio, cercado por cinzas e raízes.",
                "", "", "", "", ""
            },
            {
                "O selo secundário foi drenado pela corrupção carmesim.",
                "", "", "", "", ""
            }
        };
        int tid = (e->chapelTextId >= 0 && e->chapelTextId < 14) ? e->chapelTextId : 0;
        DrawGameTextIn(titles[tid], (Rectangle){(float)bx, (float)(by + 18), (float)bw, 34}, 28, GOLD, true);
        for (int i = 0; i < 6; i++) {
            if (bodies[tid][i][0]) DrawGameText(bodies[tid][i], bx + 36, by + 84 + i * 32, 20, RAYWHITE);
        }
        const char *hint = "[Espaço / Enter / Clique] Continuar";
        DrawGameText(hint, (sw - MeasureGameText(hint, 19)) / 2, by + bh - 44, 19, GOLD);
        return;
    }
    if (e->phase == ENCOUNTER_BASEMENT_PILLAR) {
        int sw = GetScreenWidth();
        int titleY = (int)(GetScreenHeight() / 2 - 160);
        static const char *pnames[4] = {
            "Pilar Noroeste (Sup. Esq.)",
            "Pilar Nordeste (Sup. Dir.)",
            "Pilar Sudoeste (Inf. Esq.)",
            "Pilar Sudeste (Inf. Dir.)"
        };
        int t = (e->basementTarget >= 0 && e->basementTarget < 4) ? e->basementTarget : 0;
        DrawGameTextIn(pnames[t], (Rectangle){0, (float)titleY, (float)sw, 49}, 38, GOLD, true);
        const char *prompt = "Qual estado lógico deduzido pelas placas ativa este pilar?";
        DrawGameText(prompt, (sw - MeasureGameText(prompt, 22)) / 2, titleY + 60, 22, RAYWHITE);
        for (int i = 0; i < 2; i++) {
            Rectangle box = ChoiceRect(i, 2);
            DrawRectangleRec(box, CheckCollisionPointRec(GetMousePosition(), box) ? (Color){80,45,50,255} : (Color){40,25,35,255});
            DrawRectangleLinesEx(box, 2, GOLD);
            const char *lbl = i == 0 ? "1. Verdadeiro (V)" : "2. Falso (F)";
            DrawGameText(lbl, (int)box.x + 18, (int)box.y + 20, 20, RAYWHITE);
        }
        const char *escHint = "[1 / 2 ou Clique] Escolher  |  [Esc] Voltar";
        DrawGameText(escHint, (sw - MeasureGameText(escHint, 18)) / 2, GetScreenHeight() / 2 + 145, 18, GOLD);
        return;
    }
    if (e->phase == ENCOUNTER_LETTER) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        Texture2D letter=enemies->villageLetter;
        if (letter.id) {
            const float hintHeight=36;
            float scale=fminf(sw/(float)letter.width,fmaxf(1,sh-hintHeight)/(float)letter.height);
            float width=letter.width*scale, height=letter.height*scale;
            DrawRectangle(0,0,sw,sh,BLACK);
            DrawTexturePro(letter,(Rectangle){0,0,letter.width,letter.height},
                (Rectangle){(sw-width)/2,(sh-hintHeight-height)/2,width,height},
                (Vector2){0},0,WHITE);
            DrawGameTextIn("Espaço / Enter / E / Clique para continuar",
                (Rectangle){0,sh-hintHeight,sw,hintHeight},20,GOLD,false);
            return;
        }
        int bw = (int)fminf(720.0f, sw - 80.0f), bh = 380;
        int bx = (sw - bw) / 2, by = (sh - bh) / 2;
        DrawRectangle(bx, by, bw, bh, (Color){38, 28, 22, 245});
        DrawRectangleLinesEx((Rectangle){(float)bx, (float)by, (float)bw, (float)bh}, 2, GOLD);
        const char *ltitle = "Último Registro do Líder da Vila";
        DrawGameTextIn(ltitle, (Rectangle){(float)bx, (float)(by + 18), (float)bw, 34}, 28, GOLD, true);
        static const char *lines[] = {
            "\"Quando a névoa rubra desceu sobre a Chã de Alegria, nenhum",
            "dos nossos conseguiu resistir à febre que tomou o vale.",
            "Enterramos dezenas no Cemitério das Raízes antes que os",
            "próprios mortos começassem a responder ao chamado do selo.",
            "",
            "Eu fui o único que permaneceu de pé, preso à culpa de",
            "sobreviver... Se você me derrubou, cuidado: a terra da",
            "vila ainda guarda os que não descansaram.\""
        };
        for (int i = 0; i < 8; i++) {
            DrawGameText(lines[i], bx + 36, by + 76 + i * 28, 19, RAYWHITE);
        }
        const char *hint = "[Espaço / Enter / Clique] Continuar";
        DrawGameText(hint, (sw - MeasureGameText(hint, 19)) / 2, by + bh - 44, 19, GOLD);
        return;
    }
    int titleY=(int)(GetScreenHeight()/2-160);
    const char *title=e->phase==ENCOUNTER_REWARD ? "Escolha seu bônus" :
                      e->phase==ENCOUNTER_SEAL ? (e->progress.map==NECROMANCER_MAP ?
                        "Grande Selo Profano" : "Selo - combate pausado") : "Fim da tentativa";
    DrawGameTextIn(title,(Rectangle){0,titleY,GetScreenWidth(),49},42,GOLD,true);
    if (e->phase==ENCOUNTER_DEFEAT) {
        DrawGameText("Esc: menu | Continuar reinicia esta onda ou boss",GetScreenWidth()/2-270,titleY+80,20,RAYWHITE);
        return;
    }
    int count=e->phase==ENCOUNTER_REWARD ? e->rewardCount :
              enemies->list[e->sealEnemy].type==ENEMY_BOSS ? 3 : 2;
    static const char *questions[]={"V e V = V", "V e F = V", "nao V = V", "V ou F = V"};
    const SealQuestion *sealQuestions=e->progress.map==NECROMANCER_MAP ? finalQuestions : bossQuestions;
    if (e->phase==ENCOUNTER_SEAL) {
        const char *prompt=count==3 ? TextFormat("Qual expressão equivale a: %s ?",sealQuestions[e->question].expression)
                                    : questions[e->question];
        if (e->progress.map==NECROMANCER_MAP) {
            DrawGameTextIn(prompt,(Rectangle){18,titleY+46,GetScreenWidth()-36,35},22,RAYWHITE,false);
            DrawGameTextIn(TextFormat("Tentativa %d/3",e->progress.finalSealFailures+1),
                (Rectangle){18,titleY+81,GetScreenWidth()-36,27},18,GOLD,false);
        } else DrawGameText(prompt,(GetScreenWidth()-MeasureGameText(prompt,22))/2,titleY+60,22,RAYWHITE);
    }
    for (int i=0;i<count;i++) {
        Rectangle box=ChoiceRect(i,count);
        bool hovered = CheckCollisionPointRec(GetMousePosition(),box);
        if (e->phase==ENCOUNTER_REWARD) {
            DrawRewardCardFrame(box, e->rewards[i].rarity, hovered);
            const char *label = RewardText(e, e->rewards[i]);
            Rectangle textArea = {box.x + 14.0f, box.y + 30.0f, box.width - 28.0f, box.height - 40.0f};
            DrawMultilineCardText(TextFormat("%d. %s", i + 1, label), textArea, 18.0f, RAYWHITE);
            if (e->progress.stage != CLEARING_WAVES) {
                Rectangle btn = RerollButtonRect(i, count);
                bool btnHover = !e->rerolled && CheckCollisionPointRec(GetMousePosition(), btn);
                DrawRerollButton(btn, e->rerolled, btnHover);
            }
        } else {
            DrawRectangleRec(box,hovered?(Color){80,45,50,255}:(Color){40,25,35,255});
            DrawRectangleLinesEx(box,2,GOLD);
            const char *label = count==2 ? (i==0?"Verdadeiro":"Falso")
                                         : sealQuestions[e->question].choices[(i+3-e->answer)%3];
            if (e->phase==ENCOUNTER_SEAL && e->progress.map==NECROMANCER_MAP)
                DrawGameTextIn(TextFormat("%d. %s",i+1,label),
                    (Rectangle){box.x+8,box.y+6,box.width-16,box.height-12},19,RAYWHITE,false);
            else DrawGameText(TextFormat("%d. %s",i+1,label),(int)box.x+14,(int)box.y+20,19,RAYWHITE);
        }
    }
}
