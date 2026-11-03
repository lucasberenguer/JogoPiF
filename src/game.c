#include "raylib.h"
#include "raymath.h"
#include "clearing.h"

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

    Vector2 player = clearing.spawn;
    const float playerSpeed = 240.0f;
    bool showCollisionGuide = false;
    float pedestalMessageTime = 0.0f;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F3)) showCollisionGuide = !showCollisionGuide;

        Vector2 direction = {
            (float) IsKeyDown(KEY_D) - (float) IsKeyDown(KEY_A),
            (float) IsKeyDown(KEY_S) - (float) IsKeyDown(KEY_W)
        };
        if (Vector2LengthSqr(direction) > 0.0f) {
            direction = Vector2Normalize(direction);
            Vector2 movement = Vector2Scale(direction, playerSpeed * GetFrameTime());
            Vector2 next = { player.x + movement.x, player.y };
            if (!IsClearingBlocked(&clearing, next)) player.x = next.x;
            next = (Vector2) { player.x, player.y + movement.y };
            if (!IsClearingBlocked(&clearing, next)) player.y = next.y;
        }

        Camera2D camera = FitClearing(&clearing);
        bool nearPedestal = CheckCollisionPointCircle(player, clearing.pedestal, 72.0f);
        if (nearPedestal && IsKeyPressed(KEY_E)) pedestalMessageTime = 3.0f;
        if (pedestalMessageTime > 0.0f) pedestalMessageTime -= GetFrameTime();

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
                DrawClearing(&clearing);
                if (showCollisionGuide) {
                    DrawClearingCollisionGuide(&clearing);
                }
                DrawCircleV(player, 16.0f, (Color) { 66, 172, 204, 255 });
                DrawCircleLines((int) player.x, (int) player.y, 16.0f, WHITE);
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

    UnloadClearing(&clearing);
    CloseWindow();
    return 0;
}
