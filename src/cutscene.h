#ifndef CUTSCENE_H
#define CUTSCENE_H

#include "raylib.h"

enum { INTRO_ART_COUNT = 8, INTRO_BEAT_COUNT = 6 };

typedef struct OpeningCutscene {
    Texture2D art[INTRO_ART_COUNT];
    Sound cues[INTRO_BEAT_COUNT];
    Music music;
    float elapsed, age, finalBeatAtAdvance;
    int beat;
    bool finished, ownsAudio;
} OpeningCutscene;

bool InitOpeningCutscene(OpeningCutscene *scene);
void UpdateOpeningCutscene(OpeningCutscene *scene, float dt);
void DrawOpeningCutscene(const OpeningCutscene *scene);
void UnloadOpeningCutscene(OpeningCutscene *scene);

#endif
