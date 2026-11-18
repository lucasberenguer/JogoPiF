#ifndef GUARDIAN_CUTSCENE_H
#define GUARDIAN_CUTSCENE_H

#include "raylib.h"

enum { GUARDIAN_SCENE_LIMIT = 32, GUARDIAN_ART_LIMIT = 16 };
typedef enum CinematicTransition { CINEMATIC_CUT, CINEMATIC_FADE, CINEMATIC_CROSSFADE,
                                   CINEMATIC_WHITE } CinematicTransition;
typedef enum GuardianEventType { GUARDIAN_CINEMATIC, GUARDIAN_NARRATION, GUARDIAN_DIALOGUE,
                                 GUARDIAN_EFFECT, GUARDIAN_TRANSITION } GuardianEventType;
enum { CINEMATIC_LEAVES = 1, CINEMATIC_FOG = 2, CINEMATIC_SHAKE = 4,
       CINEMATIC_SEAL = 8, CINEMATIC_CORRUPTION = 16, CINEMATIC_DUST = 32,
       CINEMATIC_PURIFY = 64, CINEMATIC_CYAN = 128, CINEMATIC_GOLD = 256,
       CINEMATIC_WISP = 512 };

typedef struct GuardianScene {
    char id[64], image[96], speaker[48], dialogue[512];
    char narration[512];
    GuardianEventType type;
    float narrationAt, narrationDuration, narrationFade;
    char audioEvent[64], music[160], title[128], subtitle[160];
    char chapter[64], tagline[160];
    float musicStart;
    float duration, fadeIn, fadeOut, cueAt, cueGain;
    Vector2 focusStart, focusEnd, sealPosition;
    float zoomStart, zoomEnd;
    CinematicTransition transitionIn, transitionOut;
    unsigned int effects;
    int artIndex;
    bool waitsForInput;
    bool chapterCard;
    Sound cue;
} GuardianScene;

typedef struct GuardianCutscene {
    GuardianScene scenes[GUARDIAN_SCENE_LIMIT];
    Texture2D art[GUARDIAN_ART_LIMIT];
    int count, artCount, current;
    float elapsed, letters, musicVolume, effectsVolume, musicGain, exitElapsed;
    Music music;
    char musicPath[160];
    char assetDirectory[256];
    bool paused, finished, skipped, exiting, cueStarted, cuePaused;
} GuardianCutscene;

typedef struct GuardianCutsceneInput { bool advance, skip, pause; } GuardianCutsceneInput;

/* Shared by rendering and the layout audit. Text is never truncated to two lines. */
int LayoutGuardianNarration(const char *text, float scale, float width,
                           char lines[8][512], float *fontSize);
float GuardianNarrationOpacity(const GuardianScene *shot, float elapsed);

bool InitGuardianCutscene(GuardianCutscene *scene, float musicVolume, float effectsVolume);
bool InitGuardianAftermath(GuardianCutscene *scene, bool passage, float musicVolume, float effectsVolume);
void UpdateGuardianCutscene(GuardianCutscene *scene, float dt, GuardianCutsceneInput input);
GuardianCutsceneInput ReadGuardianCutsceneInput(void);
void SetGuardianCutscenePaused(GuardianCutscene *scene, bool paused);
void DrawGuardianCutscene(const GuardianCutscene *scene);
void UnloadGuardianCutscene(GuardianCutscene *scene);

#endif
