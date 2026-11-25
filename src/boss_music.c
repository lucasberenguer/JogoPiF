#include "boss_music.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

enum { RATE=24000, STEPS=16, BARS=8, WAV_HEADER=44 };
static const float tau=6.28318530718f;

typedef enum ColorVoice { LEAD, BELL, STRINGS, ORGAN } ColorVoice;
typedef enum Drum { KICK, SNARE, HAT, CRASH, TOM } Drum;
typedef struct Theme {
    const char *name, *file;
    int bpm, key;
    int riff[STEPS], refrain[STEPS], roots[BARS], melody[BARS];
    unsigned kick, snare, hats, tom;
    float drive, sustain, accompaniment;
    ColorVoice color;
} Theme;

/* Each riff entry is a semitone from the current root. -1 leaves room for the drums. */
static const Theme themes[] = {
    {"Guardião da Clareira", "guardiao", 116, 40,
      {0,-1,0,0,3,-1,5,-1,0,-1,0,7,-1,5,3,-1}, {0,-1,7,-1,10,-1,9,-1,7,-1,5,-1,3,-1,5,-1},
      {0,0,3,5,0,5,3,0}, {12,15,19,21,19,24,22,15}, 0x1119,0x1010,0x5555,0x2200,1.8f,.52f,.66f,LEAD},
    {"Coveiro das Raízes", "coveiro", 78, 37,
      {0,-1,-1,-1,0,-1,-1,1,0,-1,-1,-1,6,-1,1,-1}, {0,-1,-1,-1,3,-1,-1,-1,1,-1,-1,0,6,-1,-1,-1},
      {0,0,1,0,0,3,1,0}, {12,13,18,13,12,15,18,11}, 0x0181,0x1000,0x1111,0x8820,2.7f,.95f,.57f,BELL},
    {"Líder da Vila", "lider_vila", 94, 45,
      {0,-1,7,-1,12,-1,7,-1,0,-1,7,10,-1,7,-1,3}, {0,-1,3,-1,7,-1,10,-1,0,-1,12,-1,7,3,-1,2},
      {0,-4,3,-2,0,-4,-2,0}, {15,12,19,14,15,12,10,7}, 0x1101,0x9450,0x5555,0x0200,1.15f,.76f,.72f,STRINGS},
    {"Rasga-Ossos", "rasga_ossos", 154, 38,
      {0,0,0,-1,1,0,0,0,6,0,0,0,3,1,0,-1}, {0,0,10,0,8,0,6,0,5,0,3,0,1,-1,0,0},
      {0,1,0,3,0,1,3,0}, {12,18,13,19,12,16,18,13}, 0x77dd,0x1010,0xffff,0x0a00,3.1f,.34f,.27f,LEAD},
    {"Igris, o Comandante", "igris", 136, 35,
      {0,-1,0,0,5,-1,0,0,8,-1,0,0,7,-1,6,0}, {0,0,0,-1,7,-1,7,7,8,-1,8,8,11,-1,7,0},
      {0,3,5,7,0,8,5,7}, {19,23,24,26,19,27,24,23}, 0x6d55,0x1010,0x5555,0x4400,2.35f,.57f,.64f,ORGAN},
    {"Necromante", "necromante", 144, 39,
      {0,0,-1,0,6,-1,0,0,1,0,-1,0,8,0,7,-1}, {0,-1,0,6,0,0,-1,6,1,-1,1,0,11,0,6,0},
      {0,0,-1,1,0,6,-1,0}, {12,18,23,16,24,18,25,11}, 0xedb5,0x1110,0xffff,0x2200,3.55f,.67f,.78f,ORGAN}
};

static const Theme *ThemeForTrack(GameTrack track) {
    if (track<TRACK_GUARDIAN || track>TRACK_NECROMANCER) return NULL;
    return &themes[track-TRACK_GUARDIAN];
}

const char *BossThemeName(GameTrack track) {
    const Theme *theme=ThemeForTrack(track);
    return theme ? theme->name : "";
}

typedef struct Render {
    float *pcm;
    int frames, at;
    float left, right;
    uint32_t random;
} Render;

static float Noise(Render *r) {
    r->random=r->random*1664525u+1013904223u;
    return (float)(r->random>>16)/32767.5f-1;
}

static void Mix(Render *r, int at, float signal) {
    if (at>=r->frames) at-=r->frames;
    r->pcm[2*at]+=signal*r->left;
    r->pcm[2*at+1]+=signal*r->right;
}

static void Pan(Render *r, float pan) {
    r->left=1-pan;
    r->right=pan;
}

static float Frequency(int midi) { return 440.0f*exp2f((midi-69)/12.0f); }

static void Guitar(Render *r, int note, int length, float drive, float gain, float tuning) {
    float string[1024];
    float hz=Frequency(note)*tuning;
    int delay=(int)(RATE/hz-.5f);
    if (delay<2 || delay>=1024) return;
    float fraction=RATE/hz-.5f-delay;
    for (int i=0; i<=delay; i++)
        string[i]=.40f*Noise(r)+.6f*sinf(tau*i/delay)+.20f*sinf(2*tau*i/delay);
    int tap=0;
    float dc=0, cabinet=0, previous=0;
    for (int i=0; i<length; i++) {
        int next=tap==delay ? 0 : tap+1;
        float wave=string[tap];
        string[tap]=.997f*(.50f*wave+.50f*previous)*(1-fraction)+.997f*previous*fraction;
        previous=wave;
        tap=next;
        float clipped=wave*drive/(1+.85f*fabsf(wave*drive));
        dc+=.018f*(clipped-dc);
        cabinet+=.36f*(clipped-dc-cabinet);
        float envelope=fminf(1,i/72.0f)*fminf(1,(length-i)/360.0f);
        Mix(r,r->at+i,gain*envelope*cabinet);
    }
}

static void Tone(Render *r, float hz, int length, float gain, ColorVoice voice) {
    float phase=0;
    for (int i=0; i<length; i++) {
        float t=(float)i/RATE, u=(float)i/length;
        phase+=tau*hz/RATE;
        float signal, attack=fminf(1,t*95), release=fminf(1,(1-u)*8);
        switch (voice) {
            case BELL:
                signal=(sinf(phase)+.52f*sinf(phase*2.61f)*(1-u)+.24f*sinf(phase*4.17f))*(1-u)*(1-u);
                break;
            case STRINGS:
                attack=fminf(1,t*7);
                signal=.62f*sinf(phase)+.24f*sinf(phase*2.005f)+.28f*sinf(phase*.996f);
                break;
            case ORGAN:
                attack=fminf(1,t*26);
                signal=.64f*sinf(phase)+.31f*sinf(phase*2)+.18f*sinf(phase*3)+.11f*sinf(phase*4.007f);
                break;
            default:
                signal=.73f*sinf(phase+.10f*sinf(tau*5*t))+.30f*sinf(phase*2.005f)+.21f*sinf(phase*3);
                break;
        }
        Mix(r,r->at+i,gain*attack*release*signal);
    }
}

static void Hit(Render *r, Drum drum, float gain) {
    static const float lengths[]={.18f,.19f,.074f,.48f,.24f};
    int length=(int)(RATE*lengths[drum]);
    float low=0, phase=0;
    for (int i=0; i<length; i++) {
        float t=(float)i/RATE, u=(float)i/length;
        float white=Noise(r);
        low+=.23f*(white-low);
        float f=1-u;
        float body=0, grit=0;
        if (drum==KICK || drum==TOM) {
            phase+=tau*((drum==KICK ? 49.0f : 91.0f)+119.0f*f*f*f*f)/RATE;
            body=sinf(phase)*f*f*.88f;
            grit=.22f*(white-low)*f*f*f*f;
        } else if (drum==SNARE) {
            body=.35f*sinf(tau*173*t)*f*f*f;
            grit=.95f*(white-.30f*low)*f*f;
        } else {
            grit=(white-low)*f*f*(drum==CRASH ? .80f : .62f);
        }
        Mix(r,r->at+i,gain*fminf(1,i/12.0f)*(body+grit));
    }
}

static void Arrange(const Theme *theme, Render *r) {
    double step=RATE*60.0/(theme->bpm*4.0);
    for (int bar=0; bar<BARS; bar++) {
        bool refrain=bar>=4;
        int root=theme->key+theme->roots[bar];
        for (int beat=0; beat<STEPS; beat++) {
            int at=(int)((bar*STEPS+beat)*step+.5);
            r->at=at;
            unsigned bit=1u<<beat;
            Pan(r,.50f);
            if (theme->kick&bit) Hit(r,KICK,beat%4==0 ? .72f : .52f);
            if (theme->snare&bit) Hit(r,SNARE,beat%4==0 ? .57f : .38f);
            Pan(r,.61f);
            if (theme->hats&bit) Hit(r,HAT,beat%4==0 ? .26f : .17f);
            if (beat==0 && (bar==0 || bar==4)) Hit(r,CRASH,.28f);
            Pan(r,.35f);
            if ((theme->tom&bit) && (bar%2 || refrain)) Hit(r,TOM,.44f);

            const int *riff=refrain ? theme->refrain : theme->riff;
            if (riff[beat]>=0) {
                int note=root+riff[beat];
                bool chord=beat%4==0 || (refrain && beat==12);
                int holds=1;
                while (beat+holds<STEPS && riff[beat+holds]<0) holds++;
                int length=(int)(step*(theme->sustain+.72f*(holds-1)));
                Pan(r,.19f);
                Guitar(r,note,length,theme->drive,chord ? .49f : .44f,1.002f);
                if (chord) Guitar(r,note+7,length,theme->drive,.23f,1);
                r->at=at+147;
                Pan(r,.81f);
                Guitar(r,note,length,theme->drive,.43f,.997f);
                if (chord) Guitar(r,note+7,length,theme->drive,.21f,1.001f);
                r->at=at;
                Pan(r,.50f);
                Tone(r,Frequency(note-12),length,.20f,LEAD);
            }
            if (beat==0 || (refrain && beat==10)) {
                int lead=theme->key+theme->melody[bar]-(beat==10 ? 2 : 0);
                int length=(int)(step*(beat==0 && theme->color!=BELL ? 11 : 7));
                Pan(r,.57f);
                Tone(r,Frequency(lead),length,.32f*theme->accompaniment,theme->color);
                Pan(r,.40f);
                Tone(r,Frequency(lead-(theme->color==ORGAN ? 12 : 7)),length,
                     .19f*theme->accompaniment,theme->color);
            }
        }
    }
}

static void WriteLE(unsigned char *where, uint32_t value, int bytes) {
    for (int i=0; i<bytes; i++) where[i]=(unsigned char)(value>>(8*i));
}

static unsigned char *RenderTheme(const Theme *theme, int *size) {
    int frames=(int)(RATE*60.0*BARS*4/theme->bpm+.5);
    uint32_t bytes=4u*frames;
    float *samples=MemAlloc((unsigned)frames*2*sizeof(*samples));
    unsigned char *wav=MemAlloc(WAV_HEADER+bytes);
    if (!samples || !wav) {
        MemFree(samples);
        MemFree(wav);
        return NULL;
    }
    memset(samples,0,(size_t)frames*2*sizeof(*samples));
    Render render={.pcm=samples,.frames=frames,.random=25173u+(unsigned)theme->bpm};
    Arrange(theme,&render);
    memcpy(wav,"RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\0\0\0\0\0\0\0\0\x04\0\x10\0data\0\0\0\0",WAV_HEADER);
    WriteLE(wav+4,bytes+36,4);
    WriteLE(wav+24,RATE,4);
    WriteLE(wav+28,4*RATE,4);
    WriteLE(wav+40,bytes,4);
    for (int i=0; i<2*frames; i++) {
        float signal=1.75f*samples[i];
        int16_t pcm=(int16_t)(30000.0f*signal/(1.0f+fabsf(signal)));
        WriteLE(wav+WAV_HEADER+2*i,(uint16_t)pcm,2);
    }
    MemFree(samples);
    *size=(int)(WAV_HEADER+bytes);
    return wav;
}

Music LoadBossTheme(GameTrack track, unsigned char **data) {
    const Theme *theme=ThemeForTrack(track);
    if (!theme) return (Music){0};
    const char *file=TextFormat("assets/audio/music/boss_%s.ogg",theme->file);
    Music music=FileExists(file) ? LoadMusicStream(file) : (Music){0};
    if (music.stream.buffer) return music;
    int size=0;
    *data=RenderTheme(theme,&size);
    return *data ? LoadMusicStreamFromMemory(".wav",*data,size) : (Music){0};
}
