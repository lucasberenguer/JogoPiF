#include <assert.h>
#include <stdio.h>

/* Check internal voice and limiter state without expanding the gameplay API. */
#include "../src/game_audio.c"

static int ActiveVoices(const GameAudio *audio) {
    int count=0;
    for (int id=0; id<SFX_COUNT; id++)
        for (int i=0; i<SFX_VARIANTS; i++)
            if (audio->voices[id][i].started >= 0) count++;
    return count;
}

static void VerifyBossMusic(GameAudio *audio) {
    assert(BossTrackForMap(CLEARING_MAP)==TRACK_GUARDIAN);
    assert(BossTrackForMap(CEMETERY_MAP)==TRACK_COVEIRO);
    assert(BossTrackForMap(VILLAGE_MAP)==TRACK_VILLAGE_LEADER);
    assert(BossTrackForMap(BONELAND_MAP)==TRACK_BONE_LORD);
    assert(BossTrackForMap(TEMPLAR_FIELD_MAP)==TRACK_IGRIS);
    assert(BossTrackForMap(NECROMANCER_MAP)==TRACK_NECROMANCER);
    assert(BossTrackForMap(CHAPEL_MAP)==TRACK_SILENT);
    assert(BossTrackForMap(INVERTED_CHAPEL_MAP)==TRACK_SILENT);
    for (int i=TRACK_GUARDIAN; i<=TRACK_NECROMANCER; i++) {
        Music theme=audio->tracks[i];
        assert(theme.stream.buffer && theme.looping && audio->musicData[i]);
        assert(GetMusicTimeLength(theme)>12);
        for (int j=TRACK_GUARDIAN; j<i; j++)
            assert(audio->tracks[j].frameCount!=theme.frameCount);
        size_t bytes=(size_t)theme.frameCount*4;
        int peak=0;
        for (size_t at=0; at<bytes; at+=2) {
            const unsigned char *pcm=audio->musicData[i]+44+at;
            int sample=pcm[0]+256*pcm[1];
            if (sample>=32768) sample-=65536;
            if (sample<0) sample=-sample;
            if (sample>peak) peak=sample;
        }
        assert(peak>5000 && peak<30000);
    }
    UpdateBossMusic(audio,TRACK_GUARDIAN,1,1,.4f);
    assert(audio->playing[TRACK_GUARDIAN] && audio->gain[TRACK_GUARDIAN]==1);
    UpdateBossMusic(audio,TRACK_COVEIRO,1,1,.12f);
    assert(audio->playing[TRACK_GUARDIAN] && audio->playing[TRACK_COVEIRO]);
    assert(audio->gain[TRACK_COVEIRO]>0 && audio->gain[TRACK_GUARDIAN]<1);
    UpdateBossMusic(audio,TRACK_COVEIRO,2,1,.44f);
    assert(!audio->playing[TRACK_GUARDIAN] && audio->musicPitch[TRACK_COVEIRO]>1.03f);
    UpdateBossMusic(audio,TRACK_COVEIRO,2,.42f,.4f);
    assert(audio->gain[TRACK_COVEIRO]<.5f);
    SetGameAudioContext(audio,(Vector2){0},false,true,true);
    assert(!IsMusicStreamPlaying(audio->tracks[TRACK_COVEIRO]));
    SetGameAudioContext(audio,(Vector2){0},false,false,false);
    assert(IsMusicStreamPlaying(audio->tracks[TRACK_COVEIRO]));
    UpdateBossMusic(audio,TRACK_SILENT,0,0,.30f);
    assert(!GameMusicPlaying(audio));
    UpdateBossMusic(audio,TRACK_NECROMANCER,3,1,.4f);
    assert(audio->playing[TRACK_NECROMANCER] && audio->musicPitch[TRACK_NECROMANCER]>1.04f);
    StopGameMusic(audio);
    assert(!GameMusicPlaying(audio));
    puts("Boss music: six different loops, phase intensity, fade, pause and resume verified.");
}

int main(void) {
    unsigned seed=25;
    float min=1, max=-1;
    for (int i=0; i<10000; i++) {
        float sample=RandomBipolar(&seed);
        min=fminf(min,sample);
        max=fmaxf(max,sample);
        assert(1+.06f*sample>=.94f && 1+.06f*sample<=1.06f);
        assert(1+.08f*sample>=.92f && 1+.08f*sample<=1.08f);
    }
    assert(min<-.99f && max>.99f);
    float mix[] = {3,-1.5f,.2f,-.1f,-17,2};
    LimitMix(mix,3);
    for (int i=0; i<6; i++) assert(fabsf(mix[i])<.98f);
    assert(mix[2]==.2f && mix[3]==-.1f);
    assert(fabsf(mix[0]+2*mix[1])<.0001f);

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64,64,"Audio test");
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        puts("Audio device unavailable. Variation and limiter verified.");
        CloseWindow();
        return 77;
    }
    const char *override="assets/audio/sfx/menu_confirmar.wav";
    short samples[1000]={0};
    assert(ExportWave((Wave){1000,22050,16,1,samples},override));
    GameAudio audio;
    InitGameAudio(&audio);
    audio.muted=false;
    audio.volume[AUDIO_EFFECTS]=80;
    audio.volume[AUDIO_MASTER]=100;
    assert(audio.voices[SFX_UI_CONFIRM][1].alias);
    for (int id=0; id<SFX_COUNT; id++) {
        for (int i=0; i<SFX_VARIANTS; i++) assert(audio.voices[id][i].sound.stream.buffer);
        if (id!=SFX_UI_CONFIRM) assert(!audio.voices[id][1].alias);
    }
    PlaySfx(SFX_KATANA);
    assert(audio.lastEvent==SFX_COUNT);
    SetGameAudioContext(&audio,(Vector2){0},true,false,false);
    PlaySfx(SFX_KATANA);
    assert(audio.lastEvent==SFX_KATANA && audio.nextVariant[SFX_KATANA]==1);
    PlaySfx(SFX_KATANA);
    assert(audio.nextVariant[SFX_KATANA]==1);
    audio.lastPlayed[SFX_KATANA]=-1000;
    PlaySfx(SFX_KATANA);
    assert(audio.nextVariant[SFX_KATANA]==2);
    audio.lastPlayed[SFX_KATANA]=-1000;
    PlaySfx(SFX_KATANA);
    assert(audio.nextVariant[SFX_KATANA]==2);
    SetGameAudioContext(&audio,(Vector2){0},false,false,true);
    assert(ActiveVoices(&audio)==0);
    SetGameAudioContext(&audio,(Vector2){0},false,true,false);
    PlaySfx(SFX_BEAM);
    assert(audio.lastEvent==SFX_KATANA);
    SetGameAudioContext(&audio,(Vector2){0},false,false,false);
    EmitSfx(SFX_BEAM,(Vector2){10000,10000},1,1);
    assert(audio.lastEvent==SFX_KATANA);
    for (int id=0; id<SFX_COUNT; id++) {
        for (int i=0; i<SFX_VARIANTS; i++) {
            audio.lastPlayed[id]=-1000;
            PlaySfx((GameSfx)id);
            assert(ActiveVoices(&audio)<=SFX_VOICE_BUDGET);
        }
    }
    SetGameAudioContext(&audio,(Vector2){0},false,false,true);
    assert(ActiveVoices(&audio)<=2);
    SetGameAudioContext(&audio,(Vector2){0},false,false,false);
    VerifyBossMusic(&audio);
    UnloadGameAudio(&audio);
    assert(remove(override)==0);
    CloseAudioDevice();
    CloseWindow();
    puts("Audio: overrides, fallback, unlock, cooldown, variants, voice limits, pause, attenuation and limiter verified.");
}
