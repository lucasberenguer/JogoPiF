#include "video_settings.h"
#include "game_text.h"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>

static const int renderScales[VIDEO_RENDER_SCALES] = {50, 75, 100};
static const int fpsOptions[VIDEO_FPS_OPTIONS] = {30, 60};

static VideoSettings settings = {.fullscreen = true, .renderScale = 100, .fpsLimit = 60, .showFps = false};
static RenderTexture2D frameTarget = {0};
static bool drawingToTarget = false;

static const char *VideoSettingsPath(void) {
    return TextFormat("%s/video_pif.cfg", GetApplicationDirectory());
}

static int IndexOf(const int *values, int count, int value) {
    for (int i = 0; i < count; i++) if (values[i] == value) return i;
    return -1;
}

VideoSettings *GetVideoSettings(void) {
    return &settings;
}

void LoadVideoSettings(VideoSettings *video) {
    *video = (VideoSettings){.fullscreen = true, .renderScale = 100, .fpsLimit = 60, .showFps = false};
    const char *path = VideoSettingsPath();
    char *data = FileExists(path) ? LoadFileText(path) : NULL;
    if (!data) return;
    int fullscreen, scale, fps, showFps;
    if (sscanf(data, "fullscreen=%d render=%d fps=%d show_fps=%d", &fullscreen, &scale, &fps, &showFps) == 4 &&
        (fullscreen == 0 || fullscreen == 1) && IndexOf(renderScales, VIDEO_RENDER_SCALES, scale) >= 0 &&
        IndexOf(fpsOptions, VIDEO_FPS_OPTIONS, fps) >= 0 && (showFps == 0 || showFps == 1)) {
        *video = (VideoSettings){fullscreen == 1, scale, fps, showFps == 1};
    } else TraceLog(LOG_WARNING, "video_pif.cfg invalido; usando as opcoes padrao.");
    UnloadFileText(data);
}

void SaveVideoSettings(const VideoSettings *video) {
    char data[96];
    snprintf(data, sizeof(data), "fullscreen=%d render=%d fps=%d show_fps=%d\n",
             video->fullscreen, video->renderScale, video->fpsLimit, video->showFps);
    if (!SaveFileText(VideoSettingsPath(), data))
        TraceLog(LOG_WARNING, "Nao foi possivel salvar as opcoes de video.");
}

void ApplyVideoWindowSize(void) {
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) return;
    int monitor = GetCurrentMonitor();
    int width = VIDEO_WINDOW_WIDTH, height = VIDEO_WINDOW_HEIGHT;
    if (GetMonitorWidth(monitor) > 0 && width > GetMonitorWidth(monitor)) width = GetMonitorWidth(monitor);
    if (GetMonitorHeight(monitor) > 0 && height > GetMonitorHeight(monitor)) height = GetMonitorHeight(monitor);
    Vector2 origin = GetMonitorPosition(monitor);
    SetWindowSize(width, height);
    SetWindowPosition((int)origin.x + (GetMonitorWidth(monitor) - width) / 2,
                      (int)origin.y + (GetMonitorHeight(monitor) - height) / 2);
}

void CycleVideoRenderScale(VideoSettings *video, int direction) {
    int index = IndexOf(renderScales, VIDEO_RENDER_SCALES, video->renderScale);
    if (index < 0) index = VIDEO_RENDER_SCALES - 1;
    video->renderScale = renderScales[(index + direction + VIDEO_RENDER_SCALES) % VIDEO_RENDER_SCALES];
}

void CycleVideoFpsLimit(VideoSettings *video, int direction) {
    int index = IndexOf(fpsOptions, VIDEO_FPS_OPTIONS, video->fpsLimit);
    if (index < 0) index = VIDEO_FPS_OPTIONS - 1;
    video->fpsLimit = fpsOptions[(index + direction + VIDEO_FPS_OPTIONS) % VIDEO_FPS_OPTIONS];
}

void BeginGameFrame(void) {
    BeginDrawing();
    drawingToTarget = false;
    if (settings.renderScale >= 100) return;
    int screenWidth = GetScreenWidth(), screenHeight = GetScreenHeight();
    int width = (int)fmaxf(1, roundf(screenWidth * settings.renderScale / 100.0f));
    int height = (int)fmaxf(1, roundf(screenHeight * settings.renderScale / 100.0f));
    if (!frameTarget.id || frameTarget.texture.width != width || frameTarget.texture.height != height) {
        if (frameTarget.id) UnloadRenderTexture(frameTarget);
        frameTarget = LoadRenderTexture(width, height);
        if (!frameTarget.id) return;
        SetTextureFilter(frameTarget.texture, TEXTURE_FILTER_POINT);
    }
    BeginTextureMode(frameTarget);
    /* Mantém as coordenadas de tela: todo o desenho do jogo continua usando
       GetScreenWidth/GetScreenHeight e cabe na imagem menor. */
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, screenWidth, screenHeight, 0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    drawingToTarget = true;
}

static void DrawFpsCounter(void) {
    const char *label = TextFormat("%d FPS", GetFPS());
    int size = (int)fmaxf(18, GetScreenHeight() / 40.0f);
    int width = MeasureGameText(label, size);
    int x = GetScreenWidth() - width - 16, y = GetScreenHeight() - size - 14;
    DrawRectangle(x - 8, y - 4, width + 16, size + 8, Fade(BLACK, .55f));
    DrawGameText(label, x, y, size, GetFPS() + 2 >= settings.fpsLimit ? GREEN : YELLOW);
}

void EndGameFrame(void) {
    if (drawingToTarget) {
        EndTextureMode();
        Texture2D texture = frameTarget.texture;
        DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width, -(float)texture.height},
                       (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                       (Vector2){0}, 0, WHITE);
        drawingToTarget = false;
    }
    if (settings.showFps) DrawFpsCounter();
    EndDrawing();
}

void BeginGameScissor(int x, int y, int width, int height) {
    if (!drawingToTarget) {
        BeginScissorMode(x, y, width, height);
        return;
    }
    float sx = (float)frameTarget.texture.width / GetScreenWidth();
    float sy = (float)frameTarget.texture.height / GetScreenHeight();
    BeginScissorMode((int)floorf(x * sx), (int)floorf(y * sy),
                     (int)ceilf(width * sx), (int)ceilf(height * sy));
}

void UnloadVideoFrame(void) {
    if (frameTarget.id) UnloadRenderTexture(frameTarget);
    frameTarget = (RenderTexture2D){0};
}
