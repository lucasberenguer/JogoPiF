#include "menu_scene.h"
#include "video_settings.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LAYERS = 80, MAX_WIND_KEYS = 32, STAR_COUNT = 32, PARTICLE_COUNT = 43, PLANT_FLUTTER_FRAMES = 8 };

typedef struct SceneLayer {
    Texture2D texture;
    int x, y, width, height, columns, frames;
    float period, driftX, driftY, opacity, delay;
    float start, duration;
    char kind;
} SceneLayer;

struct MenuScene {
    SceneLayer layers[MAX_LAYERS];
    int count;
    Vector2 wind[MAX_WIND_KEYS];
    int windCount;
    float windPeriod;
};

static float Ease(float value) {
    float t = fminf(1, fmaxf(0, value));
    return t*t*(3-2*t);
}

static void LoadWind(MenuScene *scene) {
    char *schedule = LoadFileText("assets/menu/metadata/vento.txt");
    if (!schedule) return;
    const char *line = schedule;
    if (sscanf(line, "%f", &scene->windPeriod) == 1 && scene->windPeriod > 0) {
        while ((line = strchr(line, '\n')) && line[1] && scene->windCount < MAX_WIND_KEYS) {
            line++;
            Vector2 key;
            if (sscanf(line, "%f %f", &key.x, &key.y) != 2) break;
            if (scene->windCount && key.x <= scene->wind[scene->windCount-1].x) break;
            scene->wind[scene->windCount++] = key;
        }
    }
    UnloadFileText(schedule);
}

static float WindAt(const MenuScene *scene, float time) {
    if (scene->windCount < 2) return .15f;
    float local = fmodf(time, scene->windPeriod);
    if (local < 0) local += scene->windPeriod;
    for (int i = 1; i < scene->windCount; i++) {
        Vector2 a = scene->wind[i-1], b = scene->wind[i];
        if (local <= b.x) return a.y + (b.y-a.y)*Ease((local-a.x)/(b.x-a.x));
    }
    return scene->wind[scene->windCount-1].y;
}

MenuScene *LoadMenuScene(void) {
    char *manifest = LoadFileText("assets/menu/metadata/layers.txt");
    if (!manifest) return NULL;
    MenuScene *scene = calloc(1, sizeof(*scene));
    if (!scene) { UnloadFileText(manifest); return NULL; }
    const char *line = manifest;
    while (*line && scene->count < MAX_LAYERS) {
        char path[160];
        SceneLayer layer = {0};
        if (*line != '#' && sscanf(line, "%159s %c %d %d %d %d %d %d %f %f %f %f %f %f %f",
            path, &layer.kind, &layer.x, &layer.y, &layer.width, &layer.height,
            &layer.columns, &layer.frames, &layer.period, &layer.driftX, &layer.driftY,
            &layer.opacity, &layer.delay, &layer.start, &layer.duration) >= 13) {
            layer.texture = LoadTexture(TextFormat("assets/menu/%s", path));
            if (!layer.texture.id || layer.width < 1 || layer.height < 1 || layer.columns < 1 ||
                layer.frames < 1 || layer.columns > layer.texture.width / layer.width ||
                (layer.frames-1) / layer.columns >= layer.texture.height / layer.height) {
                if (layer.texture.id) UnloadTexture(layer.texture);
                UnloadMenuScene(scene);
                UnloadFileText(manifest);
                return NULL;
            }
            SetTextureFilter(layer.texture, TEXTURE_FILTER_POINT);
            scene->layers[scene->count++] = layer;
        }
        while (*line && *line != '\n') line++;
        if (*line) line++;
    }
    UnloadFileText(manifest);
    if (!scene->count) { free(scene); return NULL; }
    LoadWind(scene);
    return scene;
}

void UnloadMenuScene(MenuScene *scene) {
    if (!scene) return;
    for (int i = 0; i < scene->count; i++) UnloadTexture(scene->layers[i].texture);
    free(scene);
}

static Rectangle Frame(const SceneLayer *layer, int index) {
    return (Rectangle){(index % layer->columns) * layer->width,
                       (index / layer->columns) * layer->height, layer->width, layer->height};
}

static float Noise(unsigned int n) {
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    return (n & 65535u) / 65536.0f;
}

static void DrawAir(const SceneLayer *layer, float time, float opacity) {
    bool stars = layer->kind == 's';
    int count = stars ? STAR_COUNT : PARTICLE_COUNT;
    int stride = stars ? 10 : 8;
    int types = layer->frames / stride;
    if (types < 1) return;
    for (int i = 0; i < count; i++) {
        unsigned int seed = 113u + 17u*i + (stars ? 103u : 0u);
        float phase = fmodf(time / (4.0f + 8.0f*Noise(seed)) + Noise(seed+1), 1.0f);
        float x = Noise(seed+2) * 1672, y = Noise(seed+3) * (stars ? 382 : 515);
        float scale = 0.38f + 0.5f * Noise(seed+4);
        if (!stars) {
            x = fmodf(x - phase * (60 + 70*Noise(seed+5)) + 1672, 1672);
            y += 395 - phase * (45 + 50*Noise(seed+6)) + 7*sinf(phase*6 + seed);
        } else if (x > 360 && x < 1200 && y < 275) continue;
        int type = i % types;
        int frame = type*stride + (int)(phase*stride);
        float life = stars ? 1.0f : sinf(phase*PI);
        DrawTexturePro(layer->texture, Frame(layer, frame),
                       (Rectangle){x,y,layer->width*scale,layer->height*scale},
                       (Vector2){0}, 0, Fade(WHITE, opacity*life));
    }
}

void DrawMenuScene(const MenuScene *scene, Rectangle canvas, Rectangle crop,
                   float time, float intro, float departure, bool reduced) {
    float scale = canvas.width / crop.width;
    float wind = WindAt(scene, time);
    float moon = 0.5f - 0.5f*cosf(time*2*PI/10.0f);
    BeginGameScissor((int)canvas.x, (int)canvas.y, (int)ceilf(canvas.width), (int)ceilf(canvas.height));
    // Local reference coordinates are shared by the artwork and the atlas metadata.
    Camera2D camera = {{canvas.x, canvas.y}, {crop.x, crop.y}, 0, scale};
    BeginMode2D(camera);
    for (int i = 0; i < scene->count; i++) {
        const SceneLayer *layer = &scene->layers[i];
        float opacity = layer->opacity * Ease((intro-layer->delay)*1.65f);
        if (opacity <= 0) continue;
        if (layer->kind == 's' || layer->kind == 'p') {
            if (!reduced) DrawAir(layer, time, opacity);
            continue;
        }
        float phase = layer->period > 0 ? fmodf(time/layer->period, 1.0f) : 0;
        int cell = (int)(phase*layer->frames);
        float dx = reduced ? 0 : layer->driftX * (.28f*wind + .72f*sinf(2*PI*phase));
        float dy = reduced ? 0 : layer->driftY * sinf(time*.53f);
        if (layer->kind == 'c') {
            // Cape atlases are consecutive segments on the same clock, with no repeated seam frame.
            float local = fmodf(time, layer->period) - layer->start;
            if (local < 0 || local >= layer->duration) continue;
            cell = (int)(local/layer->duration*layer->frames);
        } else if (layer->kind == 'v') {
            // A plant leans toward earlier wind samples while it gives back stored flexion.
            float arrival = time - layer->start;
            float inertia = layer->duration;
            float flex = .53f*WindAt(scene, arrival) + .31f*WindAt(scene, arrival-inertia) +
                         .16f*WindAt(scene, arrival-2*inertia);
            int levels = layer->frames / PLANT_FLUTTER_FRAMES;
            if (levels < 1) continue;
            int level = (int)roundf(fminf(1, fmaxf(0,flex))*(levels-1));
            cell = level*PLANT_FLUTTER_FRAMES + (int)(phase*PLANT_FLUTTER_FRAMES);
        } else if (layer->kind == 'h') {
            dx = layer->driftX*(.86f*sinf(2*PI*phase) - .26f*WindAt(scene,time-.9f));
            dy = layer->driftY*cosf(2*PI*phase);
        }
        if (cell > layer->frames-1) cell = layer->frames-1;
        Rectangle dest = {layer->x + dx, layer->y + dy, layer->width, layer->height};
        Vector2 pivot = {0};
        float angle = 0;
        switch (layer->kind) {
            case 'l': opacity *= .55f + .38f*moon + .07f*departure; break;
            case 't': opacity *= 1 - Ease(departure*1.7f); break;
            case 'g': {
                float shine = fmodf(time+4, 20.0f) / 1.8f;
                if (reduced || shine >= 1 || departure > 0) continue;
                cell = (int)(shine*layer->frames);
                break;
            }
            case 'a':
            case 'r':
                pivot = (Vector2){dest.width/2,dest.height/2};
                dest.x += pivot.x;
                dest.y += pivot.y;
                angle = (layer->kind == 'a' ? -1 : 1) * phase * 360;
                opacity = fminf(1, opacity*(.74f + .17f*moon + .25f*departure));
                break;
        }
        DrawTexturePro(layer->texture, Frame(layer, cell), dest, pivot, angle, Fade(WHITE, opacity));
    }
    EndMode2D();
    EndScissorMode();
}
