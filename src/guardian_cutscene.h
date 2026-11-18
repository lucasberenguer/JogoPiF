#ifndef GUARDIAN_CUTSCENE_H
#define GUARDIAN_CUTSCENE_H

#include "raylib.h"

enum { GUARDIAN_SCENE_LIMIT = 32, GUARDIAN_ART_LIMIT = 16 };
typedef enum CinematicTransition { CINEMATIC_CUT, CINEMATIC_FADE, CINEMATIC_CROSSFADE,
                                   CINEMATIC_WHITE } CinematicTransition;
enum { CINEMATIC_LEAVES = 1, CINEMATIC_FOG = 2, CINEMATIC_SHAKE = 4,
       CINEMATIC_SEAL = 8, CINEMATIC_CORRUPTION = 16, CINEMATIC_DUST = 32 };

typedef struct GuardianScene {
    char id[64], image[96], speaker[48], dialogue[512];
    char audioEvent[64], music[160], title[128], subtitle[160];
    float duration, fadeIn, fadeOut, cueAt, cueGain;
    Vector2 focusStart, focusEnd, sealPosition;
    float zoomStart, zoomEnd;
    CinematicTransition transitionIn, transitionOut;
    unsigned int effects;
    int artIndex;
    bool waitsForInput;
    Sound cue;
} GuardianScene;

typedef struct GuardianCutscene {
    GuardianScene scenes[GUARDIAN_SCENE_LIMIT];
    Texture2D art[GUARDIAN_ART_LIMIT];
    int count, artCount, current;
    float elapsed, letters, musicVolume, effectsVolume, musicGain;
    Music music;
    char musicPath[160];
    bool paused, finished, cueStarted, cuePaused;
} GuardianCutscene;

typedef struct GuardianCutsceneInput { bool advance, skip, pause; } GuardianCutsceneInput;

bool InitGuardianCutscene(GuardianCutscene *scene, float musicVolume, float effectsVolume);
void UpdateGuardianCutscene(GuardianCutscene *scene, float dt, GuardianCutsceneInput input);
GuardianCutsceneInput ReadGuardianCutsceneInput(void);
void SetGuardianCutscenePaused(GuardianCutscene *scene, bool paused);
void DrawGuardianCutscene(const GuardianCutscene *scene);
void UnloadGuardianCutscene(GuardianCutscene *scene);

#endif
