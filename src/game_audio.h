#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include "raylib.h"

typedef enum GameTrack { TRACK_MENU, TRACK_COMBAT, TRACK_MINIBOSS, TRACK_CHAPEL, TRACK_NECROMANCER, TRACK_COUNT, TRACK_SILENT } GameTrack;
typedef enum AudioChannel { AUDIO_MUSIC, AUDIO_EFFECTS, AUDIO_CHANNELS } AudioChannel;

typedef struct GameAudio {
    Music tracks[TRACK_COUNT];
    float gain[TRACK_COUNT];
    bool playing[TRACK_COUNT];
    Sound feedback, villageEmergence;
    int volume[AUDIO_CHANNELS];
    bool ownsDevice;
} GameAudio;

void InitGameAudio(GameAudio *audio);
void UpdateGameMusic(GameAudio *audio, GameTrack track, float strength, float dt);
void StopGameMusic(GameAudio *audio);
void PlayVillageEmergence(const GameAudio *audio, bool initialGroup);
void PlayAudioFeedback(const GameAudio *audio);
void SaveAudioSettings(const GameAudio *audio);
void UnloadGameAudio(GameAudio *audio);

#endif
