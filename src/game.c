#include "game_text.h"
#include "raylib.h"
#include "raymath.h"
#include "clearing.h"
#include "hud.h"
#include "menu.h"
#include "weapon_select.h"
#include "bible.h"
#include "gun.h"
#include "enemy.h"
#include "encounter.h"
#include "cutscene.h"
#include "guardian_cutscene.h"
#include "dev_tools.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
#define KATANA_BASE_ARC_DOT 0.0872f
#define KATANA_L3_ARC_DOT -0.12f
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
    const int monitor = GetCurrentMonitor();
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    ToggleFullscreen();
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

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
    float playerSpeed = 240.0f;
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
    enum { GUARDIAN_STORY_INTRO, GUARDIAN_STORY_LIBERATION, GUARDIAN_STORY_PASSAGE, VILLAGE_STORY_AFTERMATH, ANGEL_STORY } guardianStory=GUARDIAN_STORY_INTRO;
    DevTools devTools={.wave=1,.selected=6};
    bool devOpeningWasPaused=false,devGuardianWasPaused=false;
    bool openingPreview=false;
    int previewReturnScreen=0;
    DevAction practiceScenario=DEV_STAGE;
    bool openingSeen = false;
    int currentWeapon = -1;
    Encounter encounter = {.progress = {.weapon=0}};
    const float entryFadeDuration = 1.15f;
    float entryFadeRemaining = 0.0f;

    while (!WindowShouldClose()) {
        bool devToggle=IsKeyPressed(KEY_F1);
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
                 encounter.progress.stage <= CLEARING_BOSS_STAGE) {
            track = (encounter.guardianIntroPending || encounter.phase==ENCOUNTER_VILLAGE_CINEMATIC ||
                encounter.phase==ENCOUNTER_LETTER) ? TRACK_SILENT :
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
            } else if (action==DEV_HEAL && inPractice) {
                playerHealth=encounter.progress.maxHealth;
                encounter.progress.health=playerHealth;
                if (encounter.phase==ENCOUNTER_DEFEAT) encounter.phase=ENCOUNTER_FIGHT;
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
                    practiceScenario=action;
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
            BeginDrawing();
                ClearBackground(BLACK);
                if (devTools.open) DrawDevTools(&devTools,inPractice);
            EndDrawing();
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
                else CompleteGuardianPassage(&encounter,&enemies,&clearing,&player,playerHealth);
                screen=2;
                entryFadeRemaining=entryFadeDuration;
            }
            BeginDrawing();
                ClearBackground(BLACK);
                if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndDrawing();
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
                currentWeapon = progress.weapon;
                playerMaxHealth = progress.maxHealth;
                playerHealth = progress.health;
                playerSpeed = progress.speed;
                screen = 2;
            } else if (action == MENU_TEST_CHAPEL) {
                if (!clearing.chapel) {
                    UnloadClearing(&clearing);
                    clearing = (Clearing){0};
                    if (!InitChapel(&clearing)) break;
                }
                RunProgress progress = {
                    .map = 3,
                    .stage = CLEARING_BOSS_STAGE,
                    .weapon = WEAPON_KATANA,
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
            BeginDrawing();
                ClearBackground(BLACK);
                DrawMenu(&menu);
            EndDrawing();
            continue;
        }
        if (screen == 1 || screen == OPENING_SCREEN) {
            bool fromOpening = screen == OPENING_SCREEN;
            int choice;
            if (fromOpening) {
                UpdateOpeningCutscene(&opening, GetFrameTime());
                BeginDrawing();
                    ClearBackground(BLACK);
                    if (!opening.finished) DrawOpeningCutscene(&opening);
                EndDrawing();
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
            if (choice == -2) { screen = 0; continue; }
            if (choice >= 0) {
                if (!openingSeen) {
                    openingSeen = true;
                    if (InitOpeningCutscene(&opening, menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                            menu.audio.volume[AUDIO_EFFECTS]/100.0f)) {
                        currentWeapon = choice;
                        screen = OPENING_SCREEN;
                        BeginDrawing();
                            ClearBackground(BLACK);
                            DrawOpeningCutscene(&opening);
                        EndDrawing();
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
            BeginDrawing();
                ClearBackground(BLACK);
                DrawWeaponSelect(&weaponSelect);
            EndDrawing();
            continue;
        }
        if (clearing.necromancerArena && encounter.phase==ENCOUNTER_CLEAR &&
            encounter.presentation.remaining<=0 && IsKeyPressed(KEY_ENTER)) {
            screen=0;
            continue;
        }
        if (playerHealth <= 0) {
            Rectangle retry = {GetScreenWidth()/2.0f-150,GetScreenHeight()/2.0f+25,300,65};
            bool hover=CheckCollisionPointRec(GetMousePosition(),retry);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R) ||
                (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                bool practice=encounter.practice;
                if (!practice && encounter.progress.map > 0) {
                    UnloadClearing(&clearing);
                    clearing=(Clearing){0};
                    if (!InitClearing(&clearing)) break;
                }
                RunProgress progress=practice ? encounter.progress : NewRunProgress(currentWeapon);
                progress.health=progress.maxHealth;
                if (practice) progress.guardianIntroSeen=true;
                int retryForm=practice && clearing.necromancerArena ? enemies.list[0].necromancerForm : 0;
                BeginEncounter(&encounter,&enemies,&clearing,progress,clearing.spawn);
                encounter.practice=practice;
                if (practice) {
                    if (!clearing.boneland) encounter.presentation=(StagePresentation){0};
                    if (retryForm>1) {
                        Enemy *boss=&enemies.list[0];
                        boss->necromancerForm=retryForm;
                        boss->health=boss->maxHealth*(retryForm==2?70:35)/100;
                        boss->state=ENEMY_IDLE;
                        boss->position=clearing.pedestal;
                    } else if (!clearing.necromancerArena && progress.stage==CLEARING_BOSS_STAGE &&
                               practiceScenario==DEV_BOSS_TWO) {
                        enemies.list[0].guardianPhase=GUARDIAN_PHASE_TWO;
                        enemies.list[0].health=enemies.list[0].maxHealth/2;
                        enemies.list[0].state=ENEMY_IDLE;
                        enemies.list[0].stateTime=0;
                    }
                } else SaveClearingCheckpoint(&progress);
                player=clearing.spawn;
                playerHealth=playerMaxHealth=progress.maxHealth;
                playerSpeed=progress.speed;
                ResetGun(&gun); ResetBible(&bible);
                dodgeRemaining=dodgeCooldown=0;
                katanaAttackRemaining=katanaAttackCooldown=0;
                katanaAttackId=0; walkTime=0; facing=0;
                katanaShockwaveCooldown = 0.0f;
                memset(katanaShockwaves, 0, sizeof(katanaShockwaves));
                pedestalMessageTime=0;
                entryFadeRemaining=entryFadeDuration;
                continue;
            }
            BeginDrawing();
            ClearBackground((Color){16,10,22,255});
            DrawGameTextIn("Fim da tentativa", (Rectangle){0,GetScreenHeight()/2-100,GetScreenWidth(),62},
                           59, (Color){255,119,103,255}, true);
            DrawGameText("Sua tentativa terminou.",(GetScreenWidth()-MeasureGameText("Sua tentativa terminou.",22))/2,
                GetScreenHeight()/2-30,22,RAYWHITE);
            DrawRectangleRec(retry,hover?MAROON:DARKGRAY);
            DrawRectangleLinesEx(retry,2,GOLD);
            DrawGameTextIn("Repetir", retry, 34, RAYWHITE, true);
            DrawGameText("Enter ou R: nova tentativa",(GetScreenWidth()-MeasureGameText("Enter ou R: nova tentativa",18))/2,
                (int)retry.y+85,18,LIGHTGRAY);
            EndDrawing();
            continue;
        }
        if (screen == 3 || IsKeyPressed(KEY_ESCAPE)) {
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
            BeginDrawing();
                ClearBackground(BLACK);
                DrawPauseMenu(&menu);
            EndDrawing();
            continue;
        }
        if (entryFadeRemaining<=0 && !encounter.guardianIntroPending)
            UpdateStagePresentation(&encounter.presentation,fminf(GetFrameTime(),.05f));
        bool encounterChanged=entryFadeRemaining<=0 &&
            UpdateEncounter(&encounter,&enemies,&clearing,player,&playerHealth);
        if (encounterChanged) {
            ResetGun(&gun);
            ResetBible(&bible);
            katanaAttackRemaining=0;
            katanaAttackCooldown=0;
            dodgeRemaining=0;
        }
        if (encounter.phase==ENCOUNTER_ANGEL_CINEMATIC) {
            StopGameMusic(&menu.audio);
            guardianStory=ANGEL_STORY;
            if (InitAngelCutscene(&guardianIntro,menu.audio.volume[AUDIO_MUSIC]/100.0f,
                                  menu.audio.volume[AUDIO_EFFECTS]/100.0f))
                screen=GUARDIAN_SCREEN;
            else CompleteAngelCinematic(&encounter,playerHealth);
            BeginDrawing();
            ClearBackground(BLACK);
            if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndDrawing();
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
            BeginDrawing();
            ClearBackground(BLACK);
            if (screen==GUARDIAN_SCREEN) DrawGuardianCutscene(&guardianIntro);
            EndDrawing();
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
                BeginDrawing();
                    ClearBackground(BLACK);
                    DrawGuardianCutscene(&guardianIntro);
                EndDrawing();
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
            if (clearing.cemetery && menu.audio.playing[TRACK_COMBAT]) {
                BeginDrawing();
                ClearBackground(BLACK);
                EndDrawing();
                continue;
            }
            float musicVolume=menu.audio.volume[AUDIO_MUSIC]/100.0f;
            float effectsVolume=menu.audio.volume[AUDIO_EFFECTS]/100.0f;
            bool loaded=clearing.cemetery
                ? InitCoveiroCutscene(&guardianIntro,musicVolume,effectsVolume)
                : InitGuardianCutscene(&guardianIntro,musicVolume,effectsVolume);
            if (loaded) {
                screen=GUARDIAN_SCREEN;
                BeginDrawing();
                    ClearBackground(BLACK);
                    DrawGuardianCutscene(&guardianIntro);
                EndDrawing();
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
                continue;
            }
        }
        if (!entering && IsKeyPressed(KEY_F4)) {
            playerHealth -= 5;
            if (playerHealth < 0) playerHealth = 0;
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
        bool bibleActivationStarted = false;
        if (currentWeapon == WEAPON_BIBLE) {
            float previousActivation = bible.activationRemaining;
            UpdateBible(&bible, entering ? 0.0f : deltaTime,
                        !entering && dodgeRemaining <= 0.0f && !IsKeyPressed(KEY_SPACE) &&
                        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
                        weaponLevel, !entering && EnemiesRemaining(&enemies) > 0, player);
            bibleActivationStarted = previousActivation <= 0.0f &&
                                     bible.activationRemaining > 0.0f;
        }

        Vector2 direction = {
            entering ? 0.0f : (float) IsKeyDown(KEY_D) - (float) IsKeyDown(KEY_A),
            entering ? 0.0f : (float) IsKeyDown(KEY_S) - (float) IsKeyDown(KEY_W)
        };
        bool dodgePressed = !entering && (IsKeyPressed(KEY_SPACE) ||
                            (currentWeapon != WEAPON_BIBLE && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)));
        Camera2D camera = FitClearing(&clearing);
        if (!entering && currentWeapon == WEAPON_KATANA &&
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
                      !entering && !dodging && IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
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
            int remainingVillagers=enemies.villageSpawnRemaining;
            bool villageHorde=clearing.village && encounter.villageHordeStarted;
            bool initialEmergence=villageHorde && enemies.list[0].state==ENEMY_SPAWNING &&
                enemies.list[0].stateTime==0 && remainingVillagers==12;
            enemies.practiceInvincible = encounter.practice && devTools.invincible;
            UpdateEnemies(&enemies, &clearing, player, deltaTime,
                          dodging || enemies.practiceInvincible,
                          &playerHealth, playerMaxHealth);
            if (villageHorde && (initialEmergence || enemies.villageSpawnRemaining<remainingVillagers))
                PlayVillageEmergence(&menu.audio,initialEmergence);
        }
        int frame = walking ? 1 + (int) (walkTime * 7.0f) % 4 : 0;
        if (!entering && nearPedestal && interactPressed) pedestalMessageTime = 3.0f;
        if (pedestalMessageTime > 0.0f) pedestalMessageTime -= deltaTime;

        BeginDrawing();
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

            int hudOffset = EncounterBossVisible(&encounter, &enemies) ? ENCOUNTER_BOSS_HUD_HEIGHT : 0;
            DrawRectangle(12, 12 + hudOffset, 490, 130, Fade(BLACK, 0.72f));
            DrawGameText(currentWeapon == WEAPON_BIBLE ? "WASD: mover  |  Espaço: esquiva" :
                     "WASD: mover  |  Espaço/botão direito: esquiva", 24, 23 + hudOffset, 17, RAYWHITE);
            DrawGameText("F1: ferramentas | F3: colisão | Esc: pausa", 24, 76 + hudOffset, 17, RAYWHITE);
            DrawGameText("F4/F5: testar dano e cura", 24, 104 + hudOffset, 17, RAYWHITE);
            if (altarOpen) {
                DrawGameText(nearPedestal ? "E: Interagir com o Altar Antigo"
                                      : "Investigue o altar da clareira",
                         24, 53 + hudOffset, 18, GOLD);
            } else if (pedestalMessageTime > 0.0f) {
                DrawGameText("Conclua as ondas e sele o Guardião", 24, 53 + hudOffset, 18, GOLD);
            } else if (nearPedestal) {
                DrawGameText("O altar está selado", 24, 53 + hudOffset, 18, GOLD);
            } else {
                DrawGameText(nearExit
                    ? (clearing.boneland ? "Passagem para o Campo dos Templários"
                        : clearing.village ? "Entrar na Capela do Anjo" : "Saída para Chã da Tristeza")
                    : (clearing.templarField ? (clearing.exitLocked ? "Derrote o Marechal para liberar a escadaria"
                        : "Suba ao Topo do Palácio") : clearing.necromancerArena ? "Topo do Palácio" : clearing.boneland ? "Terra dos Ossos"
                        : clearing.invertedChapel ? "Capela Invertida - saída ao sul"
                        : (clearing.basement ? "Porão da Capela (E nas placas da parede e nos 4 pilares)"
                        : (clearing.chapel ? (encounter.chapelAltarDone
                            ? "Capela do Anjo - porta superior esquerda aberta para o Porão"
                            : "Capela do Anjo (E perto do altar / estátuas / trono)")
                        : (clearing.village ? "Chã da Tristeza"
                        : (clearing.cemetery ? "Cemitério das Raízes" : "Explore a clareira"))))),
                    24, 53 + hudOffset, 18, RAYWHITE);
            }
            DrawHud(&hud, playerHealth, playerMaxHealth,
                    dodgeCooldown, dodgeRecovery, hudOffset);
            DrawGameText(TextFormat("Arma: %s", WeaponName((Weapon)currentWeapon)),
                     24, 150 + hudOffset, 19, GOLD);
            if (currentWeapon == WEAPON_KATANA)
                DrawGameText("Clique esquerdo: corte de katana", 24, 176 + hudOffset, 18, RAYWHITE);
            if (currentWeapon == WEAPON_BIBLE)
                DrawGameText(bible.boostCooldown <= 0.0f
                             ? "Botão direito: acelerar órbitas"
                             : TextFormat("Aceleração: %.1f s", bible.boostCooldown),
                         24, 176 + hudOffset, 18, RAYWHITE);
            if (currentWeapon == WEAPON_GUN)
                DrawGameText("Clique esquerdo: disparar (um tiro por clique)", 24, 176 + hudOffset, 18, RAYWHITE);
            DrawEncounter(&encounter,&enemies);
            if (encounter.practice) {
                DrawRectangle(0,GetScreenHeight()-28,GetScreenWidth(),28,Fade(BLACK,.8f));
                DrawGameTextIn(devTools.invincible ? "PRÁTICA · Invulnerável · F1 ferramentas" :
                    "PRÁTICA · F1 ferramentas",(Rectangle){8,GetScreenHeight()-28,GetScreenWidth()-16,28},
                    20,GOLD,false);
            }
            if (entering) {
                float t = entryFadeRemaining / entryFadeDuration;
                float opacity = t*t*(3.0f - 2.0f*t);
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Fade(BLACK, opacity));
            }
        EndDrawing();
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
    UnloadGameFonts();
    CloseWindow();
    return 0;
}
