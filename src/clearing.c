#include "clearing.h"
#include <stddef.h>

bool InitClearing(Clearing *clearing) {
    clearing->background = LoadTexture("assets/clareira.png");
    if (clearing->background.id == 0) {
        TraceLog(LOG_ERROR, "Nao foi possivel carregar assets/clareira.png");
        return false;
    }

    SetTextureFilter(clearing->background, TEXTURE_FILTER_POINT);

    Image collisionImage = LoadImage("assets/clareira_collision.png");
    if (collisionImage.data == NULL ||
        collisionImage.width != clearing->background.width ||
        collisionImage.height != clearing->background.height) {
        TraceLog(LOG_ERROR, "Mascara de colisao ausente ou com tamanho diferente do mapa");
        if (collisionImage.data != NULL) UnloadImage(collisionImage);
        UnloadTexture(clearing->background);
        clearing->background.id = 0;
        return false;
    }
    clearing->collisionPixels = LoadImageColors(collisionImage);
    clearing->collisionGuide = LoadTextureFromImage(collisionImage);
    UnloadImage(collisionImage);
    if (clearing->collisionPixels == NULL || clearing->collisionGuide.id == 0) {
        TraceLog(LOG_ERROR, "Nao foi possivel preparar a mascara de colisao");
        UnloadClearing(clearing);
        return false;
    }
    SetTextureFilter(clearing->collisionGuide, TEXTURE_FILTER_POINT);

    // A mascara bloqueia as marcas vermelhas; este retangulo fecha o resto da borda.
    clearing->playableBounds = (Rectangle) {
        90.0f, 160.0f,
        (float) clearing->background.width - 180.0f,
        (float) clearing->background.height - 230.0f
    };
    clearing->spawn = (Vector2) { 757.0f, 821.0f };
    clearing->pedestal = (Vector2) { 869.0f, 162.0f };
    return true;
}

void DrawClearing(const Clearing *clearing) {
    DrawTexture(clearing->background, 0, 0, WHITE);
}

void DrawClearingCollisionGuide(const Clearing *clearing) {
    DrawTexture(clearing->collisionGuide, 0, 0, WHITE);
    DrawRectangleLinesEx(clearing->playableBounds, 3.0f, RED);
    DrawCircleLines((int) clearing->pedestal.x,
                    (int) clearing->pedestal.y, 72.0f, BLUE);
}

bool IsClearingBlocked(const Clearing *clearing, Vector2 point) {
    if (!CheckCollisionPointRec(point, clearing->playableBounds)) return true;

    int x = (int) point.x;
    int y = (int) point.y;
    int width = clearing->background.width;
    int height = clearing->background.height;
    if (x < 0 || y < 0 || x >= width || y >= height) return true;
    return clearing->collisionPixels[y * width + x].a > 0;
}

void UnloadClearing(Clearing *clearing) {
    if (clearing->collisionPixels != NULL) {
        UnloadImageColors(clearing->collisionPixels);
        clearing->collisionPixels = NULL;
    }
    if (clearing->collisionGuide.id != 0) {
        UnloadTexture(clearing->collisionGuide);
        clearing->collisionGuide.id = 0;
    }
    if (clearing->background.id != 0) {
        UnloadTexture(clearing->background);
        clearing->background.id = 0;
    }
}
