#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include "raylib.h"

typedef enum GameTrack { TRACK_MENU, TRACK_COMBAT, TRACK_MINIBOSS, TRACK_CHAPEL, TRACK_NECROMANCER, TRACK_COUNT, TRACK_SILENT } GameTrack;
typedef enum AudioChannel { AUDIO_MUSIC, AUDIO_EFFECTS, AUDIO_MASTER, AUDIO_CHANNELS } AudioChannel;
typedef enum GameSfx {
#define SFX(id, ...) SFX_##id,
#include "sfx_presets.h"
#undef SFX
    SFX_COUNT
} GameSfx;

enum { SFX_VARIANTS = 3, SFX_VOICE_BUDGET = 14 };

typedef struct SfxVoice {
    Sound sound;
    double started;
    bool alias;
} SfxVoice;

typedef struct GameAudio {
    Music tracks[TRACK_COUNT];
    float gain[TRACK_COUNT];
    bool playing[TRACK_COUNT];
    Sound villageEmergence;
    SfxVoice voices[SFX_COUNT][SFX_VARIANTS];
    double lastPlayed[SFX_COUNT];
    unsigned nextVariant[SFX_COUNT], random;
    GameSfx lastEvent;
    Vector2 listener;
    int volume[AUDIO_CHANNELS];
    bool ownsDevice, muted, unlocked, suspended, combatPaused;
} GameAudio;

void InitGameAudio(GameAudio *audio);
void UpdateGameMusic(GameAudio *audio, GameTrack track, float strength, float dt);
void StopGameMusic(GameAudio *audio);
void PlayVillageEmergence(const GameAudio *audio, bool initialGroup);
void PlayAudioFeedback(GameAudio *audio);
void SetGameAudioContext(GameAudio *audio, Vector2 listener, bool gesture, bool suspended, bool combatPaused);
void PlaySfx(GameSfx id);
void EmitSfx(GameSfx id, Vector2 position, float volume, float pitch);
void EmitSfxTimed(GameSfx id, Vector2 position, float duration);
void DrawAudioDebug(GameAudio *audio);
void SaveAudioSettings(const GameAudio *audio);
void UnloadGameAudio(GameAudio *audio);

#endif
