#include "game_audio.h"
#include <math.h>
#include <stdio.h>

typedef struct SfxPreset {
    const char *file;
    float gain, cooldown;
    int limit, priority;
    float duration, frequency, sweep, noise, tone;
} SfxPreset;

static const SfxPreset presets[SFX_COUNT] = {
#define SFX(id, ...) [SFX_##id] = {__VA_ARGS__},
#include "sfx_presets.h"
#undef SFX
};
static GameAudio *activeAudio;

static const char *AudioSettingsPath(void) {
    return TextFormat("%s/audio_pif.cfg", GetApplicationDirectory());
}

static float RandomBipolar(unsigned *state) {
    *state = *state*1664525u + 1013904223u;
    return (float)(*state >> 16)/32767.5f-1.0f;
}

static Sound MakeSound(const SfxPreset *p, unsigned seed, int variant) {
    enum { RATE = 22050 };
    unsigned count = (unsigned)(p->duration*RATE);
    short *samples=MemAlloc(count*sizeof(*samples));
    if (!samples) return (Sound){0};
    float phase=0, low=0;
    float base = p->frequency*(.97f+.03f*variant);
    bool rising = p->sweep > 1.0f;
    for (unsigned i=0; i<count; i++) {
        float t=(float)i/count;
        phase += (2*3.14159265f/RATE)*base*(1+(p->sweep-1)*t);
        low += (.12f+.22f*p->noise)*(RandomBipolar(&seed)-low);
        float envelope = fminf(t*(rising ? 7 : 54),1.0f)*(1-t);
        envelope *= rising ? .34f+.66f*t : (1-t)*(1-t);
        float tone = sinf(phase) + .26f*p->tone*sinf(phase*2.01f) +
                     .18f*p->tone*sinf(phase*1.50f);
        samples[i]=(short)(11500*envelope*((1-p->noise)*tone+1.5f*p->noise*low));
    }
    Wave wave = {count, RATE, 16, 1, samples};
    Sound sound=LoadSoundFromWave(wave);
    MemFree(samples);
    return sound;
}

static void LimitMix(void *buffer, unsigned frames) {
    float *samples = buffer;
    for (unsigned i=0; i<frames; i++) {
        float peak = fmaxf(fabsf(samples[2*i]), fabsf(samples[2*i+1]));
        if (peak > .75f) {
            float excess=peak-.75f;
            float gain=(.75f+.23f*excess/(.23f+excess))/peak;
            samples[2*i] *= gain;
            samples[2*i+1] *= gain;
        }
    }
}

static void LoadSfx(GameAudio *audio) {
    static const char *extensions[] = {"ogg","mp3","wav"};
    for (int id=0; id<SFX_COUNT; id++) {
        audio->lastPlayed[id] = -1000;
        Sound override = {0};
        for (unsigned ext=0; ext<sizeof(extensions)/sizeof(extensions[0]); ext++) {
            const char *path = TextFormat("assets/audio/sfx/%s.%s", presets[id].file, extensions[ext]);
            if (FileExists(path)) override = LoadSound(path);
            if (override.stream.buffer) break;
        }
        for (int variant=0; variant<SFX_VARIANTS; variant++) {
            audio->voices[id][variant].sound = override.stream.buffer ?
                (variant == 0 ? override : LoadSoundAlias(override)) :
                MakeSound(&presets[id], 7819u + 541u*id + 1693u*variant, variant);
            audio->voices[id][variant].started = -1000;
            audio->voices[id][variant].alias = override.stream.buffer && variant > 0;
        }
    }
}

static void PlayCue(GameAudio *audio, GameSfx id, Vector2 position, bool positional,
                    float volume, float pitch, float duration) {
    if (!audio || !audio->unlocked || audio->muted || audio->suspended ||
        audio->volume[AUDIO_EFFECTS] <= 0 || audio->volume[AUDIO_MASTER] <= 0 ||
        (unsigned)id >= SFX_COUNT || !audio->voices[id][0].sound.stream.buffer) return;
    const SfxPreset *preset = &presets[id];
    double now = GetTime();
    if (now-audio->lastPlayed[id] < preset->cooldown) return;
    float pan=.5f;
    if (positional) {
        float dx=position.x-audio->listener.x, dy=position.y-audio->listener.y;
        float distance=sqrtf(dx*dx+dy*dy);
        volume *= 1.0f/(1.0f+distance*distance/(540.0f*540.0f));
        pan -= .38f*fmaxf(-1,fminf(1,dx/530));
    }
    if (volume < .04f) return;
    int count=0, same=0, weakestPriority=preset->priority;
    SfxVoice *evict=NULL;
    for (int i=0; i<SFX_COUNT; i++) {
        for (int j=0; j<SFX_VARIANTS; j++) {
            SfxVoice *voice = &audio->voices[i][j];
            if (voice->started < 0 || !IsSoundPlaying(voice->sound)) {
                voice->started=-1000;
                continue;
            }
            count++;
            same += i == (int)id;
            if (presets[i].priority < weakestPriority ||
                (evict && presets[i].priority == weakestPriority && voice->started < evict->started)) {
                evict=voice;
                weakestPriority=presets[i].priority;
            }
        }
    }
    if (same >= preset->limit || (count >= SFX_VOICE_BUDGET && !evict)) return;
    unsigned variant=audio->nextVariant[id];
    for (int i=0; i<SFX_VARIANTS; i++, variant=(variant+1)%SFX_VARIANTS) {
        SfxVoice *voice=&audio->voices[id][variant];
        if (!voice->sound.stream.buffer || voice->started >= 0) continue;
        if (count >= SFX_VOICE_BUDGET) {
            StopSound(evict->sound);
            evict->started=-1000;
        }
        float durationPitch=duration > 0 ? fmaxf(.5f,fminf(2.5f,preset->duration/duration)) : 1;
        SetSoundPitch(voice->sound, pitch*durationPitch*(1+.06f*RandomBipolar(&audio->random)));
        SetSoundPan(voice->sound, pan);
        SetSoundVolume(voice->sound, preset->gain*volume*audio->volume[AUDIO_EFFECTS]/100.0f*
                       (1+.08f*RandomBipolar(&audio->random)));
        PlaySound(voice->sound);
        voice->started=now;
        audio->lastPlayed[id]=now;
        audio->nextVariant[id]=(variant+1)%SFX_VARIANTS;
        audio->lastEvent=id;
        break;
    }
}

void SetGameAudioContext(GameAudio *audio, Vector2 listener, bool gesture, bool suspended, bool combatPaused) {
    audio->listener=listener;
    audio->unlocked |= gesture;
    if ((combatPaused && !audio->combatPaused) || (suspended && !audio->suspended)) {
        for (int id=0; id<SFX_UI_FOCUS; id++) {
            for (int variant=0; variant<SFX_VARIANTS; variant++) {
                SfxVoice *voice=&audio->voices[id][variant];
                if (voice->started >= 0 && voice->sound.stream.buffer) StopSound(voice->sound);
                voice->started=-1000;
            }
        }
        if (audio->villageEmergence.stream.buffer) StopSound(audio->villageEmergence);
    }
    audio->combatPaused=combatPaused;
    audio->suspended=suspended;
    if (IsAudioDeviceReady())
        SetMasterVolume(audio->muted || suspended || !audio->unlocked ? 0 : audio->volume[AUDIO_MASTER]/100.0f);
}

void PlaySfx(GameSfx id) {
    PlayCue(activeAudio,id,(Vector2){0},false,1,1,0);
}

void EmitSfx(GameSfx id, Vector2 position, float volume, float pitch) {
    PlayCue(activeAudio,id,position,true,volume,pitch,0);
}

void EmitSfxTimed(GameSfx id, Vector2 position, float duration) {
    PlayCue(activeAudio,id,position,true,1,1,duration);
}

void DrawAudioDebug(GameAudio *audio) {
    static int selected;
    static AudioChannel channel=AUDIO_EFFECTS;
    if (IsKeyPressed(KEY_UP)) selected=(selected+SFX_COUNT-1)%SFX_COUNT;
    if (IsKeyPressed(KEY_DOWN)) selected=(selected+1)%SFX_COUNT;
    if (IsKeyPressed(KEY_TAB)) channel=(channel+1)%AUDIO_CHANNELS;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
        int step=IsKeyPressed(KEY_LEFT) ? -5 : 5;
        audio->volume[channel]=(int)fmaxf(0,fminf(100,audio->volume[channel]+step));
        SaveAudioSettings(audio);
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) PlaySfx((GameSfx)selected);
    int playing=0;
    for (int i=0; i<SFX_COUNT; i++)
        for (int j=0; j<SFX_VARIANTS; j++)
            if (audio->voices[i][j].sound.stream.buffer && IsSoundPlaying(audio->voices[i][j].sound)) playing++;
    DrawText("F10  Fechar   |   Setas  Escolher / volume   |   Tab  Canal   |   Enter  Ouvir",20,22,18,RAYWHITE);
    const char *last=audio->lastEvent == SFX_COUNT ? "-" : presets[audio->lastEvent].file;
    static const char *channels[] = {"Musica","Efeitos","Geral"};
    DrawText(TextFormat("%s %d%%   |   Vozes %d/%d   |   Ultimo: %s",
             channels[channel],audio->volume[channel],playing,SFX_VOICE_BUDGET,last),20,54,18,LIGHTGRAY);
    int rows=(GetScreenHeight()-96)/23;
    int first=selected>=rows ? selected-rows+1 : 0;
    for (int i=first; i<SFX_COUNT && i<first+rows; i++) {
        Rectangle row={20,86+23.0f*(i-first),420,23};
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),row)) {
            selected=i;
            PlaySfx((GameSfx)i);
        }
        DrawText(presets[i].file,(int)row.x,(int)row.y,17,selected==i ? GOLD : RAYWHITE);
        DrawText(audio->voices[i][1].alias ? "Arquivo" : "Preset",440,(int)row.y,15,LIGHTGRAY);
    }
}

void InitGameAudio(GameAudio *audio) {
    *audio = (GameAudio){.volume = {75, 80, 100}, .random = 0x7f9a32u, .lastEvent = SFX_COUNT};
    const char *path = AudioSettingsPath();
    char *data = FileExists(path) ? LoadFileText(path) : NULL;
    if (data) {
        int music, effects, master=100, muted=0;
        if (sscanf(data, "%d %d %d %d", &music, &effects, &master, &muted) >= 2 &&
            music >= 0 && music <= 100 && effects >= 0 && effects <= 100) {
            audio->volume[AUDIO_MUSIC] = music;
            audio->volume[AUDIO_EFFECTS] = effects;
            if (master >= 0 && master <= 100) audio->volume[AUDIO_MASTER]=master;
            audio->muted=muted == 1;
        }
        UnloadFileText(data);
    }
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        audio->ownsDevice = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) return;

    activeAudio=audio;
    SetMasterVolume(0.0f);
    AttachAudioMixedProcessor(LimitMix);
    LoadSfx(audio);
    static const char *paths[TRACK_COUNT] = {
        [TRACK_MENU] = "assets/audio/menu.ogg",
        [TRACK_COMBAT] = "assets/audio/combate.ogg",
        [TRACK_MINIBOSS] = "assets/audio/miniboss.ogg",
        [TRACK_CHAPEL] = "assets/audio/capela.wav",
        [TRACK_NECROMANCER] = "assets/audio/necromante_rock.wav"
    };
    for (int i = 0; i < TRACK_COUNT; i++) {
        if (!FileExists(paths[i])) continue;
        audio->tracks[i] = LoadMusicStream(paths[i]);
        audio->tracks[i].looping = true;
    }
    const char *emergence="assets/cutscenes/village_aftermath/audio/emergencia.wav";
    if (FileExists(emergence)) audio->villageEmergence=LoadSound(emergence);
}

void UpdateGameMusic(GameAudio *audio, GameTrack track, float strength, float dt) {
    static const float mix[TRACK_COUNT] = {
        [TRACK_MENU]=.66f, [TRACK_COMBAT]=.66f, [TRACK_MINIBOSS]=.61f,
        [TRACK_CHAPEL]=.66f, [TRACK_NECROMANCER]=.64f
    };
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

void PlayVillageEmergence(const GameAudio *audio, bool initialGroup) {
    if (!audio->unlocked || audio->muted || audio->suspended) return;
    if (!audio->villageEmergence.stream.buffer) {
        PlaySfx(SFX_SUMMON);
        return;
    }
    SetSoundVolume(audio->villageEmergence,(initialGroup ? .58f : .26f)*audio->volume[AUDIO_EFFECTS]/100.0f);
    PlaySound(audio->villageEmergence);
}

void PlayAudioFeedback(GameAudio *audio) {
    PlayCue(audio,SFX_UI_CONFIRM,(Vector2){0},false,1,1,0);
}

void StopGameMusic(GameAudio *audio) {
    for (int i=0; i<TRACK_COUNT; i++) {
        if (audio->tracks[i].stream.buffer) StopMusicStream(audio->tracks[i]);
        audio->playing[i]=false;
        audio->gain[i]=0;
    }
}

void SaveAudioSettings(const GameAudio *audio) {
    char data[32];
    snprintf(data, sizeof(data), "%d %d %d %d\n", audio->volume[AUDIO_MUSIC],
             audio->volume[AUDIO_EFFECTS],audio->volume[AUDIO_MASTER],audio->muted);
    if (!SaveFileText(AudioSettingsPath(), data))
        TraceLog(LOG_WARNING, "Nao foi possivel salvar o volume.");
}

void UnloadGameAudio(GameAudio *audio) {
    for (int i = 0; i < TRACK_COUNT; i++)
        if (audio->tracks[i].stream.buffer) UnloadMusicStream(audio->tracks[i]);
    if (activeAudio == audio) {
        DetachAudioMixedProcessor(LimitMix);
        activeAudio=NULL;
    }
    for (int id=0; id<SFX_COUNT; id++) {
        for (int j=SFX_VARIANTS-1; j>=0; j--) {
            Sound sound=audio->voices[id][j].sound;
            if (!sound.stream.buffer) continue;
            if (audio->voices[id][j].alias) UnloadSoundAlias(sound);
            else UnloadSound(sound);
        }
    }
    if (audio->villageEmergence.stream.buffer) UnloadSound(audio->villageEmergence);
    if (audio->ownsDevice) CloseAudioDevice();
    *audio = (GameAudio){0};
}
