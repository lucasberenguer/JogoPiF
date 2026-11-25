#include "video_settings.h"
#include "game_text.h"
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>

static const int renderScales[VIDEO_RENDER_SCALES] = {50, 75, 100};
static const int fpsOptions[VIDEO_FPS_OPTIONS] = {30, 60};

static const VideoSettings defaults = {
    .fullscreen = true, .renderScale = 100, .fpsLimit = 60, .dynamicLighting = true
};
static VideoSettings settings = {
    .fullscreen = true, .renderScale = 100, .fpsLimit = 60, .dynamicLighting = true
};
static RenderTexture2D frameTarget = {0};
static bool drawingToTarget = false;
static Shader worldLighting;
static bool triedWorldLighting;
static int lightScreenSize, lightCount, lightArray, lightTints, lightAmbient;
static int shadowLightMask, shadowCount, shadowStartsLocation, shadowShapesLocation;

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
    *video = defaults;
    const char *path = VideoSettingsPath();
    char *data = FileExists(path) ? LoadFileText(path) : NULL;
    if (!data) return;
    int fullscreen, scale, fps, showFps, lighting = 1, reduceMotion = 0;
    if (sscanf(data, "fullscreen=%d render=%d fps=%d show_fps=%d dynamic_lighting=%d reduce_menu_motion=%d",
               &fullscreen, &scale, &fps, &showFps, &lighting, &reduceMotion) >= 4 &&
        (fullscreen == 0 || fullscreen == 1) && IndexOf(renderScales, VIDEO_RENDER_SCALES, scale) >= 0 &&
        IndexOf(fpsOptions, VIDEO_FPS_OPTIONS, fps) >= 0 && (showFps == 0 || showFps == 1)) {
        *video = (VideoSettings){.fullscreen = fullscreen == 1, .renderScale = scale, .fpsLimit = fps,
            .showFps = showFps == 1, .dynamicLighting = lighting != 0, .reduceMenuMotion = reduceMotion != 0};
    } else TraceLog(LOG_WARNING, "video_pif.cfg invalido; usando as opcoes padrao.");
    UnloadFileText(data);
}

void SaveVideoSettings(const VideoSettings *video) {
    char data[128];
    snprintf(data, sizeof(data), "fullscreen=%d render=%d fps=%d show_fps=%d dynamic_lighting=%d reduce_menu_motion=%d\n",
             video->fullscreen, video->renderScale, video->fpsLimit, video->showFps, video->dynamicLighting,
             video->reduceMenuMotion);
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

static void BeginVideoFrame(bool forceTarget, TextureFilter filter) {
    BeginDrawing();
    drawingToTarget = false;
    if (settings.renderScale >= 100 && !forceTarget) return;
    int screenWidth = GetScreenWidth(), screenHeight = GetScreenHeight();
    int width = (int)fmaxf(1, roundf(screenWidth * settings.renderScale / 100.0f));
    int height = (int)fmaxf(1, roundf(screenHeight * settings.renderScale / 100.0f));
    if (!frameTarget.id || frameTarget.texture.width != width || frameTarget.texture.height != height) {
        if (frameTarget.id) UnloadRenderTexture(frameTarget);
        frameTarget = LoadRenderTexture(width, height);
        if (!frameTarget.id) return;
    }
    SetTextureFilter(frameTarget.texture, filter);
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

void BeginGameFrame(void) {
    BeginVideoFrame(false, TEXTURE_FILTER_BILINEAR);
}

void BeginWorldFrame(void) {
    if (settings.dynamicLighting && !triedWorldLighting) {
        triedWorldLighting = true;
        worldLighting = LoadShader(NULL, "assets/shaders/world_light.fs");
        if (worldLighting.id && worldLighting.id != rlGetShaderIdDefault()) {
            lightScreenSize = GetShaderLocation(worldLighting, "screenSize");
            lightCount = GetShaderLocation(worldLighting, "lightCount");
            lightArray = GetShaderLocation(worldLighting, "lights[0]");
            lightTints = GetShaderLocation(worldLighting, "tints[0]");
            lightAmbient = GetShaderLocation(worldLighting, "ambientGrade");
            shadowLightMask = GetShaderLocation(worldLighting, "shadowLightMask");
            shadowCount = GetShaderLocation(worldLighting, "shadowCount");
            shadowStartsLocation = GetShaderLocation(worldLighting, "shadowStarts[0]");
            shadowShapesLocation = GetShaderLocation(worldLighting, "shadowShapes[0]");
        } else worldLighting = (Shader){0};
    }
    BeginVideoFrame(settings.dynamicLighting && worldLighting.id, TEXTURE_FILTER_POINT);
}

static void DrawFrameTexture(void) {
    Texture2D texture = frameTarget.texture;
    DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width, -(float)texture.height},
                   (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                   (Vector2){0}, 0, WHITE);
}

static bool WorldPointVisible(Vector2 point, float margin, Vector2 size) {
    return point.x + margin >= 0 && point.y + margin >= 0 &&
           point.x - margin <= size.x && point.y - margin <= size.y;
}

void DrawWorldContactShadows(Camera2D camera, const SceneLights *scene) {
    Vector2 size = {(float)GetScreenWidth(), (float)GetScreenHeight()};
    for (int i = 0; i < scene->casterCount; i++) {
        const SceneCaster *caster = &scene->casters[i];
        if (caster->contactOpacity <= 0.0f) continue;
        Vector2 point = GetWorldToScreen2D(caster->feet, camera);
        if (!WorldPointVisible(point, caster->halfWidth*camera.zoom, size)) continue;
        rlPushMatrix();
        rlTranslatef(caster->feet.x, caster->feet.y, 0);
        rlScalef(1.0f, .38f, 1.0f);
        DrawCircleGradient(0, 0, caster->halfWidth, Fade(BLACK,caster->contactOpacity), BLANK);
        rlPopMatrix();
    }
}

static void UploadWorldShadows(Camera2D camera, const SceneLights *scene, Vector2 size,
                               const Vector4 *lights, int visibleLights) {
    int count = 0, mask = 0;
    Vector4 starts[SCENE_SHADOW_LIMIT];
    Vector3 shapes[SCENE_SHADOW_LIMIT];
    for (int kind = SCENE_MAIN_ACTOR; kind < SCENE_CASTER_KINDS && count < SCENE_SHADOW_LIMIT && visibleLights > 0; kind++) {
        for (int i = 0; i < scene->casterCount && count < SCENE_SHADOW_LIMIT; i++) {
            const SceneCaster *caster = &scene->casters[i];
            if ((int)caster->kind != kind) continue;
            Vector2 from = GetWorldToScreen2D(caster->feet, camera);
            float maxReach = caster->reach*camera.zoom;
            float halfWidth = caster->halfWidth*camera.zoom;
            if (!WorldPointVisible(from, maxReach+halfWidth, size)) continue;
            // Usa a contribuição local, para que uma tocha distante não roube a sombra.
            int best = -1;
            float score = .018f, distance = 0, reach = 0;
            Vector2 away = {0};
            for (int j = 0; j < visibleLights; j++) {
                Vector2 delta = {from.x-lights[j].x, from.y-lights[j].y};
                float d = Vector2Length(delta);
                if (d < fmaxf(1, halfWidth*.42f)) continue;
                float fraction = Vector2Length((Vector2){delta.x,delta.y*1.18f}) / lights[j].z;
                if (fraction >= 1) continue;
                float contribution = lights[j].w*(1-fraction*fraction*(3-2*fraction));
                if (contribution <= score) continue;
                score = contribution;
                best = j;
                distance = d;
                away = delta;
                reach = fminf(maxReach, lights[j].z-d+halfWidth);
            }
            if (best < 0) continue;
            if (reach < 8) continue;
            Vector2 axis = Vector2Scale(away,1.0f/distance);
            float width = halfWidth*(.58f+.42f*fabsf(axis.y));
            starts[count] = (Vector4){from.x, from.y, axis.x, axis.y};
            shapes[count++] = (Vector3){reach,width,(float)best};
            mask |= 1 << best;
        }
    }
    SetShaderValue(worldLighting, shadowLightMask, &mask, SHADER_UNIFORM_INT);
    SetShaderValue(worldLighting, shadowCount, &count, SHADER_UNIFORM_INT);
    if (count > 0) {
        SetShaderValueV(worldLighting, shadowStartsLocation, starts, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(worldLighting, shadowShapesLocation, shapes, SHADER_UNIFORM_VEC3, count);
    }
}

void DrawWorldFrame(Camera2D camera, const SceneLights *scene) {
    if (!drawingToTarget) return;
    EndTextureMode();
    drawingToTarget = false;
    bool lit = settings.dynamicLighting && worldLighting.id;
    if (lit) {
        Vector4 screenLights[SCENE_LIGHT_LIMIT];
        Vector3 tints[SCENE_LIGHT_LIMIT];
        Vector2 size = {(float)GetScreenWidth(), (float)GetScreenHeight()};
        int count = 0;
        Vector2 ambient = scene->ambient;
        for (int i = 0; i < scene->count; i++) {
            SceneLight light = scene->lights[i];
            Vector2 position = GetWorldToScreen2D(light.position, camera);
            float radius = light.radius * camera.zoom;
            if (!WorldPointVisible(position,radius,size)) continue;
            screenLights[count] = (Vector4){position.x, position.y, radius, light.intensity};
            tints[count++] = light.tint;
        }
        SetShaderValue(worldLighting, lightScreenSize, &size, SHADER_UNIFORM_VEC2);
        SetShaderValue(worldLighting, lightCount, &count, SHADER_UNIFORM_INT);
        SetShaderValue(worldLighting, lightAmbient, &ambient, SHADER_UNIFORM_VEC2);
        UploadWorldShadows(camera, scene, size, screenLights, count);
        if (count > 0) {
            SetShaderValueV(worldLighting, lightArray, screenLights, SHADER_UNIFORM_VEC4, count);
            SetShaderValueV(worldLighting, lightTints, tints, SHADER_UNIFORM_VEC3, count);
        }
        BeginShaderMode(worldLighting);
    }
    DrawFrameTexture();
    if (lit) EndShaderMode();
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
        DrawFrameTexture();
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
    if (worldLighting.id) UnloadShader(worldLighting);
    worldLighting = (Shader){0};
    triedWorldLighting = false;
}
