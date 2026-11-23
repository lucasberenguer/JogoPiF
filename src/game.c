#include "game_text.h"
#include "raylib.h"
#include "raymath.h"
#include "clearing.h"
#include "hud.h"
#include "speedrun.h"
#include "menu.h"
#include "weapon_select.h"
#include "bible.h"
#include "gun.h"
#include "enemy.h"
#include "encounter.h"
#include "cutscene.h"
#include "guardian_cutscene.h"
#include "dev_tools.h"
#include "video_settings.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int DrawAngelRevival(const LogicMCQuestion *question) {
    const char *choices[] = {question->choices[0],question->choices[1],question->choices[2]};
    DrawRitualQuestion("A bênção do Anjo",question->questionText,
                       "Escolha a equivalente para voltar com metade da vida.",choices,3);
    return ReadRitualChoice(3);
}

static Camera2D FitClearing(const Clearing *clearing) {
    const float mapWidth = (float) clearing->background.width;
    const float mapHeight = (float) clearing->background.height;
    const float widthScale = (float) GetScreenWidth() / mapWidth;
    const float heightScale = (float) GetScreenHeight() / mapHeight;
    Camera2D camera = { 0 };
    camera.offset = (Vector2) { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    camera.zoom = widthScale < heightScale ? widthScale : heightScale;
    camera.target = (Vector2) { mapWidth / 2.0f, mapHeight / 2.0f };
    return camera;
}

static void MoveWithCollision(const Clearing *clearing, Vector2 *position,
                              Vector2 movement) {
    MoveMapPlayer(clearing,position,movement);
}

static Vector2 FacingVector(int facing) {
    switch (facing) {
        case 1: return (Vector2) { 0.0f, -1.0f };
        case 2: return (Vector2) { -1.0f, 0.0f };
        case 3: return (Vector2) { 1.0f, 0.0f };
        default: return (Vector2) { 0.0f, 1.0f };
    }
}

static Vector2 RotateVector2(Vector2 dir, float radians) {
    float c = cosf(radians), s = sinf(radians);
    return (Vector2){dir.x * c - dir.y * s, dir.x * s + dir.y * c};
}

#define KATANA_BASE_RANGE 105.0f
#define KATANA_L2_RANGE 123.0f
#define KATANA_BASE_RECOVERY 0.45f
#define KATANA_L2_RECOVERY 0.40f
#define KATANA_BASE_ARC_DOT 0.4462f
#define KATANA_L3_ARC_DOT 0.3007f
#define KATANA_L3_KNOCKBACK 42.0f
#define KATANA_L1_DODGE_REDUCTION 0.8f
#define KATANA_MAX_SHOCKWAVES 12
#define KATANA_SHOCKWAVE_COOLDOWN 2.5f
#define KATANA_L5_SHOCKWAVE_COOLDOWN 1.8f
#define KATANA_L5_FAN_ANGLE_DEG 20.0f
#define KATANA_SHOCKWAVE_DAMAGE 22
#define KATANA_SHOCKWAVE_SPEED 620.0f
#define KATANA_SHOCKWAVE_LIFE 0.95f
#define KATANA_SHOCKWAVE_RADIUS 32.0f

typedef struct KatanaShockwave {
    Vector2 position;
    Vector2 direction;
    float life;
    bool hitEnemies[ENEMY_COUNT];
} KatanaShockwave;

typedef struct KatanaSprites {
    Texture2D idle;
    Texture2D walk;
    Texture2D attack[3];
} KatanaSprites;

static void UnloadKatanaSprites(KatanaSprites *sprites) {
    if (sprites->idle.id) UnloadTexture(sprites->idle);
    if (sprites->walk.id) UnloadTexture(sprites->walk);
    for (int i = 0; i < 3; i++)
        if (sprites->attack[i].id) UnloadTexture(sprites->attack[i]);
    *sprites = (KatanaSprites){0};
}

static bool InitKatanaSprites(KatanaSprites *sprites) {
    *sprites = (KatanaSprites){0};
    sprites->idle = LoadTexture("assets/katana_idle_novo.png");
    sprites->walk = LoadTexture("assets/katana_caminhada.png");
    for (int i = 0; i < 3; i++)
        sprites->attack[i] = LoadTexture(TextFormat("assets/katana_golpe_%d.png", i + 1));
    if (!sprites->idle.id || !sprites->walk.id) { UnloadKatanaSprites(sprites); return false; }
    SetTextureFilter(sprites->idle, TEXTURE_FILTER_POINT);
    SetTextureFilter(sprites->walk, TEXTURE_FILTER_POINT);
    for (int i = 0; i < 3; i++) {
        if (!sprites->attack[i].id) { UnloadKatanaSprites(sprites); return false; }
        SetTextureFilter(sprites->attack[i], TEXTURE_FILTER_POINT);
    }
    return true;
}

int main(void) {
    const char *applicationDirectory = GetApplicationDirectory();
    if (DirectoryExists(TextFormat("%s/assets", applicationDirectory)) &&
        !ChangeDirectory(applicationDirectory))
        TraceLog(LOG_WARNING, "Nao foi possivel abrir a pasta de assets do executavel.");

    InitWindow(1280, 720, "Jogo PIF - Clareira");
    /* Opções de vídeo salvas em video_pif.cfg (padrão: tela cheia sem borda, 60 FPS, 100%). */
    VideoSettings *video = GetVideoSettings();
    LoadVideoSettings(video);
    if (video->fullscreen) ToggleGameFullscreen();
    else ApplyVideoWindowSize();
    SetExitKey(KEY_NULL);
    SetTargetFPS(video->fpsLimit);

    Clearing clearing = { 0 };
    if (!InitClearing(&clearing)) {
        CloseWindow();
        return 1;
    }

    Texture2D protagonist = LoadTexture("assets/protagonista.png");
    if (protagonist.id == 0) {
        TraceLog(LOG_ERROR, "Nao foi possivel carregar assets/protagonista.png");
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }
    SetTextureFilter(protagonist, TEXTURE_FILTER_POINT);

    Texture2D dodgeSprites = LoadTexture("assets/protagonista_dash.png");
    if (dodgeSprites.id == 0) {
        TraceLog(LOG_ERROR, "Nao foi possivel carregar assets/protagonista_dash.png");
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }
    SetTextureFilter(dodgeSprites, TEXTURE_FILTER_POINT);

    KatanaSprites katanaSprites = {0};
    if (!InitKatanaSprites(&katanaSprites)) {
        TraceLog(LOG_ERROR, "Nao foi possivel carregar os sprites da katana");
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }

    Hud hud = { 0 };
    if (!InitHud(&hud)) {
        UnloadKatanaSprites(&katanaSprites);
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }

    Menu menu = { 0 };
    if (!InitMenu(&menu)) {
        UnloadKatanaSprites(&katanaSprites);
        UnloadHud(&hud);
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }
    WeaponSelect weaponSelect = { 0 };
    if (!InitWeaponSelect(&weaponSelect)) {
        UnloadKatanaSprites(&katanaSprites);
        UnloadMenu(&menu);
        UnloadHud(&hud);
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }
    Bible bible = { 0 };
    if (!InitBible(&bible)) {
        UnloadKatanaSprites(&katanaSprites);
        UnloadWeaponSelect(&weaponSelect);
        UnloadMenu(&menu);
        UnloadHud(&hud);
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }
    Gun gun = { 0 };
    if (!InitGun(&gun)) {
        UnloadKatanaSprites(&katanaSprites);
        UnloadBible(&bible);
        UnloadWeaponSelect(&weaponSelect);
        UnloadMenu(&menu);
        UnloadHud(&hud);
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }
    EnemyGroup enemies = { 0 };
    if (!InitEnemies(&enemies)) {
        UnloadGun(&gun);
        UnloadBible(&bible);
        UnloadWeaponSelect(&weaponSelect);
        UnloadMenu(&menu);
        UnloadHud(&hud);
        UnloadKatanaSprites(&katanaSprites);
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }

    LoadGameFonts();
    Vector2 player = clearing.spawn;
    int playerHealth = 100;
    int playerMaxHealth = 100;
    float playerSpeed = 204.0f;
    const float dodgeSpeed = 460.0f;
    const float dodgeDuration = 0.36f;
    const float dodgeRecovery = 5.0f;
    float dodgeRemaining = 0.0f;
    float dodgeCooldown = 0.0f;
    Vector2 dodgeDirection = { 0.0f, 1.0f };
    int facing = 0; // 0: baixo, 1: cima, 2: esquerda, 3: direita
    int dodgeFacing = 0;
    float walkTime = 0.0f;
    bool walking = false;
    bool showCollisionGuide = false;
    bool showSpriteDebug = false;
    float pedestalMessageTime = 0.0f;
    const float katanaAttackDuration = 0.42f;
    float katanaAttackRemaining = 0.0f;
    float katanaAttackCooldown = 0.0f;
    float katanaShockwaveCooldown = 0.0f;
    KatanaShockwave katanaShockwaves[KATANA_MAX_SHOCKWAVES] = {0};
    bool katanaDashReducedThisSwing = false;
    int katanaAttackFacing = 0;
    Vector2 katanaAttackDirection = {0.0f, 1.0f};
    int gunAttackFacing = 0;
    int bibleAttackFacing = 0;
    int katanaAttackId = 0;
    enum { OPENING_SCREEN = 4, GUARDIAN_SCREEN = 5 };
    int screen = 0; /* 0: menu, 1: escolha da arma, 2: partida, 3: pausa */
    OpeningCutscene opening = {0};
    GuardianCutscene guardianIntro = {0};
    enum { GUARDIAN_STORY_INTRO, GUARDIAN_STORY_LIBERATION, GUARDIAN_STORY_PASSAGE,
           VILLAGE_STORY_AFTERMATH, ANGEL_STORY, IGRIS_STORY_INTRO, IGRIS_STORY_AFTERMATH,
           BONELAND_STORY_AFTERMATH } guardianStory=GUARDIAN_STORY_INTRO;
    DevTools devTools={.wave=1,.selected=6};
    bool devOpeningWasPaused=false,devGuardianWasPaused=false;
    bool openingPreview=false;
    int previewReturnScreen=0;
    bool openingSeen = false;
    bool deathHandled=false, revivalPending=false;
    LogicMCQuestion revivalQuestion={0};
    bool choosingWeaponAfterDeath=false;
    int currentWeapon = -1;
    Encounter encounter = {.progress = {.weapon=0}};
    SpeedrunBoard speedrunBoard;
    SpeedrunLoad(&speedrunBoard);
    bool speedrunRecorded=false;
    bool speedrunNamePending=false;
    char speedrunName[SPEEDRUN_NAME_BYTES]={0};
    int speedrunNameBytes=0, speedrunNameChars=0;
    double completedRunTime=0;
    const float entryFadeDuration = 1.15f;
    float entryFadeRemaining = 0.0f;

    while (!WindowShouldClose()) {
        if ((screen==2 || screen==GUARDIAN_SCREEN || screen==OPENING_SCREEN) &&
            !devTools.open && !openingPreview && playerHealth>0 &&
            !(encounter.progress.map==NECROMANCER_MAP && encounter.phase==ENCOUNTER_CLEAR))
            SpeedrunTick(GetFrameTime());
        bool resolvingDeath=(screen==2 || screen==3) && playerHealth<=0;
        bool toolsBlocked=resolvingDeath || choosingWeaponAfterDeath || speedrunNamePending;
        if (toolsBlocked) devTools.open=false;
        Rectangle toolsButton={16,12,194,38};
        bool toolsClicked=screen==0 && !devTools.open &&
            CheckCollisionPointRec(GetMousePosition(),toolsButton) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool devToggle=!toolsBlocked && (IsKeyPressed(KEY_F1) || toolsClicked);
        bool devWasOpen=devTools.open;
        if (devToggle && !devWasOpen) {
            devTools.open=true;
            devTools.status[0]=0;
            devTools.collision=showCollisionGuide;
            if ((screen==2 || screen==3 || screen==GUARDIAN_SCREEN) && encounter.progress.stage>0) {
                devTools.map=encounter.progress.map;
                devTools.wave=encounter.progress.stage>CLEARING_BOSS_STAGE ? CLEARING_BOSS_STAGE : encounter.progress.stage;
                if (clearing.necromancerArena && enemies.list[0].necromancerForm<3)
                    devTools.wave=enemies.list[0].necromancerForm>0?enemies.list[0].necromancerForm:1;
                devTools.weapon=encounter.progress.weapon;
                devTools.weaponLevel=encounter.progress.weaponLevel[devTools.weapon];
            }
            devOpeningWasPaused=opening.paused;
            devGuardianWasPaused=guardianIntro.paused;
            if (screen==OPENING_SCREEN) SetOpeningCutscenePaused(&opening,true);
            if (screen==GUARDIAN_SCREEN) SetGuardianCutscenePaused(&guardianIntro,true);
        }
        GameTrack track = TRACK_SILENT;
        float musicStrength = 1.0f;
        if (screen == 0) track = TRACK_MENU;
        else if ((screen == 2 || screen == 3) && playerHealth > 0 &&
                 (clearing.chapel || clearing.basement || clearing.invertedChapel)) {
            track = TRACK_CHAPEL;
            if (screen == 3 || EncounterPaused(&encounter)) musicStrength = 0.42f;
        } else if ((screen == 2 || screen == 3) && playerHealth > 0 &&
                 encounter.progress.stage <= CLEARING_BOSS_STAGE) {
            track = (encounter.guardianIntroPending || encounter.phase==ENCOUNTER_VILLAGE_CINEMATIC ||
                encounter.phase==ENCOUNTER_LETTER || encounter.phase==ENCOUNTER_VILLAGE_LETTER_READY ||
                encounter.phase==ENCOUNTER_IGRIS_APPROACH || encounter.phase==ENCOUNTER_IGRIS_CINEMATIC ||
                encounter.phase==ENCOUNTER_IGRIS_AFTERMATH) ? TRACK_SILENT :
                encounter.progress.stage == CLEARING_BOSS_STAGE ? TRACK_MINIBOSS : TRACK_COMBAT;
            if (encounter.progress.map == TEMPLAR_FIELD_MAP &&
                encounter.progress.stage == CLEARING_BOSS_STAGE &&
                (encounter.presentation.remaining > 0 || enemies.list[0].state == ENEMY_SPAWNING))
                track = TRACK_SILENT;
            if (screen == 3 || EncounterPaused(&encounter)) musicStrength = 0.42f;
        }
        if (devTools.open) musicStrength*=.15f;
        UpdateGameMusic(&menu.audio, track, musicStrength, GetFrameTime());
        if (devTools.open) {
            if (screen==2 || screen==3 || screen==GUARDIAN_SCREEN) SpeedrunInvalidate();
            bool inPractice=encounter.practice && (screen==2 || screen==3);
            DevAction action=devToggle && devWasOpen ? DEV_CLOSE : UpdateDevTools(&devTools,inPractice);
            if (action==DEV_CLOSE) {
                devTools.open=false;
                showCollisionGuide=devTools.collision;
                if (screen==OPENING_SCREEN) SetOpeningCutscenePaused(&opening,devOpeningWasPaused);
                if (screen==GUARDIAN_SCREEN) SetGuardianCutscenePaused(&guardianIntro,devGuardianWasPaused);
            } else if (action==DEV_OPENING) {
                OpeningCutscene preview={0};
                if (InitOpeningCutscene(&preview,menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                         menu.audio.volume[AUDIO_EFFECTS]/100.0f)) {
                    previewReturnScreen=screen==OPENING_SCREEN || screen==GUARDIAN_SCREEN ? 0 : screen;
                    UnloadOpeningCutscene(&opening);
                    UnloadGuardianCutscene(&guardianIntro);
                    opening=preview;
                    openingPreview=true;
                    devOpeningWasPaused=devGuardianWasPaused=false;
                    screen=OPENING_SCREEN;
                    devTools.open=false;
                } else snprintf(devTools.status,sizeof(devTools.status),"Não foi possível carregar a abertura.");
            } else if (action==DEV_MENU) {
                UnloadOpeningCutscene(&opening);
                UnloadGuardianCutscene(&guardianIntro);
                openingPreview=false;
                devTools.open=false;
                screen=0;
            } else if (action==DEV_HEAL && inPractice && playerHealth>0) {
                playerHealth=encounter.progress.maxHealth;
                encounter.progress.health=playerHealth;
                ResetGun(&gun);
                ResetBible(&bible);
                dodgeRemaining=dodgeCooldown=katanaAttackRemaining=katanaAttackCooldown=0;
                screen=2;
                devTools.open=false;
            } else if (action==DEV_CLEAR_STAGE && inPractice) {
                if (encounter.phase==ENCOUNTER_FIGHT && !encounter.guardianIntroPending) {
                    ClearEnemyAttacks(&enemies);
                    encounter.presentation=(StagePresentation){0};
                    if (encounter.progress.stage==CLEARING_BOSS_STAGE &&
                        (encounter.progress.map<=1 || encounter.progress.map==BONELAND_MAP ||
                         encounter.progress.map==TEMPLAR_FIELD_MAP ||
                         encounter.progress.map==NECROMANCER_MAP) &&
                        enemies.list[0].type==ENEMY_BOSS) {
                        Enemy *boss=&enemies.list[0];
                        boss->guardianPhase=GUARDIAN_PHASE_TWO;
                        boss->health=0;
                        boss->state=ENEMY_FALLEN;
                        boss->stateTime=0;
                        if (clearing.necromancerArena) {
                            boss->necromancerForm=3;
                            boss->position=clearing.pedestal;
                        }
                        player=boss->position;
                        OpenEncounterSeal(&encounter,&enemies,player);
                    } else {
                        for (int i=0; i<ENEMY_COUNT; i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
                        UpdateEncounter(&encounter,&enemies,&clearing,player,&playerHealth);
                    }
                    screen=2;
                    devTools.open=false;
                } else snprintf(devTools.status,sizeof(devTools.status),"Conclua a recompensa ou o selamento antes de encerrar outra onda.");
            } else if (action==DEV_STAGE || action==DEV_BOSS_ONE || action==DEV_BOSS_TWO ||
                       action==DEV_GUARDIAN || action==DEV_SEAL || action==DEV_LIBERATION || action==DEV_PASSAGE) {
                if (StartDevEncounter(&devTools,action,&encounter,&enemies,&clearing,&player,&playerHealth)) {
                    UnloadOpeningCutscene(&opening);
                    UnloadGuardianCutscene(&guardianIntro);
                    openingPreview=false;
                    currentWeapon=encounter.progress.weapon;
                    playerMaxHealth=encounter.progress.maxHealth;
                    playerSpeed=encounter.progress.speed;
                    ResetGun(&gun);
                    ResetBible(&bible);
                    dodgeRemaining=dodgeCooldown=katanaAttackRemaining=katanaAttackCooldown=0;
                    katanaAttackId=0;
                    walkTime=0;
                    facing=0;
                    pedestalMessageTime=0;
                    entryFadeRemaining=0;
                    showCollisionGuide=devTools.collision;
                    screen=2;
                    devTools.open=false;
                } else snprintf(devTools.status,sizeof(devTools.status),"Não foi possível carregar o cenário de teste.");
            }
            BeginGameFrame();
                ClearBackground(BLACK);
                if (devTools.open) DrawDevTools(&devTools,inPractice);
            EndGameFrame();
            continue;
        }
        if (screen == GUARDIAN_SCREEN) {
            GuardianCutsceneInput input=ReadGuardianCutsceneInput();
            if (guardianIntro.mouseAdvance && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) input.advance=true;
            UpdateGuardianCutscene(&guardianIntro,GetFrameTime(),input);
            if (guardianIntro.finished) {
                bool skipped=guardianIntro.skipped;
                UnloadGuardianCutscene(&guardianIntro);
                encounter.progress.health=playerHealth;
                if (guardianStory==GUARDIAN_STORY_INTRO) CompleteGuardianIntro(&encounter,true);
                else if (guardianStory==GUARDIAN_STORY_LIBERATION) {
                    CompleteGuardianLiberation(&encounter);
                    if (skipped) FindFreeMapPosition(&clearing,clearing.pedestal,&player);
                } else if (guardianStory==VILLAGE_STORY_AFTERMATH) CompleteVillageCinematic(&encounter);
                else if (guardianStory==ANGEL_STORY) CompleteAngelCinematic(&encounter,playerHealth);
                else if (guardianStory==IGRIS_STORY_INTRO) CompleteIgrisIntro(&encounter,&enemies);
                else if (guardianStory==IGRIS_STORY_AFTERMATH)
                    CompleteIgrisAftermath(&encounter,&enemies,&clearing,&player,playerHealth);
                else if (guardianStory==GUARDIAN_STORY_PASSAGE)
                    CompleteGuardianPassage(&encounter,&enemies,&clearing,&player,playerHealth);
                screen=2;
                entryFadeRemaining=guardianStory==IGRIS_STORY_INTRO ? 0 : entryFadeDuration;
            }
            BeginGameFrame();
                ClearBackground(BLACK);
                if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndGameFrame();
            continue;
        }
        if (screen == 0) {
            MenuAction action = UpdateMenu(&menu);
            if (action == MENU_QUIT) break;
            if (action == MENU_START) {
                weaponSelect.selected = 0;
                screen = 1;
            } else if (action == MENU_CONTINUE) {
                RunProgress progress;
                if (LoadClearingCheckpoint(&progress)) {
                    SpeedrunStart(progress.runSeconds,progress.runEligible);
                    speedrunRecorded=progress.map==NECROMANCER_MAP && progress.stage==CLEARING_COMPLETE_STAGE;
                    if (ClearingMapId(&clearing) != progress.map) {
                        Clearing next = {0};
                        if (!InitCampaignMap(&next, progress.map)) break;
                        UnloadClearing(&clearing);
                        clearing = next;
                    }
                    BeginEncounter(&encounter,&enemies,&clearing,progress,clearing.spawn);
                    currentWeapon=progress.weapon;
                    playerMaxHealth=progress.maxHealth;
                    playerHealth=progress.health;
                    playerSpeed=progress.speed;
                    screen=2;
                }
            } else if (action == MENU_TEST_VILLA) {
                if (!clearing.village) {
                    UnloadClearing(&clearing);
                    clearing = (Clearing){0};
                    if (!InitVillage(&clearing)) break;
                }
                RunProgress progress = {
                    .map = 2,
                    .stage = CLEARING_BOSS_STAGE,
                    .weapon = WEAPON_KATANA,
                    .initialWeapon = WEAPON_KATANA,
                    .health = 140,
                    .maxHealth = 140,
                    .speed = 235.0f,
                    .damage = 28.0f,
                    .bossPower = 1.0f,
                    .guardianIntroSeen = true,
                    .weaponLevel = {2, 2, 2}
                };
                BeginEncounter(&encounter, &enemies, &clearing, progress, clearing.spawn);
                encounter.practice = true;
                SpeedrunStart(0,false);
                currentWeapon = progress.weapon;
                playerMaxHealth = progress.maxHealth;
                playerHealth = progress.health;
                playerSpeed = progress.speed;
                screen = 2;
            } else if (action == MENU_TEST_CHAPEL || action == MENU_TEST_INVERTED_CHAPEL) {
                int chapelMap=action==MENU_TEST_CHAPEL ? CHAPEL_MAP : INVERTED_CHAPEL_MAP;
                if (ClearingMapId(&clearing)!=chapelMap) {
                    UnloadClearing(&clearing);
                    clearing = (Clearing){0};
                    if (!InitCampaignMap(&clearing,chapelMap)) break;
                }
                RunProgress progress = {
                    .map = chapelMap,
                    .angelSceneCompleted = chapelMap==INVERTED_CHAPEL_MAP,
                    .angelBlessingUses = chapelMap==INVERTED_CHAPEL_MAP,
                    .stage = CLEARING_BOSS_STAGE,
                    .weapon = WEAPON_KATANA,
                    .initialWeapon = WEAPON_KATANA,
                    .health = 140,
                    .maxHealth = 140,
                    .speed = 235.0f,
                    .damage = 28.0f,
                    .bossPower = 1.0f,
                    .guardianIntroSeen = true,
                    .weaponLevel = {2, 2, 2}
                };
                BeginEncounter(&encounter, &enemies, &clearing, progress, clearing.spawn);
                encounter.practice = true;
                SpeedrunStart(0,false);
                currentWeapon = progress.weapon;
                playerMaxHealth = progress.maxHealth;
                playerHealth = progress.health;
                playerSpeed = progress.speed;
                screen = 2;
            }
            if (screen == 2) {
                player = clearing.spawn;
                dodgeRemaining = 0.0f;
                dodgeCooldown = 0.0f;
                facing = 0;
                walkTime = 0.0f;
                pedestalMessageTime = 0.0f;
                ResetBible(&bible);
                ResetGun(&gun);
                katanaAttackRemaining = 0.0f;
                katanaAttackCooldown = 0.0f;
                katanaAttackId = 0;
                katanaShockwaveCooldown = 0.0f;
                memset(katanaShockwaves, 0, sizeof(katanaShockwaves));
                entryFadeRemaining = entryFadeDuration;
            }
            BeginGameFrame();
                ClearBackground(BLACK);
                DrawMenu(&menu);
                bool hoverTools=CheckCollisionPointRec(GetMousePosition(),toolsButton);
                DrawRectangleRec(toolsButton,hoverTools?MAROON:Fade(BLACK,.65f));
                DrawRectangleLinesEx(toolsButton,1,GOLD);
                DrawGameTextIn("Ferramentas (F1)",toolsButton,20,RAYWHITE,GAME_TEXT_BODY);
            EndGameFrame();
            continue;
        }
        if (screen == 1 || screen == OPENING_SCREEN) {
            bool fromOpening = screen == OPENING_SCREEN;
            int choice;
            if (fromOpening) {
                UpdateOpeningCutscene(&opening, GetFrameTime());
                BeginGameFrame();
                    ClearBackground(BLACK);
                    if (!opening.finished) DrawOpeningCutscene(&opening);
                EndGameFrame();
                if (!opening.finished) continue;
                UnloadOpeningCutscene(&opening);
                if (openingPreview) {
                    openingPreview=false;
                    screen=previewReturnScreen;
                    devTools.open=true;
                    snprintf(devTools.status,sizeof(devTools.status),"Prévia da abertura concluída. Escolha outro teste ou feche o painel.");
                    continue;
                }
                choice = currentWeapon;
            } else {
                choice = UpdateWeaponSelect(&weaponSelect);
            }
            if (choice == -2) {
                choosingWeaponAfterDeath=false;
                screen = 0;
                continue;
            }
            if (choice >= 0) {
                choosingWeaponAfterDeath=false;
                if (!fromOpening) { SpeedrunStart(0,true); speedrunRecorded=false; }
                if (!openingSeen) {
                    openingSeen = true;
                    if (InitOpeningCutscene(&opening, menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                            menu.audio.volume[AUDIO_EFFECTS]/100.0f)) {
                        currentWeapon = choice;
                        screen = OPENING_SCREEN;
                        BeginGameFrame();
                            ClearBackground(BLACK);
                            DrawOpeningCutscene(&opening);
                        EndGameFrame();
                        continue;
                    }
                    TraceLog(LOG_WARNING, "Abertura indisponivel. Iniciando a partida.");
                }
                if (ClearingMapId(&clearing) != 0) {
                    Clearing next={0};
                    if (!InitClearing(&next)) break;
                    UnloadClearing(&clearing);
                    clearing=next;
                }
                currentWeapon = choice;
                RunProgress progress=NewRunProgress(currentWeapon);
                BeginEncounter(&encounter,&enemies,&clearing,progress,clearing.spawn);
                if (fromOpening) encounter.presentation.areaSubtitle = "A Terra dos que Não Descansam";
                playerMaxHealth=progress.maxHealth;
                playerSpeed=progress.speed;
                if (!SaveClearingCheckpoint(&progress))
                    TraceLog(LOG_WARNING, "Nao foi possivel salvar a arma escolhida");
                player = clearing.spawn;
                playerHealth = playerMaxHealth;
                dodgeRemaining = 0.0f;
                dodgeCooldown = 0.0f;
                facing = 0;
                walkTime = 0.0f;
                pedestalMessageTime = 0.0f;
                ResetBible(&bible);
                ResetGun(&gun);
                katanaAttackRemaining = 0.0f;
                katanaAttackCooldown = 0.0f;
                katanaAttackId = 0;
                katanaShockwaveCooldown = 0.0f;
                memset(katanaShockwaves, 0, sizeof(katanaShockwaves));
                entryFadeRemaining = entryFadeDuration;
                screen = 2;
            }
            if (fromOpening) continue;
            BeginGameFrame();
                ClearBackground(BLACK);
                DrawWeaponSelect(&weaponSelect);
            EndGameFrame();
            continue;
        }
        if (clearing.necromancerArena && encounter.phase==ENCOUNTER_CLEAR &&
            encounter.presentation.remaining<=0 &&
            !speedrunRecorded && !encounter.practice && SpeedrunEligible()) {
            speedrunRecorded=true;
            speedrunNamePending=true;
            completedRunTime=SpeedrunTime();
            speedrunName[0]=0;
            speedrunNameBytes=speedrunNameChars=0;
        }
        if (speedrunNamePending) {
            int codepoint;
            while ((codepoint=GetCharPressed())!=0) {
                if (codepoint<32 || codepoint>255 || (codepoint>=127 && codepoint<160) ||
                    speedrunNameChars>=24) continue;
                int utf8Size=0;
                const char *utf8=CodepointToUTF8(codepoint,&utf8Size);
                if (speedrunNameBytes+utf8Size>=SPEEDRUN_NAME_BYTES) continue;
                memcpy(speedrunName+speedrunNameBytes,utf8,(size_t)utf8Size);
                speedrunNameBytes+=utf8Size;
                speedrunName[speedrunNameBytes]=0;
                speedrunNameChars++;
            }
            if (IsKeyPressed(KEY_BACKSPACE) && speedrunNameBytes>0) {
                do {speedrunNameBytes--;} while (speedrunNameBytes>0 &&
                    ((unsigned char)speedrunName[speedrunNameBytes]&0xC0)==0x80);
                speedrunName[speedrunNameBytes]=0;
                speedrunNameChars--;
            }
            bool hasName=false;
            for (int i=0;i<speedrunNameBytes;i++)
                if (speedrunName[i]!=' ') {hasName=true;break;}
            if (hasName && IsKeyPressed(KEY_ENTER)) {
                if (SpeedrunRecord(&speedrunBoard,completedRunTime,speedrunName))
                    speedrunNamePending=false;
                else TraceLog(LOG_WARNING,"Nao foi possivel salvar o nome da run");
            }
            float sw=(float)GetScreenWidth(), sh=(float)GetScreenHeight();
            Rectangle panel={sw*.1f,sh*.2f,sw*.8f,sh*.6f};
            char timeText[40];
            SpeedrunFormat(timeText,sizeof(timeText),completedRunTime);
            BeginGameFrame();
                ClearBackground((Color){16,10,22,255});
                DrawRectangleRounded(panel,.08f,8,(Color){35,22,34,245});
                DrawRectangleRoundedLinesEx(panel,.08f,8,2,GOLD);
                DrawGameTextIn("Run concluída",(Rectangle){panel.x+12,panel.y+14,panel.width-24,panel.height*.19f},
                               48,GOLD,GAME_TEXT_TITLE);
                DrawGameTextIn(timeText,(Rectangle){panel.x+12,panel.y+panel.height*.24f,panel.width-24,panel.height*.15f},
                               38,RAYWHITE,GAME_TEXT_BODY);
                DrawGameTextIn("Nome de quem concluiu a run",(Rectangle){panel.x+12,panel.y+panel.height*.42f,panel.width-24,panel.height*.12f},
                               27,RAYWHITE,GAME_TEXT_BODY);
                Rectangle input={panel.x+panel.width*.1f,panel.y+panel.height*.56f,panel.width*.8f,panel.height*.16f};
                DrawRectangleRec(input,Fade(BLACK,.65f));
                DrawRectangleLinesEx(input,2,GOLD);
                DrawGameTextIn(speedrunNameBytes?speedrunName:"Digite seu nome",
                               (Rectangle){input.x+8,input.y+3,input.width-16,input.height-6},
                               30,speedrunNameBytes?RAYWHITE:LIGHTGRAY,GAME_TEXT_BODY);
                DrawGameTextIn("Enter: salvar  |  Backspace: apagar",
                               (Rectangle){panel.x+12,panel.y+panel.height*.78f,panel.width-24,panel.height*.12f},
                               22,LIGHTGRAY,GAME_TEXT_BODY);
            EndGameFrame();
            continue;
        }
        if (clearing.necromancerArena && encounter.phase==ENCOUNTER_CLEAR &&
            encounter.presentation.remaining<=0 && IsKeyPressed(KEY_ENTER)) {
            screen=0;
            continue;
        }
        if (playerHealth > 0) {
            deathHandled=false;
            revivalPending=false;
        }
        if (playerHealth <= 0) {
            if (!deathHandled) {
                deathHandled=true;
                devTools.invincible=false;
                enemies.practiceInvincible=false;
                revivalPending=OpenAngelRevival(&encounter,(unsigned)GetRandomValue(1,2147483647),&revivalQuestion);
                if (!encounter.practice &&
                    !SaveMenuCheckpoint(encounter.progress.initialWeapon))
                    TraceLog(LOG_WARNING, "Nao foi possivel encerrar o checkpoint da tentativa");
            }
            bool revivalFailed=false;
            if (revivalPending) {
                BeginGameFrame();
                ClearBackground((Color){16,10,22,255});
                int choice=DrawAngelRevival(&revivalQuestion);
                EndGameFrame();
                if (choice<0) continue;
                revivalPending=false;
                if (AnswerAngelRevival(&encounter,&revivalQuestion,choice,&playerHealth)) {
                    enemies.playerHitCooldown=fmaxf(enemies.playerHitCooldown,1.2f);
                    dodgeRemaining=0;
                    continue;
                }
                revivalFailed=true;
            }
            encounter.phase=ENCOUNTER_DEFEAT;
            bool restartImmediately=revivalFailed;
            float sw = (float)GetScreenWidth();
            float sh = (float)GetScreenHeight();
            float centerX = sw / 2.0f;
            float centerY = sh / 2.0f;
            float btnGap = 24.0f;
            float btnWidth = fminf(280.0f, (sw - 80.0f - btnGap) / 2.0f);
            float btnHeight = 64.0f;
            float btnY = centerY + 38.0f;
            Rectangle retry = {centerX - btnWidth - btnGap * 0.5f, btnY, btnWidth, btnHeight};
            Rectangle backToMenu = {centerX + btnGap * 0.5f, btnY, btnWidth, btnHeight};
            bool hoverRetry = CheckCollisionPointRec(GetMousePosition(), retry);
            bool hoverMenu = CheckCollisionPointRec(GetMousePosition(), backToMenu);
            if (!restartImmediately && (IsKeyPressed(KEY_ESCAPE) ||
                (hoverMenu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))) {
                screen = 0;
                continue;
            }
            if (restartImmediately || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R) ||
                (hoverRetry && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                devTools.map=CLEARING_MAP;
                devTools.wave=1;
                choosingWeaponAfterDeath=true;
                weaponSelect.selected=0;
                openingSeen=true;
                screen=1;
                // Finish the frame so the restart click cannot select a weapon.
                BeginGameFrame();
                ClearBackground(BLACK);
                DrawWeaponSelect(&weaponSelect);
                EndGameFrame();
                continue;
            }
            BeginGameFrame();
            ClearBackground((Color){16,10,22,255});
            DrawGameTextIn("Game Over", (Rectangle){0.0f, centerY - 190.0f, sw, 96.0f},
                           92.0f, (Color){220, 45, 55, 255}, GAME_TEXT_TITLE);
            DrawGameTextIn("Fim da tentativa", (Rectangle){0.0f, centerY - 82.0f, sw, 52.0f},
                           48.0f, (Color){255,119,103,255}, GAME_TEXT_TITLE);
            DrawGameTextIn("Sua tentativa terminou.", (Rectangle){0.0f, centerY - 18.0f, sw, 32.0f},
                           22.0f, RAYWHITE, GAME_TEXT_BODY);
            DrawRectangleRec(retry,hoverRetry?MAROON:DARKGRAY);
            DrawRectangleLinesEx(retry,2,GOLD);
            DrawGameTextIn("Reiniciar", (Rectangle){retry.x + 12.0f, retry.y + 8.0f, retry.width - 24.0f, retry.height - 16.0f},
                           32.0f, RAYWHITE, GAME_TEXT_TITLE);
            DrawRectangleRec(backToMenu,hoverMenu?MAROON:DARKGRAY);
            DrawRectangleLinesEx(backToMenu,2,GOLD);
            DrawGameTextIn("Voltar ao Menu", (Rectangle){backToMenu.x + 12.0f, backToMenu.y + 8.0f, backToMenu.width - 24.0f, backToMenu.height - 16.0f},
                           32.0f, RAYWHITE, GAME_TEXT_TITLE);
            DrawGameTextIn("Enter ou R: nova tentativa  |  Esc: voltar ao menu",
                           (Rectangle){0.0f, btnY + btnHeight + 24.0f, sw, 28.0f},
                           18.0f, LIGHTGRAY, GAME_TEXT_BODY);
            EndGameFrame();
            continue;
        }
        /* Ao trocar de janela (Alt+Tab, Cmd+Tab etc.), a partida pausa sozinha. */
        bool lostFocus = screen == 2 && !IsWindowFocused();
        if (screen == 3 || IsKeyPressed(KEY_ESCAPE) || lostFocus) {
            if (screen == 3) {
                PauseAction action = UpdatePauseMenu(&menu);
                if (action == PAUSE_RESUME) screen = 2;
                else if (action == PAUSE_MAIN_MENU) screen = 0;
            } else {
                screen = 3;
                menu.pause.page = PAUSE_OPTIONS;
                menu.pause.selected = 0;
                for (int i = 0; i < 4; i++) menu.pause.buttonFocus[i] = i == 0 ? 1.0f : 0.0f;
            }
            BeginGameFrame();
                ClearBackground(BLACK);
                DrawPauseMenu(&menu);
            EndGameFrame();
            continue;
        }
        if (entryFadeRemaining<=0 && !encounter.guardianIntroPending)
            UpdateStagePresentation(&encounter.presentation,fminf(GetFrameTime(),.05f));
        if (entryFadeRemaining<=0 && encounter.phase==ENCOUNTER_VILLAGE_LETTER_READY &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 cursor=GetScreenToWorld2D(GetMousePosition(),FitClearing(&clearing));
            if (CheckCollisionPointCircle(cursor,enemies.list[0].position,31))
                ReadVillageLetter(&encounter,&enemies,player);
        }
        bool encounterChanged=entryFadeRemaining<=0 &&
            UpdateEncounter(&encounter,&enemies,&clearing,player,&playerHealth);
        if (encounterChanged) {
            ResetGun(&gun);
            ResetBible(&bible);
            katanaAttackRemaining=0;
            katanaAttackCooldown=0;
            dodgeRemaining=0;
        }
        if (encounter.phase==ENCOUNTER_IGRIS_CINEMATIC || encounter.phase==ENCOUNTER_IGRIS_AFTERMATH) {
            bool aftermath=encounter.phase==ENCOUNTER_IGRIS_AFTERMATH;
            StopGameMusic(&menu.audio);
            guardianStory=aftermath ? IGRIS_STORY_AFTERMATH : IGRIS_STORY_INTRO;
            ResetGun(&gun);
            ResetBible(&bible);
            ClearEnemyAttacks(&enemies);
            katanaAttackRemaining=katanaAttackCooldown=dodgeRemaining=dodgeCooldown=0;
            katanaShockwaveCooldown=0;
            memset(katanaShockwaves,0,sizeof(katanaShockwaves));
            float music=menu.audio.volume[AUDIO_MUSIC]/100.0f;
            float effects=menu.audio.volume[AUDIO_EFFECTS]/100.0f;
            bool loaded=aftermath ? InitIgrisAftermath(&guardianIntro,music,effects)
                                  : InitIgrisCutscene(&guardianIntro,music,effects);
            if (loaded) screen=GUARDIAN_SCREEN;
            else if (aftermath) {
                CompleteIgrisAftermath(&encounter,&enemies,&clearing,&player,playerHealth);
                entryFadeRemaining=entryFadeDuration;
            }
            else CompleteIgrisIntro(&encounter,&enemies);
            BeginGameFrame();
            ClearBackground(BLACK);
            if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndGameFrame();
            continue;
        }
        if (encounter.phase==ENCOUNTER_ANGEL_CINEMATIC) {
            StopGameMusic(&menu.audio);
            guardianStory=ANGEL_STORY;
            if (InitAngelCutscene(&guardianIntro,menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                  menu.audio.volume[AUDIO_EFFECTS]/100.0f))
                screen=GUARDIAN_SCREEN;
            else CompleteAngelCinematic(&encounter,playerHealth);
            BeginGameFrame();
            ClearBackground(BLACK);
            if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndGameFrame();
            continue;
        }
        if (encounter.phase==ENCOUNTER_VILLAGE_CINEMATIC) {
            if (!menu.audio.playing[TRACK_MINIBOSS]) {
                guardianStory=VILLAGE_STORY_AFTERMATH;
                ResetGun(&gun);
                ResetBible(&bible);
                katanaAttackRemaining=katanaAttackCooldown=dodgeRemaining=dodgeCooldown=0;
                memset(katanaShockwaves,0,sizeof(katanaShockwaves));
                if (InitVillageCutscene(&guardianIntro,menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                         menu.audio.volume[AUDIO_EFFECTS]/100.0f))
                    screen=GUARDIAN_SCREEN;
                else CompleteVillageCinematic(&encounter);
            }
            BeginGameFrame();
            ClearBackground(BLACK);
            if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndGameFrame();
            continue;
        }
        if (GuardianLiberationPending(&encounter) || GuardianPassagePending(&encounter)) {
            bool passage=GuardianPassagePending(&encounter);
            guardianStory=passage ? GUARDIAN_STORY_PASSAGE : GUARDIAN_STORY_LIBERATION;
            ResetGun(&gun);
            ResetBible(&bible);
            ClearEnemyAttacks(&enemies);
            katanaAttackRemaining=katanaAttackCooldown=dodgeRemaining=dodgeCooldown=0;
            memset(katanaShockwaves,0,sizeof(katanaShockwaves));
            StopGameMusic(&menu.audio);
            if (InitGuardianAftermath(&guardianIntro,passage,menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                      menu.audio.volume[AUDIO_EFFECTS]/100.0f)) {
                screen=GUARDIAN_SCREEN;
                BeginGameFrame();
                    ClearBackground(BLACK);
                    DrawGuardianCutscene(&guardianIntro);
                EndGameFrame();
                continue;
            }
            TraceLog(LOG_WARNING,"Cutscene pos-Guardiao indisponivel. Aplicando o estado final.");
            if (passage) CompleteGuardianPassage(&encounter,&enemies,&clearing,&player,playerHealth);
            else {
                CompleteGuardianLiberation(&encounter);
                FindFreeMapPosition(&clearing,clearing.pedestal,&player);
            }
            entryFadeRemaining=entryFadeDuration;
            continue;
        }
        if (encounter.guardianIntroPending && (clearing.village ||
            clearing.chapel || clearing.basement || clearing.invertedChapel)) {
            encounter.guardianIntroPending = false;
        }
        if (encounter.guardianIntroPending) {
            guardianStory=GUARDIAN_STORY_INTRO;
            ResetGun(&gun);
            ResetBible(&bible);
            ClearEnemyAttacks(&enemies);
            katanaAttackRemaining=katanaAttackCooldown=dodgeRemaining=dodgeCooldown=0;
            if ((clearing.cemetery || clearing.boneland) && menu.audio.playing[TRACK_COMBAT]) {
                BeginGameFrame();
                ClearBackground(BLACK);
                EndGameFrame();
                continue;
            }
            float musicVolume=menu.audio.volume[AUDIO_MUSIC]/100.0f;
            float effectsVolume=menu.audio.volume[AUDIO_EFFECTS]/100.0f;
            bool loaded=clearing.cemetery
                ? InitCoveiroCutscene(&guardianIntro,musicVolume,effectsVolume)
                : clearing.boneland ? InitBonelandCutscene(&guardianIntro,musicVolume,effectsVolume)
                : InitGuardianCutscene(&guardianIntro,musicVolume,effectsVolume);
            if (loaded) {
                screen=GUARDIAN_SCREEN;
                BeginGameFrame();
                    ClearBackground(BLACK);
                    DrawGuardianCutscene(&guardianIntro);
                EndGameFrame();
                continue;
            }
            TraceLog(LOG_WARNING,"Cutscene do Guardiao indisponivel. Usando a apresentacao do chefe.");
            CompleteGuardianIntro(&encounter,false);
        }
        currentWeapon=encounter.progress.weapon;
        playerMaxHealth=encounter.progress.maxHealth;
        playerSpeed=encounter.progress.speed;
        float deltaTime = EncounterPaused(&encounter) || encounterChanged ? 0 : GetFrameTime();
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        bool entering = entryFadeRemaining > 0.0f || EncounterPaused(&encounter) || encounterChanged;
        if (entering) entryFadeRemaining = fmaxf(0.0f, entryFadeRemaining - fminf(GetFrameTime(),.05f));
        if (!entering && IsKeyPressed(KEY_F3)) showCollisionGuide = !showCollisionGuide;
        if (!entering && IsKeyPressed(KEY_F6)) showSpriteDebug = !showSpriteDebug;
        if (clearing.templarField)
            clearing.exitLocked = encounter.progress.stage != CLEARING_COMPLETE_STAGE;
        bool nearExit = (clearing.cemetery || clearing.village || clearing.boneland) &&
            CheckCollisionPointRec(player, clearing.exit);
        bool nearPedestal = encounter.progress.map == 0 &&
            CheckCollisionPointCircle(player, clearing.pedestal, CLEARING_ALTAR_RADIUS);
        bool altarOpen = ClearingAltarOpen(&encounter, &clearing);
        bool interactPressed = IsKeyPressed(KEY_E);
        encounter.progress.health=playerHealth;
        if (!entering && playerHealth>0 && BeginGuardianPassage(&encounter,&clearing,player,interactPressed))
            continue;
        bool toBoneland = clearing.invertedChapel && !EncounterPaused(&encounter) &&
            CheckCollisionPointRec(player, clearing.exit);
        bool toTemplarField = CanEnterTemplarField(&encounter, &clearing, player);
        bool toNecromancer = clearing.templarField && !clearing.exitLocked &&
                            CheckCollisionPointRec(player,(Rectangle){700,0,270,150});
        if (!entering && playerHealth > 0 &&
            (toNecromancer || toTemplarField || toBoneland || CanEnterVillage(&encounter, &clearing, player, interactPressed) ||
             CanEnterChapel(&encounter, &clearing, player, interactPressed) ||
             CanEnterBasement(&encounter, &clearing, player, interactPressed) ||
             CanEnterInvertedChapel(&encounter, &clearing, player))) {
            bool toInverted = clearing.basement;
            bool toBasement = clearing.chapel;
            bool toChapel = clearing.village;
            bool toVillage = clearing.cemetery;
            Clearing next = {0};
            int nextMap = toNecromancer ? NECROMANCER_MAP : toTemplarField ? TEMPLAR_FIELD_MAP : toBoneland ? BONELAND_MAP
                : toInverted ? 5 : toBasement ? 4 : toChapel ? 3 : toVillage ? 2 : 1;
            if (InitCampaignMap(&next, nextMap)) {
                UnloadClearing(&clearing);
                clearing = next;
                player = clearing.spawn;
                RunProgress nextProgress=encounter.progress;
                nextProgress.map = nextMap;
                nextProgress.stage = (toNecromancer || toInverted || toBasement || toChapel || toVillage) ? CLEARING_BOSS_STAGE : 1;
                if (toInverted || toBasement || toChapel || toVillage) nextProgress.guardianIntroSeen = true;
                if (toBoneland) nextProgress.guardianIntroSeen = false;
                nextProgress.bossPower=1;
                nextProgress.health=playerHealth;
                bool practice=encounter.practice;
                BeginEncounter(&encounter,&enemies,&clearing,nextProgress,player);
                encounter.practice=practice;
                if (!practice) SaveClearingCheckpoint(&nextProgress);
                ResetGun(&gun);
                ResetBible(&bible);
                dodgeRemaining = katanaAttackRemaining = 0;
                katanaShockwaveCooldown = 0.0f;
                memset(katanaShockwaves, 0, sizeof(katanaShockwaves));
                entryFadeRemaining = entryFadeDuration;
                if (toTemplarField && InitBonelandAftermath(&guardianIntro,
                        menu.audio.volume[AUDIO_MUSIC]/100.0f,
                        menu.audio.volume[AUDIO_EFFECTS]/100.0f)) {
                    guardianStory=BONELAND_STORY_AFTERMATH;
                    screen=GUARDIAN_SCREEN;
                }
                continue;
            }
        }
        if (!entering && IsKeyPressed(KEY_F4)) {
            playerHealth = playerHealth>5 ? playerHealth-5 : 0;
            if (playerHealth==0) continue;
        }
        if (!entering && IsKeyPressed(KEY_F5)) {
            playerHealth += 5;
            if (playerHealth > playerMaxHealth) playerHealth = playerMaxHealth;
        }
        int weaponLevel = (currentWeapon >= 0 && currentWeapon < 3)
            ? encounter.progress.weaponLevel[currentWeapon] : 0;
        float katanaAttackRange = (currentWeapon == WEAPON_KATANA && weaponLevel >= 2)
            ? KATANA_L2_RANGE : KATANA_BASE_RANGE;
        float katanaAttackRecovery = (currentWeapon == WEAPON_KATANA && weaponLevel >= 2)
            ? KATANA_L2_RECOVERY : KATANA_BASE_RECOVERY;
        float katanaArcDot = (currentWeapon == WEAPON_KATANA && weaponLevel >= 3)
            ? KATANA_L3_ARC_DOT : KATANA_BASE_ARC_DOT;

        if (dodgeCooldown > 0.0f) dodgeCooldown -= deltaTime;
        if (katanaAttackCooldown > 0.0f) katanaAttackCooldown -= deltaTime;
        if (katanaShockwaveCooldown > 0.0f) katanaShockwaveCooldown -= deltaTime;
        if (katanaAttackRemaining > 0.0f)
            katanaAttackRemaining = fmaxf(0.0f, katanaAttackRemaining - deltaTime);
        for (int s = 0; s < KATANA_MAX_SHOCKWAVES; s++) {
            if (katanaShockwaves[s].life <= 0.0f) continue;
            if (!entering) {
                katanaShockwaves[s].position = Vector2Add(
                    katanaShockwaves[s].position,
                    Vector2Scale(katanaShockwaves[s].direction, KATANA_SHOCKWAVE_SPEED * deltaTime));
                katanaShockwaves[s].life = fmaxf(0.0f, katanaShockwaves[s].life - deltaTime);
                if (IsClearingBlocked(&clearing, katanaShockwaves[s].position))
                    katanaShockwaves[s].life = 0.0f;
            }
        }
        bool grabbed = IgrisHoldingPlayer(&enemies);
        bool dodgePressed = !entering && !grabbed && (IsKeyPressed(KEY_SPACE) ||
                            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));
        bool bibleActivationStarted = false;
        if (currentWeapon == WEAPON_BIBLE) {
            float previousActivation = bible.activationRemaining;
            UpdateBible(&bible, entering ? 0.0f : deltaTime,
                        !entering && !grabbed && dodgeRemaining <= 0.0f && !dodgePressed &&
                        IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                        weaponLevel, !entering && EnemiesRemaining(&enemies) > 0, player);
            bibleActivationStarted = previousActivation <= 0.0f &&
                                     bible.activationRemaining > 0.0f;
        }

        Vector2 direction = {
            entering || grabbed ? 0.0f : (float) IsKeyDown(KEY_D) - (float) IsKeyDown(KEY_A),
            entering || grabbed ? 0.0f : (float) IsKeyDown(KEY_S) - (float) IsKeyDown(KEY_W)
        };
        Camera2D camera = FitClearing(&clearing);
        if (!entering && !grabbed && currentWeapon == WEAPON_KATANA &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            katanaAttackCooldown <= 0.0f && dodgeRemaining <= 0.0f) {
            Vector2 cursor = GetScreenToWorld2D(GetMousePosition(), camera);
            Vector2 aim = Vector2Subtract(cursor, (Vector2){player.x, player.y - 35.0f});
            katanaAttackDirection = Vector2LengthSqr(aim) > 0.0001f
                ? Vector2Normalize(aim) : FacingVector(facing);
            katanaAttackFacing = fabsf(katanaAttackDirection.x) >= fabsf(katanaAttackDirection.y)
                ? (katanaAttackDirection.x < 0.0f ? 2 : 3)
                : (katanaAttackDirection.y < 0.0f ? 1 : 0);
            katanaAttackId++;
            katanaDashReducedThisSwing = false;
            katanaAttackRemaining = katanaAttackDuration;
            katanaAttackCooldown = katanaAttackRecovery;
            if (weaponLevel >= 4 && katanaShockwaveCooldown <= 0.0f) {
                static const float fanAnglesDeg[3] = {-KATANA_L5_FAN_ANGLE_DEG, 0.0f, KATANA_L5_FAN_ANGLE_DEG};
                int waveCount = weaponLevel >= 5 ? 3 : 1;
                for (int w = 0; w < waveCount; w++) {
                    float rad = (waveCount == 3 ? fanAnglesDeg[w] : 0.0f) * DEG2RAD;
                    Vector2 waveDir = RotateVector2(katanaAttackDirection, rad);
                    for (int s = 0; s < KATANA_MAX_SHOCKWAVES; s++) {
                        if (katanaShockwaves[s].life <= 0.0f) {
                            katanaShockwaves[s] = (KatanaShockwave){
                                .position = Vector2Add((Vector2){player.x, player.y - 35.0f},
                                                       Vector2Scale(waveDir, 28.0f)),
                                .direction = waveDir,
                                .life = KATANA_SHOCKWAVE_LIFE
                            };
                            break;
                        }
                    }
                }
                katanaShockwaveCooldown = weaponLevel >= 5
                    ? KATANA_L5_SHOCKWAVE_COOLDOWN : KATANA_SHOCKWAVE_COOLDOWN;
            }
        }
        if (dodgePressed && dodgeRemaining <= 0.0f && dodgeCooldown <= 0.0f) {
            katanaAttackRemaining = 0.0f;
            if (Vector2LengthSqr(direction) > 0.0f) {
                dodgeDirection = Vector2Normalize(direction);
                dodgeFacing = fabsf(direction.x) >= fabsf(direction.y) &&
                    direction.x != 0.0f
                    ? (direction.x < 0.0f ? 2 : 3)
                    : (direction.y < 0.0f ? 1 : 0);
            } else {
                dodgeFacing = facing;
                dodgeDirection = FacingVector(facing);
            }
            facing = dodgeFacing;
            dodgeRemaining = dodgeDuration;
            dodgeCooldown = dodgeRecovery;
            walkTime = 0.0f;
        }

        walking = false;
        Vector2 previous = player;
        bool dodging = dodgeRemaining > 0.0f;
        int dodgeFrame = 0;
        if (dodging) {
            float activeTime = fminf(deltaTime, dodgeRemaining);
            MoveWithCollision(&clearing, &player,
                              Vector2Scale(dodgeDirection, dodgeSpeed * activeTime));
            dodgeRemaining -= activeTime;
            float progress = 1.0f - dodgeRemaining / dodgeDuration;
            int dodgeFrameCount = dodgeFacing < 2 ? 5 : 6;
            dodgeFrame = (int) (progress * (float) dodgeFrameCount);
            if (dodgeFrame >= dodgeFrameCount) dodgeFrame = dodgeFrameCount - 1;
            if (Vector2DistanceSqr(previous, player) < 0.01f) dodgeRemaining = 0.0f;
        } else if (Vector2LengthSqr(direction) > 0.0f) {
            direction = Vector2Normalize(direction);
            MoveWithCollision(&clearing, &player,
                              Vector2Scale(direction, playerSpeed * deltaTime));
        }
        Vector2 traveled = Vector2Subtract(player, previous);
        walking = !dodging && Vector2LengthSqr(traveled) > 0.01f;
        if (dodging) {
            facing = dodgeFacing;
        }
        if (walking) {
            int newFacing = fabsf(traveled.x) >= fabsf(traveled.y) &&
                fabsf(traveled.x) > 0.01f
                ? (traveled.x < 0.0f ? 2 : 3)
                : (traveled.y < 0.0f ? 1 : 0);
            if (newFacing != facing) walkTime = 0.0f;
            facing = newFacing;
            walkTime += Vector2Length(traveled) / playerSpeed;
        } else if (!dodging) {
            if (direction.x != 0.0f && fabsf(direction.x) >= fabsf(direction.y)) {
                facing = direction.x < 0.0f ? 2 : 3;
            } else if (direction.y != 0.0f) {
                facing = direction.y < 0.0f ? 1 : 0;
            }
            walkTime = 0.0f;
        }
        if (currentWeapon == WEAPON_GUN) {
            Vector2 aimWorld = GetScreenToWorld2D(GetMousePosition(), camera);
            Vector2 aimDelta = {aimWorld.x - player.x, aimWorld.y - (player.y - 35.0f)};
            if (!entering && !dodging && Vector2LengthSqr(aimDelta) > 4.0f)
                facing = fabsf(aimDelta.x) >= fabsf(aimDelta.y)
                    ? (aimDelta.x < 0.0f ? 2 : 3)
                    : (aimDelta.y < 0.0f ? 1 : 0);
            float previousPose = gun.poseRemaining;
            UpdateGun(&gun, &clearing, player, aimWorld,
                      entering ? 0.0f : deltaTime,
                      !entering && !grabbed && !dodging && IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                      weaponLevel);
            if (gun.poseRemaining > previousPose) gunAttackFacing = facing;
        }
        if (bibleActivationStarted) bibleAttackFacing = facing;
        if (!entering) {
            Vector2 playerCenter = {player.x, player.y-35.0f};
            if (currentWeapon == WEAPON_BIBLE) {
                for (int p = 0; p < ENEMY_PROJECTILE_COUNT; p++) {
                    if (enemies.projectiles[p].remaining > 0.0f &&
                        BibleBlocksCircle(&bible, player, enemies.projectiles[p].position, 6.0f)) {
                        enemies.projectiles[p].remaining = 0.0f;
                    }
                }
            }
            for (int i=0; i<ENEMY_COUNT; i++) {
                Enemy *enemy = &enemies.list[i];
                if (!EnemyCanBeHit(enemy)) continue;
                int healthBefore = enemy->health;
                Vector2 center = EnemyCenter(enemy);
                if (currentWeapon == WEAPON_GUN) {
                    int damage = GunHitEnemyCircle(&gun, i, center, EnemyHurtRadius(enemy));
                    if (damage) DamageEnemyFrom(enemy, (int)ceilf(damage*EncounterDamage(&encounter)), player);
                } else if (currentWeapon == WEAPON_BIBLE) {
                    if (enemy->bibleHitCooldown <= 0.0f &&
                        BibleTouchesCircle(&bible, player, center, EnemyHurtRadius(enemy)) &&
                        MapHasClearPath(&clearing,player,enemy->position)) {
                        DamageEnemyFrom(enemy, (int)ceilf(BibleContactDamage()*EncounterDamage(&encounter)), player);
                        if (weaponLevel >= 1)
                            KnockbackEnemy(enemy, &clearing, player, BIBLE_L1_KNOCKBACK);
                        enemy->bibleHitCooldown = BIBLE_HIT_COOLDOWN;
                    }
                    if (weaponLevel >= 5 && BibleHolyWaterHits(&bible, enemy->position, EnemyHurtRadius(enemy))) {
                        DamageEnemy(enemy, (int)ceilf(BIBLE_POOL_DAMAGE * EncounterDamage(&encounter)));
                    }
                } else if (currentWeapon == WEAPON_KATANA) {
                    if (katanaAttackRemaining > 0.0f &&
                        katanaAttackRemaining <= katanaAttackDuration*0.5f &&
                        enemy->lastKatanaSwing != katanaAttackId) {
                        Vector2 delta = Vector2Subtract(center, playerCenter);
                        float distance = Vector2Length(delta);
                        if (distance < katanaAttackRange+EnemyHurtRadius(enemy) && distance > 0.0f &&
                            Vector2DotProduct(delta, katanaAttackDirection) >= katanaArcDot*distance &&
                            MapHasClearPath(&clearing,player,enemy->position)) {
                            DamageEnemyFrom(enemy, (int)ceilf(WeaponBaseDamage(WEAPON_KATANA)*EncounterDamage(&encounter)), player);
                            enemy->lastKatanaSwing = katanaAttackId;
                            if (weaponLevel >= 1 && !katanaDashReducedThisSwing) {
                                dodgeCooldown = fmaxf(0.0f, dodgeCooldown - KATANA_L1_DODGE_REDUCTION);
                                katanaDashReducedThisSwing = true;
                            }
                            if (weaponLevel >= 3) {
                                KnockbackEnemy(enemy, &clearing, player, KATANA_L3_KNOCKBACK);
                            }
                        }
                    }
                    for (int s = 0; s < KATANA_MAX_SHOCKWAVES; s++) {
                        KatanaShockwave *wave = &katanaShockwaves[s];
                        if (wave->life <= 0.0f || wave->hitEnemies[i]) continue;
                        float limit = KATANA_SHOCKWAVE_RADIUS + EnemyHurtRadius(enemy);
                        if (Vector2DistanceSqr(wave->position, center) <= limit * limit) {
                            DamageEnemyFrom(enemy, (int)ceilf(KATANA_SHOCKWAVE_DAMAGE * EncounterDamage(&encounter)), Vector2Subtract(wave->position, Vector2Scale(wave->direction, 100)));
                            wave->hitEnemies[i] = true;
                        }
                    }
                }
                if (enemies.village && enemy->type == ENEMY_BOSS &&
                    healthBefore > 0 && enemy->health <= 0) {
                    enemies.villageBossDeathWeapon = currentWeapon;
                }
            }
            if (BeginIgrisAftermath(&encounter,&enemies,playerHealth)) continue;
            int remainingVillagers=enemies.villageSpawnRemaining;
            bool villageHorde=clearing.village && encounter.villageHordeStarted;
            bool initialEmergence=villageHorde && enemies.list[0].state==ENEMY_SPAWNING &&
                enemies.list[0].stateTime==0 && remainingVillagers==VILLAGE_HORDE_REINFORCEMENTS;
            enemies.practiceInvincible = encounter.practice && devTools.invincible;
            UpdateEnemies(&enemies, &clearing, player, deltaTime,
                          dodging || enemies.practiceInvincible,
                          &playerHealth, playerMaxHealth);
            if (playerHealth > 0) ApplyIgrisGrab(&enemies,&clearing,&player,deltaTime);
            if (villageHorde && (initialEmergence || enemies.villageSpawnRemaining<remainingVillagers))
                PlayVillageEmergence(&menu.audio,initialEmergence);
        }
        int frame = walking ? 1 + (int) (walkTime * 7.0f) % 4 : 0;
        if (!entering && nearPedestal && interactPressed) pedestalMessageTime = 3.0f;
        if (pedestalMessageTime > 0.0f) pedestalMessageTime -= deltaTime;

        BeginGameFrame();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                DrawClearing(&clearing);
                if (clearing.templarField && clearing.exitLocked) {
                    Vector2 left = {clearing.exit.x, clearing.exit.y + clearing.exit.height};
                    Vector2 right = {left.x + clearing.exit.width, left.y};
                    DrawLineEx(left, right, 7, Fade(MAROON, .85f));
                    DrawLineEx(left, right, 2, Fade(ORANGE, .8f));
                }
                if (altarOpen) DrawClearingAltarGlow(&clearing, (float)GetTime());
                if (clearing.chapel && encounter.chapelAltarDone) {
                    float w = (float)clearing.background.width, h = (float)clearing.background.height;
                    Vector2 door = {w * 0.225f, h * 0.27f};
                    float pulse = 0.78f + 0.22f * sinf((float)GetTime() * 4.0f);
                    DrawCircleV(door, 34.0f * pulse, Fade(GOLD, 0.22f));
                    DrawCircleLines((int)door.x, (int)door.y, 32.0f * pulse, Fade(GOLD, 0.80f));
                }
                if (clearing.basement) {
                    float w = (float)clearing.background.width, h = (float)clearing.background.height;
                    Vector2 sealCenter = {w * 0.50f, h * 0.56f};
                    float pulse = 0.75f + 0.25f * sinf((float)GetTime() * 3.5f);
                    static const float pPos[4][2] = {
                        {0.218f, 0.395f}, {0.782f, 0.395f}, {0.218f, 0.795f}, {0.782f, 0.795f}
                    };
                    static const float angles[4][2] = {
                        {180.0f, 270.0f}, {270.0f, 360.0f}, {90.0f, 180.0f}, {0.0f, 90.0f}
                    };
                    for (int i = 0; i < 4; i++) {
                        if (!encounter.basementSolved[i]) continue;
                        Vector2 p = {w * pPos[i][0], h * pPos[i][1]};
                        DrawCircleV((Vector2){p.x, p.y - 36.0f}, 18.0f * pulse, Fade(GOLD, 0.40f));
                        DrawLineEx(p, sealCenter, 5.0f, Fade(GOLD, 0.45f * pulse));
                        DrawCircleSector(sealCenter, 195.0f, angles[i][0], angles[i][1], 18, Fade(GOLD, 0.24f * pulse));
                        DrawCircleSectorLines(sealCenter, 195.0f, angles[i][0], angles[i][1], 18, Fade(GOLD, 0.80f));
                    }
                    if (encounter.basementComplete) {
                        Vector2 stairs = {w * 0.50f, h * 0.225f};
                        DrawCircleV(stairs, 36.0f * pulse, Fade((Color){175, 45, 75, 255}, 0.28f));
                        DrawCircleLines((int)stairs.x, (int)stairs.y, 34.0f * pulse, Fade(GOLD, 0.85f));
                    }
                }
                if (clearing.invertedChapel) {
                    float t = (float)GetTime();
                    float w = (float)clearing.background.width, h = (float)clearing.background.height;
                    for (int i = 0; i < 18; i++) {
                        float x = fmodf((float)(i * 97 + 43) + sinf(t * 0.6f + i) * 28.0f, w);
                        float y = fmodf((float)(i * 67 + 29) + t * (10.0f + (i % 5) * 3.0f), h);
                        DrawCircleV((Vector2){x, y}, 2.2f + (i % 2), Fade((Color){220, 45, 55, 255}, 0.45f));
                    }
                }
                if (currentWeapon == WEAPON_BIBLE) DrawBiblePools(&bible);
                DrawEnemies(&enemies, player, false);
                if (encounter.phase==ENCOUNTER_VILLAGE_LETTER_READY) {
                    Vector2 letter=enemies.list[0].position;
                    DrawCircleLinesV(letter,31,Fade(GOLD,.76f));
                    DrawRectangle((int)letter.x-10,(int)letter.y-6,20,12,(Color){222,193,139,255});
                    DrawRectangleLines((int)letter.x-10,(int)letter.y-6,20,12,BROWN);
                    DrawLine((int)letter.x-9,(int)letter.y-5,(int)letter.x,(int)letter.y+1,BROWN);
                    DrawLine((int)letter.x+9,(int)letter.y-5,(int)letter.x,(int)letter.y+1,BROWN);
                }
                if (currentWeapon == WEAPON_BIBLE) DrawBible(&bible, player, false);
                if (currentWeapon == WEAPON_BIBLE) DrawBibleActivation(&bible, player);
                if (dodging) {
                    DrawTexturePro(dodgeSprites,
                        (Rectangle) { dodgeFrame * 112.0f, facing * 80.0f, 112.0f, 80.0f },
                        (Rectangle) { player.x - 52.5f, player.y - 72.0f, 105.0f, 75.0f },
                        (Vector2) { 0, 0 }, 0.0f, WHITE);
                } else {
                    bool attacking = currentWeapon == WEAPON_KATANA && katanaAttackRemaining > 0.0f;
                    bool firing = currentWeapon == WEAPON_GUN && gun.poseRemaining > 0.0f;
                    bool invoking = currentWeapon == WEAPON_BIBLE && bible.activationRemaining > 0.0f;
                    int poseFrame = frame;
                    int bodyFacing = facing;
                    Texture2D bodyTexture = protagonist;
                    if (attacking) {
                        bodyTexture = katanaSprites.attack[(katanaAttackId - 1) % 3];
                        float progress = 1.0f - katanaAttackRemaining / katanaAttackDuration;
                        poseFrame = (int)(progress * 6.0f);
                        if (poseFrame > 5) poseFrame = 5;
                        bodyFacing = katanaAttackFacing;
                    } else if (currentWeapon == WEAPON_KATANA) {
                        bodyTexture = walking ? katanaSprites.walk : katanaSprites.idle;
                        poseFrame = walking ? (int)(walkTime * 10.0f) % 8 : 0;
                    } else if (firing) {
                        bodyTexture = gun.body;
                        poseFrame = (int)((1.0f - gun.poseRemaining / GUN_POSE_DURATION) * GUN_POSE_FRAMES);
                        if (poseFrame >= GUN_POSE_FRAMES) poseFrame = GUN_POSE_FRAMES - 1;
                        bodyFacing = gunAttackFacing;
                    } else if (currentWeapon == WEAPON_GUN) {
                        bodyTexture = walking ? gun.walkBody : gun.idleBody;
                        poseFrame = walking ? (int)(walkTime * 10.0f) % 8 : 0;
                    } else if (invoking) {
                        bodyTexture = bible.activation;
                        poseFrame = (int)((1.0f - bible.activationRemaining / BIBLE_ACTIVATION_DURATION) * BIBLE_ACTIVATION_FRAMES);
                        if (poseFrame >= BIBLE_ACTIVATION_FRAMES) poseFrame = BIBLE_ACTIVATION_FRAMES - 1;
                        bodyFacing = bibleAttackFacing;
                    } else if (currentWeapon == WEAPON_BIBLE) {
                        bodyTexture = walking ? bible.walk : bible.idle;
                        poseFrame = walking ? (int)(walkTime * 10.0f) % 8 :
                                              (int)(bible.animationTime * 6.0f) % 8;
                    }
                    Rectangle bodyDestination = {player.x - 30.0f, player.y - 72.0f, 60.0f, 75.0f};
                    Rectangle bodySource = {poseFrame * 64.0f, bodyFacing * 80.0f, 64.0f, 80.0f};
                    if (currentWeapon == WEAPON_KATANA) {
                        bodySource = (Rectangle){poseFrame * 160.0f, bodyFacing * 112.0f,
                                                 160.0f, 112.0f};
                        bodyDestination = (Rectangle){player.x - 80.0f,
                                                       player.y - 108.0f, 160.0f, 112.0f};
                    } else if (invoking) {
                        bodySource = (Rectangle){poseFrame * 128.0f, bodyFacing * 112.0f, 128, 112};
                        bodyDestination = (Rectangle){player.x - 64, player.y - 108, 128, 112};
                    } else if (currentWeapon == WEAPON_GUN) {
                        // Só a folha da pose parada ordena os perfis como direita, esquerda.
                        if (!firing && !walking && bodyFacing >= 2)
                            bodySource.y = (5 - bodyFacing) * bodySource.height;
                        // A arte da arma tem o corpo mais estreito que o sprite normal.
                        // Usa a mesma escala para espera, caminhada e disparo.
                        float width = bodyFacing < 2 ? 68.0f : 76.0f;
                        bodyDestination = (Rectangle){player.x - width * 0.5f,
                            player.y - 72.0f, width, 75.0f};
                    }
                    if (attacking && katanaAttackRemaining <= katanaAttackDuration * .5f) {
                        Vector2 center = {player.x, player.y - 35};
                        float angle = atan2f(katanaAttackDirection.y, katanaAttackDirection.x) * RAD2DEG;
                        float halfArc = acosf(katanaArcDot) * RAD2DEG;
                        float opacity = katanaAttackRemaining / (katanaAttackDuration * .5f);
                        DrawRing(center, katanaAttackRange - 7, katanaAttackRange,
                                 angle - halfArc, angle + halfArc, 32, Fade(MAROON, opacity * .55f));
                        DrawRing(center, katanaAttackRange - 2, katanaAttackRange,
                                 angle - halfArc, angle + halfArc, 32, Fade(RAYWHITE, opacity * .75f));
                    }
                    DrawTexturePro(bodyTexture,
                        bodySource,
                        bodyDestination,
                        (Vector2) { 0, 0 }, 0.0f, WHITE);
                }
                for (int s = 0; s < KATANA_MAX_SHOCKWAVES; s++) {
                    const KatanaShockwave *wave = &katanaShockwaves[s];
                    if (wave->life <= 0.0f) continue;
                    float angle = atan2f(wave->direction.y, wave->direction.x) * RAD2DEG;
                    float alpha = fminf(1.0f, wave->life / 0.25f);
                    DrawRing(wave->position, 18.0f, 34.0f, angle - 48.0f, angle + 48.0f, 20,
                             Fade(RED, 0.72f * alpha));
                    DrawRing(wave->position, 26.0f, 34.0f, angle - 42.0f, angle + 42.0f, 20,
                             Fade((Color){255, 130, 130, 255}, 0.85f * alpha));
                }
                if (currentWeapon == WEAPON_BIBLE) DrawBible(&bible, player, true);
                if (currentWeapon == WEAPON_GUN) DrawGun(&gun);
                DrawEnemies(&enemies, player, true);
                DrawEnemyProjectiles(&enemies);
                DrawClearingForeground(&clearing, player);
                if (showCollisionGuide) {
                    DrawClearingCollisionGuide(&clearing);
                }
                if (showSpriteDebug) {
                    DrawEnemyDebug(&enemies);
                    DrawCircleV(player, 3.0f, YELLOW);
                    DrawCircleLines((int)player.x, (int)player.y-35, 20.0f, SKYBLUE);
                    DrawGameText(TextFormat("P face:%d frame:%d scale:%.2f", facing, frame,
                                        camera.zoom*0.9375f),
                             (int)player.x-55, (int)player.y-100, 12, WHITE);
                }
            EndMode2D();

            DrawRectangle(12, 12, 490, 130, Fade(BLACK, 0.72f));
            DrawGameText("WASD: mover  |  Espaço/botão direito: esquiva", 24, 23, 17, RAYWHITE);
            DrawGameText("F1: ferramentas | F3: colisão | Esc: pausa", 24, 76, 17, RAYWHITE);
            DrawGameText("F4/F5: testar dano e cura", 24, 104, 17, RAYWHITE);
            if (altarOpen) {
                DrawGameText(nearPedestal ? "E: Interagir com o Altar Antigo"
                                      : "Investigue o altar da clareira",
                         24, 53, 18, GOLD);
            } else if (pedestalMessageTime > 0.0f) {
                DrawGameText("Conclua as ondas e sele o Guardião", 24, 53, 18, GOLD);
            } else if (nearPedestal) {
                DrawGameText("O altar está selado", 24, 53, 18, GOLD);
            } else {
                DrawGameText(nearExit
                    ? (clearing.boneland ? "Passagem para o " TEMPLAR_FIELD_TITLE
                        : clearing.village ? "Entrar na Capela do Anjo" : "Saída para Chã da Tristeza")
                    : (clearing.templarField ? (clearing.exitLocked ? "Derrote Igris para liberar a escadaria"
                        : "Suba ao Topo do Palácio") : clearing.necromancerArena ? "Topo do Palácio" : clearing.boneland ? "Terra dos Ossos"
                        : clearing.invertedChapel ? "Capela Invertida - saída ao sul"
                        : (clearing.basement ? "Porão da Capela (E nas placas da parede e nos 4 pilares)"
                        : (clearing.chapel ? (encounter.chapelAltarDone
                            ? "Capela do Anjo - porta superior esquerda aberta para o Porão"
                            : "Capela do Anjo (E perto do altar / estátuas / trono)")
                        : (clearing.village ? "Chã da Tristeza"
                        : (clearing.cemetery ? "Cemitério das Raízes" : "Explore a clareira"))))),
                    24, 53, 18, RAYWHITE);
            }
            DrawHud(&hud, playerHealth, playerMaxHealth,
                    dodgeCooldown, dodgeRecovery, SpeedrunTime());
            DrawGameText(TextFormat("Arma: %s", WeaponName((Weapon)currentWeapon)),
                     24, 150, 19, GOLD);
            if (currentWeapon == WEAPON_KATANA)
                DrawGameText("Clique esquerdo: corte de katana", 24, 176, 18, RAYWHITE);
            if (currentWeapon == WEAPON_BIBLE)
                DrawGameText(bible.boostCooldown <= 0.0f
                             ? "Clique esquerdo: acelerar órbitas"
                             : TextFormat("Aceleração: %.1f s", bible.boostCooldown),
                         24, 176, 18, RAYWHITE);
            if (currentWeapon == WEAPON_GUN)
                DrawGameText("Clique esquerdo: disparar (um tiro por clique)", 24, 176, 18, RAYWHITE);
            DrawEncounter(&encounter,&enemies);
            if (encounter.practice) {
                DrawRectangle(0,GetScreenHeight()-PRACTICE_FOOTER_HEIGHT,GetScreenWidth(),PRACTICE_FOOTER_HEIGHT,Fade(BLACK,.8f));
                DrawGameTextIn(devTools.invincible ? "PRÁTICA · Invulnerável · F1 ferramentas" :
                    "PRÁTICA · F1 ferramentas",(Rectangle){8,GetScreenHeight()-PRACTICE_FOOTER_HEIGHT,GetScreenWidth()-16,PRACTICE_FOOTER_HEIGHT},
                    20,GOLD,GAME_TEXT_BODY);
            }
            if (entering) {
                float t = entryFadeRemaining / entryFadeDuration;
                float opacity = t*t*(3.0f - 2.0f*t);
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Fade(BLACK, opacity));
            }
        EndGameFrame();
    }

    UnloadOpeningCutscene(&opening);
    UnloadGuardianCutscene(&guardianIntro);
    UnloadEnemies(&enemies);
    UnloadGun(&gun);
    UnloadBible(&bible);
    UnloadWeaponSelect(&weaponSelect);
    UnloadMenu(&menu);
    UnloadHud(&hud);
    UnloadKatanaSprites(&katanaSprites);
    UnloadTexture(dodgeSprites);
    UnloadTexture(protagonist);
    UnloadClearing(&clearing);
    UnloadVideoFrame();
    UnloadEncounterRewardUi();
    UnloadGameFonts();
    CloseWindow();
    return 0;
}
