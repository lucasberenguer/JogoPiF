#include "raylib.h"
#include "raymath.h"
#include "clearing.h"
#include "hud.h"
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
    int steps = (int) (Vector2Length(movement) / 2.0f) + 1;
    Vector2 step = Vector2Scale(movement, 1.0f / (float) steps);
    for (int index = 0; index < steps; index++) {
        Vector2 next = Vector2Add(*position, step);
        if (!IsClearingBlocked(clearing, next)) {
            *position = next;
            continue;
        }
        Vector2 alongX = { position->x + step.x, position->y };
        Vector2 alongY = { position->x, position->y + step.y };
        bool xFree = step.x != 0.0f && !IsClearingBlocked(clearing, alongX);
        bool yFree = step.y != 0.0f && !IsClearingBlocked(clearing, alongY);
        if (xFree && (!yFree || fabsf(step.x) >= fabsf(step.y))) {
            *position = alongX;
        } else if (yFree) {
            *position = alongY;
        }
    }
}

static Vector2 FacingVector(int facing) {
    switch (facing) {
        case 1: return (Vector2) { 0.0f, -1.0f };
        case 2: return (Vector2) { -1.0f, 0.0f };
        case 3: return (Vector2) { 1.0f, 0.0f };
        default: return (Vector2) { 0.0f, 1.0f };
    }
}

int main(void) {
    InitWindow(1280, 720, "Jogo PIF - Clareira");
    const int monitor = GetCurrentMonitor();
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    ToggleFullscreen();
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

    Hud hud = { 0 };
    if (!InitHud(&hud)) {
        UnloadTexture(dodgeSprites);
        UnloadTexture(protagonist);
        UnloadClearing(&clearing);
        CloseWindow();
        return 1;
    }

    Vector2 player = clearing.spawn;
    int playerHealth = 100;
    int playerMaxHealth = 100;
    const float playerSpeed = 240.0f;
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
    float pedestalMessageTime = 0.0f;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        if (IsKeyPressed(KEY_F3)) showCollisionGuide = !showCollisionGuide;
        if (IsKeyPressed(KEY_F4)) {
            playerHealth -= 5;
            if (playerHealth < 0) playerHealth = 0;
        }
        if (IsKeyPressed(KEY_F5)) {
            playerHealth += 5;
            if (playerHealth > playerMaxHealth) playerHealth = playerMaxHealth;
        }
        if (dodgeCooldown > 0.0f) dodgeCooldown -= deltaTime;

        Vector2 direction = {
            (float) IsKeyDown(KEY_D) - (float) IsKeyDown(KEY_A),
            (float) IsKeyDown(KEY_S) - (float) IsKeyDown(KEY_W)
        };
        bool dodgePressed = IsKeyPressed(KEY_SPACE) ||
                            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        if (dodgePressed && dodgeRemaining <= 0.0f && dodgeCooldown <= 0.0f) {
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
        int frame = walking ? 1 + (int) (walkTime * 7.0f) % 4 : 0;

        Camera2D camera = FitClearing(&clearing);
        bool nearPedestal = CheckCollisionPointCircle(player, clearing.pedestal, 72.0f);
        if (nearPedestal && IsKeyPressed(KEY_E)) pedestalMessageTime = 3.0f;
        if (pedestalMessageTime > 0.0f) pedestalMessageTime -= deltaTime;

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                DrawClearing(&clearing);
                if (dodging) {
                    DrawTexturePro(dodgeSprites,
                        (Rectangle) { dodgeFrame * 112.0f, facing * 80.0f, 112.0f, 80.0f },
                        (Rectangle) { player.x - 52.5f, player.y - 72.0f, 105.0f, 75.0f },
                        (Vector2) { 0, 0 }, 0.0f, WHITE);
                } else {
                    DrawTexturePro(protagonist,
                        (Rectangle) { frame * 64.0f, facing * 80.0f, 64.0f, 80.0f },
                        (Rectangle) { player.x - 30.0f, player.y - 72.0f, 60.0f, 75.0f },
                        (Vector2) { 0, 0 }, 0.0f, WHITE);
                }
                DrawClearingForeground(&clearing, player);
                if (showCollisionGuide) {
                    DrawClearingCollisionGuide(&clearing);
                }
            EndMode2D();

            DrawRectangle(12, 12, 490, 130, Fade(BLACK, 0.72f));
            DrawText("WASD: mover  |  Espaco/botao direito: esquiva", 24, 23, 17, RAYWHITE);
            DrawText("F3: colisao  |  ESC: sair", 24, 76, 17, RAYWHITE);
            DrawText("F4/F5: testar dano e cura", 24, 104, 17, RAYWHITE);
            if (pedestalMessageTime > 0.0f) {
                DrawText("O pedestal ainda esta selado", 24, 53, 18, GOLD);
            } else if (nearPedestal) {
                DrawText("E: pedestal (ainda selado)", 24, 53, 18, GOLD);
            } else {
                DrawText("Explore a clareira", 24, 53, 18, RAYWHITE);
            }
            DrawHud(&hud, playerHealth, playerMaxHealth,
                    dodgeCooldown, dodgeRecovery);
        EndDrawing();
    }

    UnloadHud(&hud);
    UnloadTexture(dodgeSprites);
    UnloadTexture(protagonist);
    UnloadClearing(&clearing);
    CloseWindow();
    return 0;
}
