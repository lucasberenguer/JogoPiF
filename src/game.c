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
#include <math.h>

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
    const float katanaAttackRecovery = 0.45f;
    const float katanaAttackRange = 105.0f;
    float katanaAttackRemaining = 0.0f;
    float katanaAttackCooldown = 0.0f;
    int katanaAttackFacing = 0;
    Vector2 katanaAttackDirection = {0.0f, 1.0f};
    int gunAttackFacing = 0;
    int bibleAttackFacing = 0;
    int katanaAttackId = 0;
    enum { OPENING_SCREEN = 4 };
    int screen = 0; /* 0: menu, 1: escolha da arma, 2: partida, 3: pausa */
    OpeningCutscene opening = {0};
    bool openingSeen = false;
    int currentWeapon = -1;
    Encounter encounter = {.progress = {.weapon=0}};
    const float entryFadeDuration = 1.15f;
    float entryFadeRemaining = 0.0f;

    while (!WindowShouldClose()) {
        GameTrack track = TRACK_SILENT;
        float musicStrength = 1.0f;
        if (screen == 0) track = TRACK_MENU;
        else if ((screen == 2 || screen == 3) && playerHealth > 0 &&
                 encounter.progress.stage <= CLEARING_BOSS_STAGE) {
            track = encounter.progress.stage == CLEARING_BOSS_STAGE ? TRACK_MINIBOSS : TRACK_COMBAT;
            if (screen == 3 || EncounterPaused(&encounter)) musicStrength = 0.42f;
        }
        UpdateGameMusic(&menu.audio, track, musicStrength, GetFrameTime());
        if (screen == 0) {
            MenuAction action = UpdateMenu(&menu);
            if (action == MENU_QUIT) break;
            if (action == MENU_START) {
                weaponSelect.selected = 0;
                screen = 1;
            } else if (action == MENU_CONTINUE) {
                RunProgress progress;
                if (LoadClearingCheckpoint(&progress)) {
                    if (clearing.cemetery!=(progress.map==1)) {
                        UnloadClearing(&clearing);
                        clearing=(Clearing){0};
                        if (!(progress.map?InitCemetery(&clearing):InitClearing(&clearing))) break;
                    }
                    BeginEncounter(&encounter,&enemies,&clearing,progress,clearing.spawn);
                    currentWeapon=progress.weapon;
                    playerMaxHealth=progress.maxHealth;
                    playerHealth=progress.health;
                    playerSpeed=progress.speed;
                    screen=2;
                }
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
                if (clearing.cemetery) {
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
        if (playerHealth <= 0) {
            Rectangle retry = {GetScreenWidth()/2.0f-150,GetScreenHeight()/2.0f+25,300,65};
            bool hover=CheckCollisionPointRec(GetMousePosition(),retry);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R) ||
                (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                if (clearing.cemetery) {
                    UnloadClearing(&clearing);
                    clearing=(Clearing){0};
                    if (!InitClearing(&clearing)) break;
                }
                RunProgress progress=NewRunProgress(currentWeapon);
                BeginEncounter(&encounter,&enemies,&clearing,progress,clearing.spawn);
                SaveClearingCheckpoint(&progress);
                player=clearing.spawn;
                playerHealth=playerMaxHealth=progress.maxHealth;
                playerSpeed=progress.speed;
                ResetGun(&gun); ResetBible(&bible);
                dodgeRemaining=dodgeCooldown=0;
                katanaAttackRemaining=katanaAttackCooldown=0;
                katanaAttackId=0; walkTime=0; facing=0;
                pedestalMessageTime=0;
                entryFadeRemaining=entryFadeDuration;
                continue;
            }
            BeginDrawing();
            ClearBackground((Color){16,10,22,255});
            DrawText("GAME OVER",(GetScreenWidth()-MeasureText("GAME OVER",52))/2,
                GetScreenHeight()/2-100,52,RED);
            DrawText("Sua tentativa terminou.",(GetScreenWidth()-MeasureText("Sua tentativa terminou.",22))/2,
                GetScreenHeight()/2-30,22,RAYWHITE);
            DrawRectangleRec(retry,hover?MAROON:DARKGRAY);
            DrawRectangleLinesEx(retry,2,GOLD);
            DrawText("REPETIR",(int)retry.x+95,(int)retry.y+19,26,RAYWHITE);
            DrawText("Enter ou R: nova tentativa",(GetScreenWidth()-MeasureText("Enter ou R: nova tentativa",18))/2,
                (int)retry.y+85,18,GRAY);
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
        if (entryFadeRemaining<=0)
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
        currentWeapon=encounter.progress.weapon;
        playerMaxHealth=encounter.progress.maxHealth;
        playerSpeed=encounter.progress.speed;
        float deltaTime = EncounterPaused(&encounter) || encounterChanged ? 0 : GetFrameTime();
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        bool entering = entryFadeRemaining > 0.0f || EncounterPaused(&encounter) || encounterChanged;
        if (entering) entryFadeRemaining = fmaxf(0.0f, entryFadeRemaining - fminf(GetFrameTime(),.05f));
        if (!entering && IsKeyPressed(KEY_F3)) showCollisionGuide = !showCollisionGuide;
        if (!entering && IsKeyPressed(KEY_F6)) showSpriteDebug = !showSpriteDebug;
        bool nearExit = clearing.cemetery && CheckCollisionPointRec(player, clearing.exit);
        bool nearPedestal = !clearing.cemetery &&
            CheckCollisionPointCircle(player, clearing.pedestal, CLEARING_ALTAR_RADIUS);
        bool altarOpen = ClearingAltarOpen(&encounter, &clearing);
        bool interactPressed = IsKeyPressed(KEY_E);
        if (!entering && playerHealth > 0 &&
            CanEnterCemetery(&encounter, &clearing, player, interactPressed)) {
            Clearing next = {0};
            if (InitCemetery(&next)) {
                UnloadClearing(&clearing);
                clearing = next;
                player = clearing.spawn;
                RunProgress nextProgress=encounter.progress;
                nextProgress.map=1; nextProgress.stage=1; nextProgress.bossPower=1;
                nextProgress.health=playerHealth;
                BeginEncounter(&encounter,&enemies,&clearing,nextProgress,player);
                SaveClearingCheckpoint(&nextProgress);
                ResetGun(&gun);
                ResetBible(&bible);
                dodgeRemaining = katanaAttackRemaining = 0;
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
        if (dodgeCooldown > 0.0f) dodgeCooldown -= deltaTime;
        if (katanaAttackCooldown > 0.0f) katanaAttackCooldown -= deltaTime;
        if (katanaAttackRemaining > 0.0f)
            katanaAttackRemaining = fmaxf(0.0f, katanaAttackRemaining - deltaTime);
        bool bibleActivationStarted = false;
        if (currentWeapon == WEAPON_BIBLE) {
            float previousActivation = bible.activationRemaining;
            UpdateBible(&bible, entering ? 0.0f : deltaTime,
                        !entering && dodgeRemaining <= 0.0f && !IsKeyPressed(KEY_SPACE) &&
                        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));
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
            katanaAttackRemaining = katanaAttackDuration;
            katanaAttackCooldown = katanaAttackRecovery;
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
                      !entering && !dodging && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
            if (gun.poseRemaining > previousPose) gunAttackFacing = facing;
        }
        if (bibleActivationStarted) bibleAttackFacing = facing;
        if (!entering) {
            Vector2 playerCenter = {player.x, player.y-35.0f};
            for (int i=0; i<ENEMY_COUNT; i++) {
                Enemy *enemy = &enemies.list[i];
                if (!EnemyCanBeHit(enemy)) continue;
                Vector2 center = EnemyCenter(enemy);
                if (currentWeapon == WEAPON_GUN) {
                    int damage = GunHitCircle(&gun, center, EnemyHurtRadius(enemy));
                    if (damage) DamageEnemy(enemy, (int)ceilf(damage*EncounterDamage(&encounter)));
                } else if (currentWeapon == WEAPON_BIBLE && enemy->bibleHitCooldown <= 0.0f &&
                           BibleTouchesCircle(&bible, player, center, EnemyHurtRadius(enemy)) &&
                           MapHasClearPath(&clearing,player,enemy->position)) {
                    DamageEnemy(enemy, (int)ceilf(BibleContactDamage()*EncounterDamage(&encounter)));
                    KnockbackEnemy(enemy, &clearing, player, 36.0f);
                    enemy->bibleHitCooldown = 0.45f;
                } else if (currentWeapon == WEAPON_KATANA && katanaAttackRemaining > 0.0f &&
                           katanaAttackRemaining <= katanaAttackDuration*0.5f &&
                           enemy->lastKatanaSwing != katanaAttackId) {
                    Vector2 delta = Vector2Subtract(center, playerCenter);
                    float distance = Vector2Length(delta);
                    if (distance < katanaAttackRange+EnemyHurtRadius(enemy) && distance > 0.0f &&
                        Vector2DotProduct(delta, katanaAttackDirection) >= -0.12f*distance &&
                        MapHasClearPath(&clearing,player,enemy->position)) {
                        DamageEnemy(enemy, (int)ceilf(25*EncounterDamage(&encounter)));
                        enemy->lastKatanaSwing = katanaAttackId;
                    }
                }
            }
            UpdateEnemies(&enemies, &clearing, player, deltaTime, dodging,
                          &playerHealth, playerMaxHealth);
        }
        int frame = walking ? 1 + (int) (walkTime * 7.0f) % 4 : 0;
        if (!entering && nearPedestal && interactPressed) pedestalMessageTime = 3.0f;
        if (pedestalMessageTime > 0.0f) pedestalMessageTime -= deltaTime;

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                DrawClearing(&clearing);
                if (altarOpen) DrawClearingAltarGlow(&clearing, (float)GetTime());
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
                        // A arte da arma tem o corpo mais estreito que o sprite normal.
                        // Usa a mesma escala para espera, caminhada e disparo.
                        float width = bodyFacing < 2 ? 68.0f : 76.0f;
                        bodyDestination = (Rectangle){player.x - width * 0.5f,
                            player.y - 72.0f, width, 75.0f};
                    }
                    if (attacking && katanaAttackRemaining <= katanaAttackDuration * .5f) {
                        Vector2 center = {player.x, player.y - 35};
                        float angle = atan2f(katanaAttackDirection.y, katanaAttackDirection.x) * RAD2DEG;
                        float halfArc = acosf(-.12f) * RAD2DEG;
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
                    DrawText(TextFormat("P face:%d frame:%d scale:%.2f", facing, frame,
                                        camera.zoom*0.9375f),
                             (int)player.x-55, (int)player.y-100, 12, WHITE);
                }
            EndMode2D();

            int hudOffset = EncounterBossVisible(&encounter, &enemies) ? ENCOUNTER_BOSS_HUD_HEIGHT : 0;
            DrawRectangle(12, 12 + hudOffset, 490, 130, Fade(BLACK, 0.72f));
            DrawText(currentWeapon == WEAPON_BIBLE ? "WASD: mover  |  Espaco: esquiva" :
                     "WASD: mover  |  Espaco/botao direito: esquiva", 24, 23 + hudOffset, 17, RAYWHITE);
            DrawText("F3: colisao | ESC: pausar", 24, 76 + hudOffset, 17, RAYWHITE);
            DrawText("F4/F5: testar dano e cura", 24, 104 + hudOffset, 17, RAYWHITE);
            if (altarOpen) {
                DrawText(nearPedestal ? "E: usar o altar e entrar no cemiterio"
                                      : "Siga o brilho dourado ate o altar",
                         24, 53 + hudOffset, 18, GOLD);
            } else if (pedestalMessageTime > 0.0f) {
                DrawText("Conclua as ondas e sele o Guardiao", 24, 53 + hudOffset, 18, GOLD);
            } else if (nearPedestal) {
                DrawText("O altar esta selado", 24, 53 + hudOffset, 18, GOLD);
            } else {
                DrawText(nearExit ? "Vila: proxima etapa do desenvolvimento"
                    : clearing.cemetery ? "Cemiterio das Raizes" : "Explore a clareira",
                    24, 53 + hudOffset, 18, RAYWHITE);
            }
            DrawHud(&hud, playerHealth, playerMaxHealth,
                    dodgeCooldown, dodgeRecovery, hudOffset);
            DrawText(TextFormat("Arma: %s", WeaponName((Weapon)currentWeapon)),
                     24, 150 + hudOffset, 19, GOLD);
            if (currentWeapon == WEAPON_KATANA)
                DrawText("Clique esquerdo: corte de katana", 24, 176 + hudOffset, 18, RAYWHITE);
            if (currentWeapon == WEAPON_BIBLE)
                DrawText(bible.boostCooldown <= 0.0f
                             ? "Botao direito: acelerar orbitas"
                             : TextFormat("Aceleracao: %.1f s", bible.boostCooldown),
                         24, 176 + hudOffset, 18, RAYWHITE);
            if (currentWeapon == WEAPON_GUN)
                DrawText("Clique esquerdo: disparar (um tiro por clique)", 24, 176 + hudOffset, 18, RAYWHITE);
            DrawEncounter(&encounter,&enemies);
            if (entering) {
                float t = entryFadeRemaining / entryFadeDuration;
                float opacity = t*t*(3.0f - 2.0f*t);
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Fade(BLACK, opacity));
            }
        EndDrawing();
    }

    UnloadOpeningCutscene(&opening);
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
    CloseWindow();
    return 0;
}
