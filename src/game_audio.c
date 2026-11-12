#include "game_audio.h"
#include <math.h>
#include <stdio.h>

static const char *AudioSettingsPath(void) {
    return TextFormat("%s/audio_pif.cfg", GetApplicationDirectory());
}

static Sound LoadFeedback(void) {
    enum { RATE = 22050, SAMPLES = RATE*7/100 };
    short samples[SAMPLES];
    for (int i = 0; i < SAMPLES; i++) {
        float t = (float)i/RATE;
        float envelope = fminf(t*600.0f, 1.0f)*expf(-t*70.0f);
        samples[i] = (short)(5000.0f*envelope*(sinf(t*2600.0f) + .25f*sinf(t*4400.0f)));
    }
    Wave wave = {SAMPLES, RATE, 16, 1, samples};
    return LoadSoundFromWave(wave);
}

void InitGameAudio(GameAudio *audio) {
    *audio = (GameAudio){.volume = {75, 80}};
    const char *path = AudioSettingsPath();
    char *data = FileExists(path) ? LoadFileText(path) : NULL;
    if (data) {
        int music, effects;
        if (sscanf(data, "%d %d", &music, &effects) == 2 &&
            music >= 0 && music <= 100 && effects >= 0 && effects <= 100) {
            audio->volume[AUDIO_MUSIC] = music;
            audio->volume[AUDIO_EFFECTS] = effects;
        }
        UnloadFileText(data);
    }
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        audio->ownsDevice = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) return;

    static const char *paths[TRACK_COUNT] = {
        "assets/audio/menu.ogg", "assets/audio/combate.ogg", "assets/audio/miniboss.ogg"
    };
    for (int i = 0; i < TRACK_COUNT; i++) {
        if (!FileExists(paths[i])) continue;
        audio->tracks[i] = LoadMusicStream(paths[i]);
        audio->tracks[i].looping = true;
    }
    audio->feedback = LoadFeedback();
}

void UpdateGameMusic(GameAudio *audio, GameTrack track, float strength, float dt) {
    static const float mix[TRACK_COUNT] = {.66f, .66f, .61f};
    float step = fminf(1.0f, dt*2.5f);
    for (int i = 0; i < TRACK_COUNT; i++) {
        Music stream = audio->tracks[i];
        if (!stream.stream.buffer) continue;
        bool selected = i == (int)track;
        if (selected && !audio->playing[i]) {
            SetMusicVolume(stream, 0.0f);
            PlayMusicStream(stream);
            audio->playing[i] = true;
        }
        if (!audio->playing[i]) continue;
        float target = selected ? strength : 0.0f;
        audio->gain[i] += fmaxf(-step, fminf(step, target-audio->gain[i]));
        if (!selected && audio->gain[i] <= 0.0f) {
            StopMusicStream(stream);
            audio->playing[i] = false;
            continue;
        }
        SetMusicVolume(stream, mix[i]*audio->gain[i]*(audio->volume[AUDIO_MUSIC]/100.0f));
        UpdateMusicStream(stream);
    }
}

void PlayAudioFeedback(const GameAudio *audio) {
    if (!audio->feedback.stream.buffer) return;
    SetSoundVolume(audio->feedback, audio->volume[AUDIO_EFFECTS]/100.0f);
    PlaySound(audio->feedback);
}

void SaveAudioSettings(const GameAudio *audio) {
    char data[32];
    snprintf(data, sizeof(data), "%d %d\n", audio->volume[AUDIO_MUSIC], audio->volume[AUDIO_EFFECTS]);
    if (!SaveFileText(AudioSettingsPath(), data))
        TraceLog(LOG_WARNING, "Nao foi possivel salvar o volume.");
}

void UnloadGameAudio(GameAudio *audio) {
    for (int i = 0; i < TRACK_COUNT; i++)
        if (audio->tracks[i].stream.buffer) UnloadMusicStream(audio->tracks[i]);
    if (audio->feedback.stream.buffer) UnloadSound(audio->feedback);
    if (audio->ownsDevice) CloseAudioDevice();
    *audio = (GameAudio){0};
}
