#include "game_text.h"
#include "encounter.h"
#include "hud.h"
#include "menu.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMON_DAMAGE_BONUS 0.15f
#define COMMON_HEALTH_BONUS 20
#define COMMON_SPEED_BONUS 15.0f
#define EPIC_STATUS_DAMAGE_BONUS 0.25f
#define EPIC_STATUS_SPEED_BONUS 51.0f
#define EPIC_STATUS_HEALTH_BONUS 50
#define EPIC_FULL_HEAL_MAX_HP_BONUS 50
#define RARITY_WEIGHT_COMMON 70
#define RARITY_WEIGHT_RARE 25
#define RARITY_WEIGHT_EPIC 5
#define MAX_CAMPAIGN_MAPS 9
#define UI_CARD_FRAME_COMMON_PATH "assets/ui/card_frame_common.png"
#define UI_CARD_FRAME_RARE_PATH "assets/ui/card_frame_rare.png"
#define UI_CARD_FRAME_EPIC_PATH "assets/ui/card_frame_epic.png"
#define UI_REROLL_BUTTON_PATH "assets/ui/reroll_button.png"
#define UI_REWARD_BANNER_PATH "assets/ui/reward_banner.png"

static const char *const cardFramePaths[RARITY_COUNT] = {
    UI_CARD_FRAME_COMMON_PATH,
    UI_CARD_FRAME_RARE_PATH,
    UI_CARD_FRAME_EPIC_PATH
};
static const Rectangle cardFrameSources[RARITY_COUNT] = {
    {218, 20, 1100, 886}, {188, 19, 1160, 930}, {145, 0, 1246, 1000}
};

typedef struct RewardUiAssets {
    bool attempted;
    Texture2D frames[RARITY_COUNT];
    Texture2D rerollButton;
    Texture2D banner;
} RewardUiAssets;

static RewardUiAssets rewardUiAssets = {0};
static Texture2D ritualFrames[2];
static bool ritualFramesAttempted;

static Texture2D LoadRewardArtwork(const char *path, bool rareFrame) {
    if (!FileExists(path)) return (Texture2D){0};
    Image art = LoadImage(path);
    if (!art.data) return (Texture2D){0};
    if (rareFrame && art.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) {
        ImageFormat(&art, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Color *pixels = art.data;
        Rectangle crop = cardFrameSources[RARITY_RARE];
        /* The blue halo was supplied on black. Recover its alpha outside the panel. */
        for (int y = 0; y < art.height; y++) for (int x = 0; x < art.width; x++) {
            float u = x / (float)art.width, v = y / (float)art.height;
            if (u >= .155f && u <= .845f && v >= .154f && v <= .864f) continue;
            Color *p = &pixels[y * art.width + x];
            int alpha = (int)fmaxf(p->r, fmaxf(p->g, p->b));
            if (alpha == 0) { p->a = 0; continue; }
            p->r = p->r * 255 / alpha;
            p->g = p->g * 255 / alpha;
            p->b = p->b * 255 / alpha;
            float edge = fminf(fminf(x - crop.x, crop.x + crop.width - x),
                               fminf(y - crop.y, crop.y + crop.height - y));
            float fade = Clamp(edge / 36.0f, 0, 1);
            p->a = (unsigned char)(alpha * fade * fade * (3 - 2 * fade));
        }
    }
    Texture2D texture = LoadTextureFromImage(art);
    UnloadImage(art);
    if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

static void EnsureRewardUiAssetsLoaded(void) {
    if (rewardUiAssets.attempted || !IsWindowReady()) return;
    rewardUiAssets.attempted = true;
    for (int i = 0; i < RARITY_COUNT; i++)
        rewardUiAssets.frames[i] = LoadRewardArtwork(cardFramePaths[i], i == RARITY_RARE);
    rewardUiAssets.rerollButton = LoadRewardArtwork(UI_REROLL_BUTTON_PATH, false);
    rewardUiAssets.banner = LoadRewardArtwork(UI_REWARD_BANNER_PATH, false);
}

void UnloadEncounterRewardUi(void) {
    for (int i = 0; i < RARITY_COUNT; i++)
        if (rewardUiAssets.frames[i].id) UnloadTexture(rewardUiAssets.frames[i]);
    if (rewardUiAssets.rerollButton.id) UnloadTexture(rewardUiAssets.rerollButton);
    if (rewardUiAssets.banner.id) UnloadTexture(rewardUiAssets.banner);
    rewardUiAssets = (RewardUiAssets){0};
    for (int i = 0; i < 2; i++) {
        if (ritualFrames[i].id) UnloadTexture(ritualFrames[i]);
        ritualFrames[i] = (Texture2D){0};
    }
    ritualFramesAttempted = false;
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

/* Tabela única de dificuldade dos selos por mapa (LogicDifficultyScore [min, max]).
   Guarda todas as linhas, inclusive as dormentes (Cemitério V/F, Chã da Tristeza V/F e boss,
   Terra dos Ossos V/F e Portão da Última Sentença V/F). Mapas 3, 4 e 5 não possuem linha. */
static const EncounterSealDifficultyRow sealDifficultyTable[MAX_CAMPAIGN_MAPS] = {
    [0] = { .map = 0, .hasSpecialVF = true,  .specialMin = 2, .specialMax = 3, .hasBossMC = true, .bossMin = 4,  .bossMax = 5  },
    [1] = { .map = 1, .hasSpecialVF = true,  .specialMin = 3, .specialMax = 4, .hasBossMC = true, .bossMin = 5,  .bossMax = 6  },
    [2] = { .map = 2, .hasSpecialVF = true,  .specialMin = 4, .specialMax = 5, .hasBossMC = true, .bossMin = 6,  .bossMax = 7  },
    [6] = { .map = 6, .hasSpecialVF = true,  .specialMin = 5, .specialMax = 6, .hasBossMC = true, .bossMin = 7,  .bossMax = 8  },
    [7] = { .map = 7, .hasSpecialVF = true,  .specialMin = 5, .specialMax = 6, .hasBossMC = true, .bossMin = 8,  .bossMax = 9  },
    [8] = { .map = 8, .hasSpecialVF = false, .specialMin = 0, .specialMax = 0, .hasBossMC = true, .bossMin = 10, .bossMax = 12 },
};

int EncounterSealTableCount(void) {
    return MAX_CAMPAIGN_MAPS;
}

const EncounterSealDifficultyRow *EncounterSealTableRow(int index) {
    if (index < 0 || index >= MAX_CAMPAIGN_MAPS) return NULL;
    const EncounterSealDifficultyRow *row = &sealDifficultyTable[index];
    if (!row->hasSpecialVF && !row->hasBossMC) return NULL;
    return row;
}

bool EncounterSealRange(int map, bool isBoss, int *outMin, int *outMax) {
    int clamped = map < 0 ? 0 : (map >= MAX_CAMPAIGN_MAPS ? MAX_CAMPAIGN_MAPS - 1 : map);
    for (int m = clamped; m >= 0; m--) {
        const EncounterSealDifficultyRow *row = &sealDifficultyTable[m];
        if (!row->hasSpecialVF && !row->hasBossMC) continue;
        if (isBoss && row->hasBossMC) {
            if (m != map)
                TraceLog(LOG_WARNING, "SELO: Mapa %d sem faixa de boss; usando linha do mapa %d [%d..%d].",
                         map, m, row->bossMin, row->bossMax);
            if (outMin) *outMin = row->bossMin;
            if (outMax) *outMax = row->bossMax;
            return m == map;
        }
        if (!isBoss && row->hasSpecialVF) {
            if (m != map)
                TraceLog(LOG_WARNING, "SELO: Mapa %d sem faixa V/F; usando linha do mapa %d [%d..%d].",
                         map, m, row->specialMin, row->specialMax);
            if (outMin) *outMin = row->specialMin;
            if (outMax) *outMax = row->specialMax;
            return m == map;
        }
    }
    if (outMin) *outMin = isBoss ? 4 : 2;
    if (outMax) *outMax = isBoss ? 5 : 3;
    return false;
}

static LogicStyle CurrentEncounterLogicStyle(void) {
    return (GameTextHasGlyph(0x2227) && GameTextHasGlyph(0x2228))
           ? LOGIC_STYLE_SYMBOLS : LOGIC_STYLE_WORDS;
}

static const char *WeaponTitle(int weapon) {
    return weapon == 0 ? "Arma de fogo" : weapon == 1 ? "Bíblia" : "Katana";
}

static bool IsEpicRewardStage(const Encounter *e) {
    return e->progress.stage == CampaignWaveCount(e->progress.map);
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
            clearing->templarField ? TEMPLAR_FIELD_TITLE
            : clearing->boneland ? "Terra dos Ossos" : clearing->invertedChapel ? "Capela Invertida"
            : (clearing->basement ? "Porão da Capela"
            : (clearing->chapel ? "Capela do Anjo"
            : (clearing->village ? "Chã da Tristeza"
            : (clearing->cemetery ? "Cemitério das Raízes" : "Clareira do Selo"))));
        const char *bossTitle = clearing->necromancerArena ? "Necromante" :
            clearing->boneland ? boneLordName : (clearing->templarField || clearing->village || clearing->chapel || clearing->basement || clearing->invertedChapel) ? NULL
            : (clearing->cemetery ? "Coveiro das Raízes" : "Guardião da Clareira");
        int wave = e->progress.stage;
        if (clearing->necromancerArena || clearing->village || clearing->chapel || clearing->basement || clearing->invertedChapel)
            wave = 0;
        else if (wave == CLEARING_BOSS_STAGE)
            wave = CampaignWaveCount(e->progress.map) + 1;
        BeginStagePresentation(&e->presentation, wave, areaTitle,
            e->progress.stage==CLEARING_BOSS_STAGE ? bossTitle : NULL);
        if (clearing->necromancerArena) {
            e->presentation.areaSubtitle = "O Grande Selo Profano";
            e->presentation.portraitCenterX = .72f;
        }
        if (wave > 0) e->presentation.waveTotal = CampaignWaveCount(e->progress.map) + 1;
        if (clearing->boneland) {
            e->presentation.portraitCenterX = .74f;
        }
        if (clearing->templarField) {
            e->presentation.areaSubtitle = "A última defesa do palácio";
            if (wave <= TEMPLAR_FIELD_WAVES)
                e->presentation.announcement = wave == 1 ? "A Linha de Frente" : "O Último Estandarte";
        }
        if (clearing->cemetery || clearing->village) e->presentation.portraitCenterX=.73f;
        e->guardianIntroPending=(e->progress.map == 0 || clearing->boneland) &&
                                e->progress.stage==CLEARING_BOSS_STAGE &&
                                !e->progress.guardianIntroSeen;
        if (e->progress.stage == CLEARING_BOSS_STAGE) {
            StartArenaBoss(enemies, clearing, player, e->progress.bossPower);
            if (clearing->templarField) {
                e->phase = ENCOUNTER_IGRIS_APPROACH;
                enemies->list[0].state = ENEMY_GONE;
            }
        } else StartEnemyWave(enemies, clearing, e->progress.stage, player, e->progress.damage);
    }
}

void BeginEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                    RunProgress progress, Vector2 player) {
    progress.map = ClearingMapId(clearing);
    if (IsWindowReady()) EnsureEnemyMapAssets(enemies, progress.map);
    if (progress.map > 0) progress.guardianAftermath = GUARDIAN_PASSAGE_COMPLETE;
    if ((clearing->village || clearing->chapel || clearing->basement || clearing->invertedChapel) && progress.stage < CLEARING_BOSS_STAGE)
        progress.stage = CLEARING_BOSS_STAGE;
    *e = (Encounter){.progress=progress, .sealEnemy=-1};
    e->chapelAltarDone=progress.angelSceneCompleted;
    StartStage(e, enemies, clearing, player);
    if (clearing->templarField && progress.stage==CLEARING_BOSS_STAGE && progress.igrisAftermathPending) {
        enemies->list[0].health=0;
        e->phase=ENCOUNTER_IGRIS_AFTERMATH;
        e->presentation=(StagePresentation){0};
    } else if (clearing->village && progress.stage==CLEARING_BOSS_STAGE && progress.villageAftermath!=VILLAGE_DUEL) {
        enemies->list[0].health=0;
        enemies->list[0].state=ENEMY_FALLEN;
        e->phase=progress.villageAftermath==VILLAGE_SCENE_PENDING ? ENCOUNTER_VILLAGE_LETTER_READY : ENCOUNTER_LETTER;
        e->presentation=(StagePresentation){0};
    } else BeginAreaEntry(&e->presentation);
}

bool EncounterPaused(const Encounter *e) {
    return e->guardianIntroPending || GuardianLiberationPending(e) || GuardianPassagePending(e) ||
           e->presentation.remaining>0 || e->phase == ENCOUNTER_REWARD ||
           e->phase == ENCOUNTER_SEAL || e->phase == ENCOUNTER_VILLAGE_CINEMATIC || e->phase == ENCOUNTER_LETTER ||
           e->phase == ENCOUNTER_ANGEL_CINEMATIC || e->phase == ENCOUNTER_CHAPEL_TEXT || e->phase == ENCOUNTER_BASEMENT_PILLAR ||
           e->phase == ENCOUNTER_DEFEAT || e->phase == ENCOUNTER_IGRIS_CINEMATIC ||
           e->phase == ENCOUNTER_IGRIS_AFTERMATH;
}

static bool CanUseAngelRevival(const Encounter *e) {
    return e->phase==ENCOUNTER_FIGHT && e->progress.angelSceneCompleted &&
           e->progress.map>=INVERTED_CHAPEL_MAP && e->progress.angelBlessingUses>0;
}

bool OpenAngelRevival(const Encounter *e, unsigned seed, LogicMCQuestion *question) {
    if (!CanUseAngelRevival(e)) return false;
    int minScore, maxScore;
    EncounterSealRange(e->progress.map, true, &minScore, &maxScore);
    LogicRng rng;
    LogicRngSeed(&rng, seed);
    return LogicGenerateMC(&rng, minScore, maxScore, CurrentEncounterLogicStyle(), question);
}

bool AnswerAngelRevival(Encounter *e, const LogicMCQuestion *question, int choice, int *health) {
    if (*health>0 || choice<0 || choice>=LOGIC_CHOICE_COUNT || !CanUseAngelRevival(e))
        return false;
    e->progress.angelBlessingUses=0;
    if (choice!=question->correct) {
        e->phase=ENCOUNTER_DEFEAT;
        return false;
    }
    *health=(e->progress.maxHealth+1)/2;
    e->progress.health=*health;
    if (!e->practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING, "Nao foi possivel salvar a ressurreicao");
    return true;
}

void CompleteAngelCinematic(Encounter *e, int health) {
    if (e->phase!=ENCOUNTER_ANGEL_CINEMATIC || e->progress.angelSceneCompleted) return;
    e->chapelAltarDone=true;
    e->progress.angelSceneCompleted=true;
    e->progress.angelBlessingUses=1;
    e->progress.health=health;
    e->phase=ENCOUNTER_FIGHT;
    snprintf(e->notice,sizeof(e->notice),"Bênção recebida. A passagem à esquerda está aberta.");
    if (!e->practice) SaveClearingCheckpoint(&e->progress);
}

bool ReadVillageLetter(Encounter *e, const EnemyGroup *enemies, Vector2 player) {
    if (e->phase!=ENCOUNTER_VILLAGE_LETTER_READY ||
        Vector2Distance(player,enemies->list[0].position)>115) return false;
    e->phase=ENCOUNTER_VILLAGE_CINEMATIC;
    return true;
}

bool PutAwayVillageLetter(Encounter *e, EnemyGroup *enemies, const Clearing *map, Vector2 player) {
    if (e->phase!=ENCOUNTER_LETTER) return false;
    e->villageHordeStarted=true;
    e->phase=ENCOUNTER_FIGHT;
    StartVillageHorde(enemies,map,player);
    BeginStagePresentation(&e->presentation,0,"Os aldeões mortos erguem-se da terra!",NULL);
    e->presentation.announcement="A MALDIÇÃO DESPERTA";
    e->presentation.duration=e->presentation.remaining=2.2f;
    return true;
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
    if (cinematicPlayed) {
        if (e->presentation.areaEntry && e->presentation.mapName) {
            // A cutscene já apresentou o chefe, mas a cartela do mapa ainda está na fila.
            e->presentation.wave=0;
            e->presentation.boss=false;
            e->presentation.bossName=NULL;
        } else e->presentation=(StagePresentation){0};
    }
    if (e->progress.map==1) return;
    e->progress.guardianIntroSeen=true;
    if (!e->practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING,"Nao foi possivel salvar a conclusao da cutscene do Guardiao.");
}

void CompleteIgrisIntro(Encounter *e, EnemyGroup *enemies) {
    if (e->phase != ENCOUNTER_IGRIS_CINEMATIC && e->phase != ENCOUNTER_IGRIS_APPROACH) return;
    Enemy *boss = &enemies->list[0];
    boss->state = ENEMY_IDLE;
    boss->stateTime = 0;
    boss->attackCooldown = 1.6f;
    ClearEnemyAttacks(enemies);
    e->phase = ENCOUNTER_FIGHT;
    e->presentation = (StagePresentation){0};
}

bool BeginIgrisAftermath(Encounter *e, EnemyGroup *enemies, int health) {
    const Enemy *boss = &enemies->list[0];
    if (e->phase != ENCOUNTER_FIGHT || e->progress.map != TEMPLAR_FIELD_MAP ||
        e->progress.stage != CLEARING_BOSS_STAGE || boss->templarRole != TEMPLAR_MARSHAL ||
        !boss->igrisFinalForm || boss->state != ENEMY_DIE || boss->health > 0 || health <= 0)
        return false;
    ClearEnemyAttacks(enemies);
    enemies->list[0].state = ENEMY_GONE;
    enemies->list[0].pickupAvailable = false;
    e->phase = ENCOUNTER_IGRIS_AFTERMATH;
    e->progress.igrisAftermathPending = true;
    e->progress.health = health;
    if (!e->practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING, "Nao foi possivel salvar a derrota de Igris.");
    return true;
}

bool CompleteIgrisAftermath(Encounter *e, EnemyGroup *enemies, Clearing *map,
                            Vector2 *player, int health) {
    if (e->phase != ENCOUNTER_IGRIS_AFTERMATH) return false;
    bool practice = e->practice;
    e->progress.igrisAftermathPending = false;
    e->progress.health = health;
    e->progress.stage = CLEARING_COMPLETE_STAGE;
    e->phase = ENCOUNTER_CLEAR;
    map->exitLocked = false;
    Clearing castle = {0};
    bool entered = InitCampaignMap(&castle, NECROMANCER_MAP);
    if (entered) {
        RunProgress progress = e->progress;
        progress.map = NECROMANCER_MAP;
        progress.stage = CLEARING_BOSS_STAGE;
        progress.bossPower = 1;
        UnloadClearing(map);
        *map = castle;
        *player = castle.spawn;
        BeginEncounter(e, enemies, map, progress, *player);
        e->practice = practice;
    }
    if (!practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING, "Nao foi possivel salvar a passagem de Igris ao castelo.");
    return entered;
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
           e->phase != ENCOUNTER_IGRIS_APPROACH && e->phase != ENCOUNTER_IGRIS_CINEMATIC &&
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
        r.amount = EPIC_STATUS_HEALTH_BONUS;
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
    if (IsEpicRewardStage(e)) {
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

static Rectangle RitualPanel(int index) {
    float w = GetScreenWidth(), h = GetScreenHeight();
    bool stacked = w < h * 1.25f;
    float gap = Clamp(w * .012f, 6, 16);
    float width = stacked ? fminf(w - 2 * gap, (h - 3 * gap) * .75f)
                          : fminf((w - 3 * gap) * .5f, (h - 2 * gap) * 1.5f);
    width = fminf(width, 740);
    float height = width / 1.5f;
    if (stacked)
        return (Rectangle){(w-width)*.5f, (h-2*height-gap)*.5f+index*(height+gap), width, height};
    return (Rectangle){(w-2*width-gap)*.5f+index*(width+gap), (h-height)*.5f, width, height};
}

static Rectangle RitualArea(Rectangle panel, float x, float y, float width, float height) {
    return (Rectangle){panel.x+x*panel.width, panel.y+y*panel.height,
                       width*panel.width, height*panel.height};
}

static Rectangle ChoiceRect(int index, int count) {
    Rectangle panel = RitualPanel(1);
    float row = .48f/count;
    return RitualArea(panel,.19f,.275f+index*row,.62f,row-.016f);
}

static void DrawRitualText(const char *text, Rectangle area, float size, Color color) {
    // Wrap at a space near the middle, preserving UTF-8 operators and words.
    char wrapped[384];
    int length = snprintf(wrapped,sizeof(wrapped),"%s",text);
    if (length > 0 && length < (int)sizeof(wrapped) &&
        MeasureGameTextEx(wrapped,size,GAME_TEXT_BODY).x > area.width && area.height >= 28) {
        int split = -1;
        for (int i = 1; i < length-1; i++)
            if (wrapped[i]==' ' && (split<0 || abs(i-length/2)<abs(split-length/2))) split=i;
        if (split>=0) wrapped[split]='\n';
    }
    DrawGameTextIn(wrapped,area,size,color,GAME_TEXT_BODY);
}

void DrawRitualQuestion(const char *title, const char *prompt, const char *detail,
                        const char *const choices[], int count) {
    if (!ritualFramesAttempted) {
        ritualFramesAttempted = true;
        ritualFrames[0] = LoadRewardArtwork("assets/ui/ritual_proposition_frame.png",false);
        ritualFrames[1] = LoadRewardArtwork("assets/ui/ritual_answers_frame.png",false);
    }
    Rectangle question = RitualPanel(0), answers = RitualPanel(1);
    for (int i = 0; i < 2; i++) {
        Texture2D art = ritualFrames[i];
        Rectangle panel = i ? answers : question;
        if (art.id)
            DrawTexturePro(art,(Rectangle){0,0,art.width,art.height},panel,(Vector2){0},0,WHITE);
        else {
            DrawRectangleRec(panel,(Color){34,16,29,255});
            DrawRectangleLinesEx(panel,2,GOLD);
        }
    }
    float font = Clamp(question.width*.059f,16,34);
    DrawGameTextIn(title,RitualArea(question,.19f,.267f,.62f,.105f),font,GOLD,GAME_TEXT_TITLE);
    DrawRitualText(prompt,RitualArea(question,.17f,.39f,.66f,.18f),font,RAYWHITE);
    if (detail && detail[0])
        DrawRitualText(detail,RitualArea(question,.17f,.57f,.66f,.09f),font*.72f,GOLD);

    for (int i = 0; i < count; i++) {
        Rectangle box = ChoiceRect(i,count);
        bool hover = CheckCollisionPointRec(GetMousePosition(),box);
        DrawRectangleRec(box,hover ? (Color){87,34,91,245} : (Color){44,20,42,239});
        DrawRectangleLinesEx(box,hover ? 2 : 1,hover ? (Color){246,156,250,255} : (Color){149,83,122,255});
        DrawRitualText(TextFormat("%d. %s",i+1,choices[i]),
                       (Rectangle){box.x+6,box.y+3,box.width-12,box.height-6},font*.86f,RAYWHITE);
    }
}

static Rectangle RewardCardRect(int index, int count) {
    float sw = GetScreenWidth(), sh = GetScreenHeight();
    float gap = Clamp(sw * .014f, 7, 18);
    float width = fminf(356, fminf((sw - gap * (count + 1)) / count, sh * .55f));
    float height = width * .79f;
    float left = (sw - width * count - gap * (count - 1)) * .5f;
    return (Rectangle){left + index * (width + gap), sh * .54f - height * .5f, width, height};
}

static Rectangle RerollButtonRect(int index, int count) {
    Rectangle card = RewardCardRect(index, count);
    float width = card.width * .50f;
    return (Rectangle){card.x + (card.width - width) * .5f,
                       card.y + card.height + 6, width, width * .29f};
}

bool RerollEncounterReward(Encounter *e, int slot) {
    if (e->phase != ENCOUNTER_REWARD || IsEpicRewardStage(e) ||
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

static int ChoiceInput(int count, Rectangle (*choiceRect)(int, int)) {
    for (int i=0; i<count; i++) {
        if (IsKeyPressed(KEY_ONE+i)) return i;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(),choiceRect(i,count))) return i;
    }
    return -1;
}

int ReadRitualChoice(int count) {
    return ChoiceInput(count,ChoiceRect);
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
            int added = reward.amount > 0 ? reward.amount : EPIC_STATUS_HEALTH_BONUS;
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
    bool lastWave = e->progress.stage == CampaignWaveCount(e->progress.map);
    e->progress.stage = lastWave ? CLEARING_BOSS_STAGE : e->progress.stage + 1;
    if (lastWave && clearing->boneland) e->progress.guardianIntroSeen=false;
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
        bool isBoss = (enemies->list[i].type == ENEMY_BOSS);
        int minScore = 0, maxScore = 0;
        EncounterSealRange(e->progress.map, isBoss, &minScore, &maxScore);
        LogicStyle style = CurrentEncounterLogicStyle();
        static unsigned sealCounter = 0;
        sealCounter++;
        unsigned r0 = (unsigned)GetRandomValue(0, 32767);
        unsigned r1 = (unsigned)GetRandomValue(0, 32767);
        unsigned r2 = (unsigned)GetRandomValue(0, 32767);
        unsigned seed = (r0 | (r1 << 15) | (r2 << 30)) ^
                        (sealCounter * 2654435761u) ^
                        ((unsigned)(e->progress.map + 1) * 40503u) ^
                        ((unsigned)(e->progress.stage + 1) * 65537u) ^
                        ((unsigned)(e->progress.finalSealFailures + 1) * 131u);
        LogicRng rng;
        LogicRngSeed(&rng, seed);
        e->sealQuestion = (LogicSealQuestion){.isBoss = isBoss};
        if (isBoss) {
            LogicGenerateMC(&rng, minScore, maxScore, style, &e->sealQuestion.mc);
            e->sealQuestion.correct = e->sealQuestion.mc.correct;
            e->sealQuestion.score = e->sealQuestion.mc.questionScore;
        } else {
            LogicGenerateVF(&rng, minScore, maxScore, style, &e->sealQuestion.vf);
            e->sealQuestion.correct = e->sealQuestion.vf.correct;
            e->sealQuestion.score = e->sealQuestion.vf.score;
        }
        e->answer = e->sealQuestion.correct;
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
    int correct=e->answer;
    ClearEnemyAttacks(enemies);
    if (e->progress.map==NECROMANCER_MAP) {
        if (choice!=correct) e->progress.finalSealFailures++;
        bool finalAttempt=e->progress.finalSealFailures>=3;
        if (choice==correct) {
            enemy->state=ENEMY_GONE;
            e->progress.stage=CLEARING_COMPLETE_STAGE;
            e->progress.finalSealFailures=0;
            e->progress.health=*health;
            e->phase=ENCOUNTER_CLEAR;
            BeginStagePresentation(&e->presentation,0,
                "O Grande Selo Profano foi restaurado",NULL);
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
    if (choice==correct) {
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
            enemy->igrisFinalForm = false;
            enemy->guardianMelee = false;
            enemy->guardianMeleeCooldown = 0;
            enemy->attackBurst = 0;
            enemy->bossAttack = enemy->templarRole == TEMPLAR_MARSHAL ? IGRIS_MELEE :
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
    if (e->phase == ENCOUNTER_IGRIS_APPROACH) {
        Rectangle stairFoot = {clearing->exit.x, clearing->exit.y + clearing->exit.height,
                               clearing->exit.width, 120};
        if (CheckCollisionPointRec(player, stairFoot)) e->phase = ENCOUNTER_IGRIS_CINEMATIC;
        return false;
    }
    if (e->phase == ENCOUNTER_IGRIS_CINEMATIC || e->phase == ENCOUNTER_IGRIS_AFTERMATH ||
        BeginIgrisAftermath(e,enemies,*health)) return false;
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
            int sel = ReadRitualChoice(2);
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
            e->phase=ENCOUNTER_VILLAGE_LETTER_READY;
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
                e->presentation.announcement = clearing->templarField ? "Igris derrotado"
                    : clearing->boneland ? "Terra dos Ossos limpa" : e->progress.map == 2 ? "CHÃ DA TRISTEZA LIMPA"
                    : (e->progress.map == 1 ? "CEMITERIO LIMPO" : "CLAREIRA LIMPA");
                e->presentation.duration=e->presentation.remaining=2.5f;
                if (clearing->boneland) QueueAreaUnlock(&e->presentation, TEMPLAR_FIELD_TITLE);
                else if (!clearing->templarField) QueueAreaUnlock(&e->presentation, e->progress.map == 2 ? "Capela do Anjo"
                    : (e->progress.map == 1 ? "Chã da Tristeza" : "Cemitério das Raízes"));
                if (!e->practice) SaveClearingCheckpoint(&e->progress);
            } else {
                e->phase=ENCOUNTER_REWARD; e->rerolled=false; RollRewards(e);
            }
        } else if (IsKeyPressed(KEY_E) && !(e->progress.map == 2 && !e->villageHordeStarted)) {
            OpenEncounterSeal(e,enemies,player);
        }
    } else if (e->phase==ENCOUNTER_VILLAGE_LETTER_READY) {
        if (IsKeyPressed(KEY_E))
            ReadVillageLetter(e,enemies,player);
    } else if (e->phase==ENCOUNTER_LETTER) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            return PutAwayVillageLetter(e,enemies,clearing,player);
    } else if (e->phase==ENCOUNTER_REWARD) {
        if (!IsEpicRewardStage(e) && !e->rerolled &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            for (int i = 0; i < e->rewardCount; i++) {
                if (CheckCollisionPointRec(mouse, RerollButtonRect(i, e->rewardCount))) {
                    RerollEncounterReward(e, i);
                    return false;
                }
            }
        }
        int selected=ChoiceInput(e->rewardCount, RewardCardRect);
        if (selected>=0) {
            return ChooseEncounterReward(e,enemies,clearing,player,health,selected);
        }
    } else if (e->phase==ENCOUNTER_SEAL) {
        Enemy *enemy=&enemies->list[e->sealEnemy];
        int selected=ReadRitualChoice(enemy->type==ENEMY_BOSS?3:2);
        if (selected>=0) return AnswerEncounterSeal(e,enemies,health,selected);
    }
    return false;
}

static const char *CommonBuffShortText(int kind) {
    if (kind == REWARD_DAMAGE) return "Dano +15%";
    if (kind == REWARD_HEALTH) return "Vida máx. +20 (cura 20)";
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
        case REWARD_HEALTH: return "Vida máxima\n+20 de vida e cura 20";
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
                if (nextLevel == 1) perk = "Intervalo 0,20->0,14 s, vel. 760->860 px/s";
                else if (nextLevel == 2) perk = "Dano da arma +25% adicional";
                else if (nextLevel == 3) perk = "2 tiros por disparo (+-5 graus)";
                else if (nextLevel == 4) perk = "Tiros perfurantes (até 2 inimigos)";
                else if (nextLevel == 5) perk = "Duas pistolas (4 tiros, +-13 px)";
            } else if (reward.weapon == 1) {
                if (nextLevel == 1) perk = "Empurrão 36 px, dano +15% adicional";
                else if (nextLevel == 2) perk = "Livros 4->5, órbita 90->102 px";
                else if (nextLevel == 3) perk = "Boost 1,25->1,65 s, recarga 4,0->2,8 s";
                else if (nextLevel == 4) perk = "Livros 5->6, bloqueia tiros no boost";
                else if (nextLevel == 5) perk = "Poças de água benta (8 dano/0,45 s)";
            } else if (reward.weapon == 2) {
                if (nextLevel == 1) perk = "Acerto reduz recarga da esquiva em 0,8 s";
                else if (nextLevel == 2) perk = "Alcance 105->123 px, recuperação 0,45->0,40 s";
                else if (nextLevel == 3) perk = "Arco 127->145 graus, recuo 42 px";
                else if (nextLevel == 4) perk = "Onda de choque (22 dano, recarga 2,5 s)";
                else if (nextLevel == 5) perk = "Onda tripla (+-20 graus, recarga 1,8 s)";
            }
            return TextFormat("%s (%d/%d)\n%s\nDano +20%%",
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
            return "Status\nVida +50 / dano +25%\nVelocidade +25%";
        case REWARD_EPIC_FULL_HEAL:
            return "Vida plena\nCura toda a vida\nVida máxima +50";
        default:
            return "";
    }
}

static Rectangle RewardSection(Rectangle box, float x, float y, float w, float h) {
    return (Rectangle){box.x + box.width*x, box.y + box.height*y, box.width*w, box.height*h};
}

static void DrawRewardEffects(char *text, Rectangle area, float size) {
    const char *lines[8];
    int count = 0;
    while (text && *text && count < 8) {
        lines[count++] = text;
        char *end = strchr(text, '\n');
        if (end) *end = '\0';
        char *wrap = NULL;
        for (char *space = strchr(text, ' '); space; space = strchr(space + 1, ' ')) {
            *space = '\0';
            bool fits = MeasureGameTextEx(text, size, GAME_TEXT_BODY).x <= area.width;
            *space = ' ';
            if (!fits) break;
            wrap = space;
        }
        if (wrap && MeasureGameTextEx(text, size, GAME_TEXT_BODY).x > area.width) {
            if (end) *end = '\n';
            *wrap = '\0';
            text = wrap + 1;
        } else {
            text = end ? end + 1 : NULL;
        }
    }
    if (!count) return;
    float step = fminf(size * 1.13f, area.height / count);
    area.y += (area.height - count * step) * .5f;
    area.height = step;
    for (int i = 0; i < count; i++) {
        DrawGameTextIn(lines[i], area, fminf(step, size), (Color){255, 239, 222, 255}, GAME_TEXT_BODY);
        area.y += step;
    }
}

static void DrawRewardCard(Rectangle box, int slot, const Encounter *e, bool hovered) {
    static const Color fallback[RARITY_COUNT] = {
        {55, 38, 42, 255}, {13, 31, 66, 255}, {43, 14, 61, 255}
    };
    Reward reward = e->rewards[slot];
    int r = (reward.rarity >= 0 && reward.rarity < RARITY_COUNT) ? reward.rarity : RARITY_COMMON;
    Texture2D texture = rewardUiAssets.frames[r];
    Rectangle title = RewardSection(box, .11f, .305f, .78f, .232f);
    Rectangle effects = RewardSection(box, .135f, .594f, .73f, .24f);
    Color titleInk = {255, 225, 161, 255};

    if (texture.id) {
        if (r == RARITY_EPIC) {
            Rectangle backing = RewardSection(box, .102f, .231f, .796f, .643f);
            DrawRectangleRounded(backing, .22f, 10, (Color){37, 17, 46, 255});
        }
        Rectangle art = box;
        if (hovered) {
            art.x -= 3; art.y -= 3; art.width += 6; art.height += 6;
        }
        DrawTexturePro(texture, cardFrameSources[r], art, (Vector2){0}, 0,
                       hovered ? WHITE : (Color){223, 223, 223, 255});
    } else {
        DrawRectangleRounded(RewardSection(box, .02f, .10f, .96f, .86f), .07f, 8, fallback[r]);
        DrawRectangleLinesEx(RewardSection(box, .02f, .10f, .96f, .86f), hovered ? 3 : 1, titleInk);
        DrawGameTextIn(RarityLabel(r), RewardSection(box, .18f, .12f, .64f, .16f), 19, titleInk, GAME_TEXT_BODY);
    }

    char label[256];
    snprintf(label, sizeof(label), "%s", RewardText(e, reward));
    char *details = strchr(label, '\n');
    if (details) *details++ = '\0';
    DrawGameTextIn(TextFormat("%d. %s", slot + 1, label), title,
                   fminf(26, box.width * .102f), titleInk, GAME_TEXT_BODY);
    DrawRewardEffects(details, effects, fminf(21, box.width * .076f));
}

static void DrawRerollButton(Rectangle btn, bool disabled, bool hovered) {
    Color tint = disabled ? (Color){130, 117, 125, 165} : hovered ? WHITE : (Color){226, 226, 226, 255};
    Texture2D texture = rewardUiAssets.rerollButton;
    if (texture.id) {
        DrawTexturePro(texture, (Rectangle){38, 59, 2098, 613}, btn, (Vector2){0}, 0, tint);
    } else {
        DrawRectangleRec(btn, disabled ? DARKGRAY : MAROON);
        DrawRectangleLinesEx(btn, 2, tint);
        DrawGameTextIn("Sortear", btn, 18, tint, GAME_TEXT_BODY);
    }
}

static void DrawRewardScreen(const Encounter *e) {
    if (e->rewardCount <= 0) return;
    EnsureRewardUiAssetsLoaded();
    Rectangle first = RewardCardRect(0, e->rewardCount);
    float width = first.width * 1.55f;
    float height = width * .313f;
    Rectangle banner = {GetScreenWidth() * .5f - width * .5f,
                        first.y - height - first.width * .052f, width, height};
    if (rewardUiAssets.banner.id) {
        DrawTexturePro(rewardUiAssets.banner, (Rectangle){47, 7, 2080, 653},
                       banner, (Vector2){0}, 0, WHITE);
    } else {
        DrawGameTextIn("Escolha seu bônus", banner, 42, GOLD, GAME_TEXT_TITLE);
    }
    bool canReroll = !IsEpicRewardStage(e);
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < e->rewardCount; i++) {
        Rectangle box = RewardCardRect(i, e->rewardCount);
        DrawRewardCard(box, i, e, CheckCollisionPointRec(mouse, box));
        if (canReroll) {
            Rectangle button = RerollButtonRect(i, e->rewardCount);
            DrawRerollButton(button, e->rerolled, !e->rerolled && CheckCollisionPointRec(mouse, button));
        }
    }
    if (canReroll && e->rerolled) {
        Rectangle button = RerollButtonRect(0, e->rewardCount);
        DrawGameTextIn("Sorteio já usado", (Rectangle){0, button.y + button.height + 3, GetScreenWidth(), 22},
                       16, (Color){226, 197, 169, 255}, GAME_TEXT_BODY);
    }
}

void DrawEncounter(const Encounter *e, const EnemyGroup *enemies) {
    if (e->presentation.remaining>0) {
        Texture2D bossSprite = e->progress.map == BONELAND_MAP ? enemies->boneLordPortrait
            : e->progress.map == 2 ? enemies->villageBossWalk
            : (e->progress.map == 1 ? enemies->cemeteryPortrait : enemies->bossPortrait);
        if (e->progress.map == TEMPLAR_FIELD_MAP) bossSprite = enemies->igrisArt[IGRIS_IDLE_ART];
        if (e->progress.map == NECROMANCER_MAP) bossSprite = enemies->necromancerPortrait;
        bool animatedPortrait = e->progress.map == 2;
        int cols = animatedPortrait ? 6 : e->progress.map == TEMPLAR_FIELD_MAP ? 8 : 1;
        int rows = cols > 1 ? 4 : 1;
        DrawStagePresentation(&e->presentation, bossSprite, cols, rows);
        return;
    }
    bool bossVisible = EncounterBossVisible(e, enemies);
    int statusY = 205;
    const char *mapLabel = e->progress.map == BONELAND_MAP ? "Terra dos Ossos" : e->progress.map == 5 ? "Capela Invertida"
        : (e->progress.map == 4 ? "Porão da Capela"
        : (e->progress.map == 3 ? "Capela do Anjo"
        : (e->progress.map == 2 ? "Chã da Tristeza"
        : (e->progress.map == 1 ? "Cemitério" : "Clareira"))));
    const char *clearLabel = e->progress.map == 2 ? "Chã da Tristeza limpa - siga pelo arco ao topo"
        : (e->progress.map == 1 ? "Cemitério limpo - siga pela saída à direita"
                                : "Clareira limpa - altar liberado");
    const char *bossLabel = e->progress.map == NECROMANCER_MAP ? "Necromante" : e->progress.map == BONELAND_MAP ? boneLordName
        : e->progress.map == TEMPLAR_FIELD_MAP ? IGRIS_TITLE
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
        const char *status = e->phase == ENCOUNTER_CLEAR ? "Igris derrotado"
            : e->phase == ENCOUNTER_IGRIS_APPROACH ? "Aproxime-se da escadaria"
            : e->progress.stage == CLEARING_BOSS_STAGE ? "Encontro 3/3 - " IGRIS_TITLE
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
    else if (e->progress.stage<=CampaignWaveCount(e->progress.map)) DrawGameText(TextFormat("%s - onda %d/%d | Restantes: %d",
        mapLabel,e->progress.stage,CampaignWaveCount(e->progress.map)+1,EnemiesRemaining(enemies)),24,statusY,20,GOLD);
    else DrawGameText(e->phase==ENCOUNTER_CLEAR ? clearLabel :
        (e->progress.map == 2 ? (e->villageHordeStarted ?
            TextFormat("Horda da Vila - Restantes: %d", EnemiesRemaining(enemies)) :
            e->phase==ENCOUNTER_VILLAGE_LETTER_READY ? "Aproxime-se da carta · E ou clique para ler" :
            "Duelo - Líder da Vila")
            : TextFormat("Onda %d/%d - %s", CampaignWaveCount(e->progress.map)+1,
                         CampaignWaveCount(e->progress.map)+1, bossLabel)),24,statusY,20,GOLD);
    if (e->notice[0]) DrawGameText(e->notice,24,statusY+27,18,RAYWHITE);
    if (bossVisible) {
        const Enemy *boss=&enemies->list[0];
        bool phaseTwo = boss->guardianPhase != GUARDIAN_PHASE_ONE;
        const char *phase = boss->guardianPhase == GUARDIAN_PHASE_TRANSITION ? "Despertando..." :
                            phaseTwo ? "Fase 2/2" : "Fase 1/2";
        if(boss->necromancerForm && boss->guardianPhase!=GUARDIAN_PHASE_TRANSITION)
            phase=TextFormat("Fase %d/3",boss->necromancerForm);
        if (boss->templarRole == TEMPLAR_MARSHAL)
            phase = boss->guardianPhase == GUARDIAN_PHASE_TRANSITION ? "Transformando..." :
                boss->igrisFinalForm ? "Fase 3/3" : phaseTwo ? "Fase 2/3" : "Fase 1/3";
        Color healthColor=boss->necromancerForm==3 ? (Color){211,48,92,255} :
            boss->necromancerForm==2 || phaseTwo ? PURPLE : MAROON;
        float markers[] = {.5f, IGRIS_FINAL_HEALTH_PERCENT / 100.0f};
        if (boss->necromancerForm) { markers[0] = .70f; markers[1] = .35f; }
        int markerCount = boss->necromancerForm || boss->templarRole == TEMPLAR_MARSHAL ? 2 : 1;
        DrawBossHealthHud(bossLabel, phase, boss->health, boss->maxHealth, healthColor, markers, markerCount,
                          e->practice ? PRACTICE_FOOTER_HEIGHT : 0);
    }
    if (e->phase == ENCOUNTER_IGRIS_CINEMATIC || e->phase == ENCOUNTER_IGRIS_AFTERMATH) return;
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
        DrawGameTextIn(titles[tid], (Rectangle){(float)bx, (float)(by + 18), (float)bw, 34}, 28, GOLD, GAME_TEXT_TITLE);
        for (int i = 0; i < 6; i++) {
            if (bodies[tid][i][0]) DrawGameText(bodies[tid][i], bx + 36, by + 84 + i * 32, 20, RAYWHITE);
        }
        const char *hint = "[Espaço / Enter / Clique] Continuar";
        DrawGameText(hint, (sw - MeasureGameText(hint, 19)) / 2, by + bh - 44, 19, GOLD);
        return;
    }
    if (e->phase == ENCOUNTER_BASEMENT_PILLAR) {
        static const char *pnames[] = {
            "Pilar Noroeste (Sup. Esq.)", "Pilar Nordeste (Sup. Dir.)",
            "Pilar Sudoeste (Inf. Esq.)", "Pilar Sudeste (Inf. Dir.)"
        };
        int t = (e->basementTarget >= 0 && e->basementTarget < 4) ? e->basementTarget : 0;
        const char *choices[] = {"Verdadeiro (V)", "Falso (F)"};
        DrawRitualQuestion(pnames[t],"Qual valor as placas indicam para este pilar?",
                           "Esc para voltar",choices,2);
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
            DrawGameTextIn("Largar a carta · Espaço / Enter / E / Clique",
                (Rectangle){0,sh-hintHeight,sw,hintHeight},20,GOLD,GAME_TEXT_BODY);
            return;
        }
        int bw = (int)fminf(720.0f, sw - 80.0f), bh = 380;
        int bx = (sw - bw) / 2, by = (sh - bh) / 2;
        DrawRectangle(bx, by, bw, bh, (Color){38, 28, 22, 245});
        DrawRectangleLinesEx((Rectangle){(float)bx, (float)by, (float)bw, (float)bh}, 2, GOLD);
        const char *ltitle = "Último Registro do Líder da Vila";
        DrawGameTextIn(ltitle, (Rectangle){(float)bx, (float)(by + 18), (float)bw, 34}, 28, GOLD, GAME_TEXT_TITLE);
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
    if (e->phase == ENCOUNTER_REWARD) {
        DrawRewardScreen(e);
        return;
    }
    if (e->phase == ENCOUNTER_DEFEAT) {
        float width = GetScreenWidth(), center = GetScreenHeight()*.5f;
        DrawGameTextIn("Fim da tentativa",(Rectangle){0,center-160,width,49},42,GOLD,GAME_TEXT_TITLE);
        DrawGameTextIn("Enter: repetir | Esc: voltar ao menu",
                       (Rectangle){0,center-80,width,38},20,RAYWHITE,GAME_TEXT_BODY);
        return;
    }
    if (e->phase != ENCOUNTER_SEAL) return;

    bool boss = enemies->list[e->sealEnemy].type == ENEMY_BOSS;
    bool final = e->progress.map == NECROMANCER_MAP;
    const char *title = final ? "Grande Selo Profano" : "Selo";
    if (boss) {
        const LogicMCQuestion *question = &e->sealQuestion.mc;
        const char *choices[] = {question->choices[0],question->choices[1],question->choices[2]};
        char hint[64] = "Escolha a expressão equivalente";
        if (final) snprintf(hint,sizeof(hint),"Expressão equivalente · Tentativa %d/3",
                            e->progress.finalSealFailures+1);
        DrawRitualQuestion(title,question->questionText,hint,choices,3);
    } else {
        const char *choices[] = {"Verdadeiro","Falso"};
        DrawRitualQuestion(title,e->sealQuestion.vf.exprText,e->sealQuestion.vf.valuesText,choices,2);
    }
}
