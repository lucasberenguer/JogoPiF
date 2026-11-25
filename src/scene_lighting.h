#ifndef SCENE_LIGHTING_H
#define SCENE_LIGHTING_H
#include "raylib.h"

enum { SCENE_LIGHT_LIMIT = 8, SCENE_CASTER_LIMIT = 7 };
typedef struct SceneLight {
    Vector2 position;
    float radius, intensity;
    Vector3 tint;
} SceneLight;

typedef struct SceneCaster {
    Vector2 feet;
    float halfWidth, reach;
    float contactOpacity;
} SceneCaster;

typedef struct SceneLights {
    SceneLight lights[SCENE_LIGHT_LIMIT];
    int count;
    Vector2 ambient; /* dessaturação e redução de luminosidade */
    SceneCaster casters[SCENE_CASTER_LIMIT];
    int casterCount;
    int shadowLightSlot; /* índice da luz + 1. Zero indica ausência de sombra projetada. */
} SceneLights;

static inline void AddSceneLight(SceneLights *scene, Vector2 point, float radius,
                                 float intensity, Vector3 tint) {
    if (scene->count < SCENE_LIGHT_LIMIT && intensity > 0.0f && radius > 0.0f)
        scene->lights[scene->count++] = (SceneLight){point, radius, intensity, tint};
}

static inline void AddSceneCaster(SceneLights *scene, Vector2 feet, float halfWidth,
                                  float reach, float contactOpacity) {
    if (scene->casterCount < SCENE_CASTER_LIMIT)
        scene->casters[scene->casterCount++] = (SceneCaster){feet, halfWidth, reach, contactOpacity};
}
#endif
