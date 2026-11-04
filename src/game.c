#include "raylib.h"
#include "raymath.h"
#include "clearing.h"
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

    Vector2 player = clearing.spawn;
    const float playerSpeed = 240.0f;
    int facing = 0; // 0: baixo, 1: cima, 2: esquerda, 3: direita
    float walkTime = 0.0f;
    bool walking = false;
    bool showCollisionGuide = false;
    float pedestalMessageTime = 0.0f;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F3)) showCollisionGuide = !showCollisionGuide;

        Vector2 direction = {
            (float) IsKeyDown(KEY_D) - (float) IsKeyDown(KEY_A),
            (float) IsKeyDown(KEY_S) - (float) IsKeyDown(KEY_W)
        };
        walking = false;
        Vector2 previous = player;
        if (Vector2LengthSqr(direction) > 0.0f) {
            direction = Vector2Normalize(direction);
            Vector2 movement = Vector2Scale(direction, playerSpeed * GetFrameTime());
            int steps = (int) (Vector2Length(movement) / 4.0f) + 1;
            Vector2 step = Vector2Scale(movement, 1.0f / (float) steps);
            for (int index = 0; index < steps; index++) {
                Vector2 next = { player.x + step.x, player.y };
                if (!IsClearingBlocked(&clearing, next)) player.x = next.x;
                next = (Vector2) { player.x, player.y + step.y };
                if (!IsClearingBlocked(&clearing, next)) player.y = next.y;
            }
        }
        Vector2 traveled = Vector2Subtract(player, previous);
        walking = Vector2LengthSqr(traveled) > 0.01f;
        if (walking) {
            int newFacing = fabsf(traveled.x) >= fabsf(traveled.y) &&
                fabsf(traveled.x) > 0.01f
                ? (traveled.x < 0.0f ? 2 : 3)
                : (traveled.y < 0.0f ? 1 : 0);
            if (newFacing != facing) walkTime = 0.0f;
            facing = newFacing;
            walkTime += Vector2Length(traveled) / playerSpeed;
        } else {
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
        if (pedestalMessageTime > 0.0f) pedestalMessageTime -= GetFrameTime();

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                DrawClearing(&clearing);
                DrawTexturePro(protagonist,
                    (Rectangle) { frame * 64.0f, facing * 80.0f, 64.0f, 80.0f },
                    (Rectangle) { player.x - 30.0f, player.y - 72.0f, 60.0f, 75.0f },
                    (Vector2) { 0, 0 }, 0.0f, WHITE);
                DrawClearingForeground(&clearing, player);
                if (showCollisionGuide) {
                    DrawClearingCollisionGuide(&clearing);
                }
            EndMode2D();

            DrawRectangle(12, 12, 380, 76, Fade(BLACK, 0.72f));
            DrawText("WASD: mover  |  F3: colisao  |  ESC: sair", 24, 23, 17, RAYWHITE);
            if (pedestalMessageTime > 0.0f) {
                DrawText("O pedestal ainda esta selado", 24, 53, 18, GOLD);
            } else if (nearPedestal) {
                DrawText("E: pedestal (ainda selado)", 24, 53, 18, GOLD);
            } else {
                DrawText("Explore a clareira", 24, 53, 18, RAYWHITE);
            }
        EndDrawing();
    }

    UnloadTexture(protagonist);
    UnloadClearing(&clearing);
    CloseWindow();
    return 0;
}
