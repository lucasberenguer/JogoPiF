#ifndef SCENE_LIGHTING_H
#define SCENE_LIGHTING_H
#include "raylib.h"

enum { SCENE_LIGHT_LIMIT = 8, SCENE_CASTER_LIMIT = 56, SCENE_SHADOW_LIMIT = 7 };
typedef enum SceneCasterKind {
    SCENE_MAIN_ACTOR, SCENE_PROP, SCENE_MINION, SCENE_CASTER_KINDS
} SceneCasterKind;
typedef struct SceneLight {
    Vector2 position;
    float radius, intensity;
    Vector3 tint;
} SceneLight;

typedef struct SceneCaster {
    Vector2 feet;
    float halfWidth, reach;
    float contactOpacity;
    SceneCasterKind kind;
} SceneCaster;

typedef struct SceneLights {
    SceneLight lights[SCENE_LIGHT_LIMIT];
    int count;
    Vector2 ambient; /* dessaturação e redução de luminosidade */
    SceneCaster casters[SCENE_CASTER_LIMIT];
    int casterCount;
} SceneLights;

static inline void AddSceneLight(SceneLights *scene, Vector2 point, float radius,
                                 float intensity, Vector3 tint) {
    if (scene->count < SCENE_LIGHT_LIMIT && intensity > 0.0f && radius > 0.0f)
        scene->lights[scene->count++] = (SceneLight){point, radius, intensity, tint};
}

static inline void AddSceneCaster(SceneLights *scene, Vector2 feet, float halfWidth,
                                  float reach, float contactOpacity, SceneCasterKind kind) {
    if (scene->casterCount < SCENE_CASTER_LIMIT)
        scene->casters[scene->casterCount++] = (SceneCaster){feet, halfWidth, reach, contactOpacity, kind};
}
#endif
