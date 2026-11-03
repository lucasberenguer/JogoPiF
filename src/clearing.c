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

    Image foregroundImage = LoadImage("assets/clareira.png");
    Image foregroundMask = LoadImage("assets/clareira_foreground_mask.png");
    if (foregroundImage.data == NULL || foregroundMask.data == NULL ||
        foregroundMask.width != clearing->background.width ||
        foregroundMask.height != clearing->background.height) {
        TraceLog(LOG_ERROR, "Mascara dos galhos ausente ou com tamanho diferente do mapa");
        if (foregroundImage.data != NULL) UnloadImage(foregroundImage);
        if (foregroundMask.data != NULL) UnloadImage(foregroundMask);
        UnloadClearing(clearing);
        return false;
    }
    ImageFormat(&foregroundImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    clearing->foregroundMaskPixels = LoadImageColors(foregroundMask);
    UnloadImage(foregroundMask);
    if (clearing->foregroundMaskPixels == NULL) {
        UnloadImage(foregroundImage);
        UnloadClearing(clearing);
        return false;
    }
    Color *foregroundPixels = (Color *) foregroundImage.data;
    int count = foregroundImage.width * foregroundImage.height;
    for (int index = 0; index < count; index++) {
        foregroundPixels[index].a = clearing->foregroundMaskPixels[index].a;
    }
    clearing->foreground = LoadTextureFromImage(foregroundImage);
    UnloadImage(foregroundImage);
    if (clearing->foreground.id == 0) {
        UnloadClearing(clearing);
        return false;
    }
    SetTextureFilter(clearing->foreground, TEXTURE_FILTER_POINT);

    // A mascara bloqueia as marcas vermelhas; este retangulo fecha o resto da borda.
    clearing->playableBounds = (Rectangle) {
        50.0f, 150.0f,
        (float) clearing->background.width - 100.0f,
        (float) clearing->background.height - 166.0f
    };
    clearing->spawn = (Vector2) { 790.0f, 894.0f };
    clearing->pedestal = (Vector2) { 869.0f, 162.0f };
    return true;
}

void DrawClearing(const Clearing *clearing) {
    DrawTexture(clearing->background, 0, 0, WHITE);
}

void DrawClearingForeground(const Clearing *clearing, Vector2 player) {
    int x = (int) player.x;
    int y = (int) player.y;
    bool behindBranch = x >= 0 && y >= 0 &&
        x < clearing->background.width && y < clearing->background.height &&
        clearing->foregroundMaskPixels[y * clearing->background.width + x].a > 0;
    int middle = clearing->background.width / 2;
    DrawTextureRec(clearing->foreground,
        (Rectangle) { 0, 0, (float) middle, (float) clearing->background.height },
        (Vector2) { 0, 0 },
        behindBranch && x < middle ? Fade(WHITE, 0.7f) : WHITE);
    DrawTextureRec(clearing->foreground,
        (Rectangle) { (float) middle, 0,
                      (float) (clearing->background.width - middle),
                      (float) clearing->background.height },
        (Vector2) { (float) middle, 0 },
        behindBranch && x >= middle ? Fade(WHITE, 0.7f) : WHITE);
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
    if (clearing->foregroundMaskPixels != NULL) {
        UnloadImageColors(clearing->foregroundMaskPixels);
        clearing->foregroundMaskPixels = NULL;
    }
    if (clearing->foreground.id != 0) {
        UnloadTexture(clearing->foreground);
        clearing->foreground.id = 0;
    }
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
