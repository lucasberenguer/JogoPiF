#include "boss_music.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

enum { RATE=24000, STEPS=16, BARS=8, WAV_HEADER=44 };
static const float tau=6.28318530718f;

typedef enum ColorVoice { LEAD, BELL, STRINGS, ORGAN, PIANO, BOWED, HARPSICHORD, BRASS } ColorVoice;
typedef enum Drum { KICK, SNARE, HAT, CRASH, TOM, WAR_DRUM, IRON } Drum;
typedef enum Arrangement {
    CLASSIC, LAMENT, HORDE_ROCK, WAR_METAL, CELESTIAL_METAL, KNIGHT_DUEL, FALLEN_ANGEL
} Arrangement;
typedef struct Theme {
    const char *name, *file;
    int bpm, key;
    int riff[STEPS], refrain[STEPS], roots[BARS], melody[BARS];
    unsigned kick, snare, hats, tom;
    float drive, sustain, accompaniment;
    ColorVoice color;
    Arrangement arrangement;
} Theme;

/* Each riff entry is a semitone from the current root. -1 leaves room for the drums. */
static const Theme themes[] = {
    {"Guardião da Clareira", "guardiao", 116, 40,
      {0,-1,0,0,3,-1,5,-1,0,-1,0,7,-1,5,3,-1}, {0,-1,7,-1,10,-1,9,-1,7,-1,5,-1,3,-1,5,-1},
      {0,0,3,5,0,5,3,0}, {12,15,19,21,19,24,22,15}, 0x1119,0x1010,0x5555,0x2200,1.8f,.52f,.66f,LEAD,CLASSIC},
    {"Coveiro das Raízes", "coveiro", 78, 37,
      {0,-1,-1,-1,0,-1,-1,1,0,-1,-1,-1,6,-1,1,-1}, {0,-1,-1,-1,3,-1,-1,-1,1,-1,-1,0,6,-1,-1,-1},
      {0,0,1,0,0,3,1,0}, {12,13,18,13,12,15,18,11}, 0x0181,0x1000,0x1111,0x8820,2.7f,.95f,.57f,BELL,CLASSIC},
    {"Líder da Vila", "lider_vila", 68, 45,
      {0,-1,-1,-1,7,-1,-1,-1,12,-1,-1,-1,7,-1,-1,-1}, {0,-1,-1,-1,7,-1,12,-1,-1,-1,7,-1,14,-1,-1,-1},
      {0,-4,3,-2,0,-4,-5,-5}, {24,22,19,17,15,14,12,11}, 0,0,0,0,0,0,0,PIANO,LAMENT},
    {"Rasga-Ossos", "rasga_ossos", 154, 38,
      {0,0,0,-1,1,0,0,0,6,0,0,0,3,1,0,-1}, {0,0,10,0,8,0,6,0,5,0,3,0,1,-1,0,0},
      {0,1,0,3,0,1,3,0}, {12,18,13,19,12,16,18,13}, 0x77dd,0x1010,0xffff,0x0a00,3.1f,.34f,.27f,LEAD,CLASSIC},
    {"Igris - faixa anterior", "igris", 166, 35,
      {0,-1,0,0,0,0,0,0,6,-1,0,0,5,3,1,0}, {0,-1,0,0,7,-1,0,0,8,7,5,3,2,-1,0,0},
      {0,0,1,0,0,3,1,0}, {19,18,15,14,19,22,23,26}, 0xddfd,0x1010,0x5555,0x0c81,4.4f,.59f,.55f,ORGAN,WAR_METAL},
    {"Necromante - faixa anterior", "necromante", 178, 36,
      {0,0,0,-1,0,0,5,0,0,0,8,0,7,0,1,0}, {0,-1,0,0,3,0,0,0,6,0,0,0,8,7,1,0},
      {0,3,-4,-5,0,-4,-2,-5}, {19,22,24,23,24,27,29,23}, 0xf7ff,0x1110,0xffff,0x2880,4.55f,.66f,.78f,ORGAN,CELESTIAL_METAL},
    {"Horda de Chã da Tristeza", "horda_vila", 172, 45,
      {0,-1,0,0,3,-1,0,0,5,5,-1,3,7,-1,5,-1}, {0,0,3,-1,5,5,5,-1,7,-1,7,7,5,3,5,-1},
      {0,0,-3,-2,0,-5,-3,-2}, {12,15,19,17,22,19,15,14}, 0x5515,0x1010,0xffff,0x0080,2.8f,.72f,.42f,LEAD,HORDE_ROCK},
    {"Igris - Duelo do Cavaleiro", "igris_duelo_teste", 150, 38,
      {0,0,-1,0,0,0,7,0,0,-1,0,0,8,7,5,0}, {0,-1,0,0,3,3,-1,3,8,8,-1,8,7,-1,6,0},
      {0,0,-4,-5,0,-4,-2,-5}, {24,22,21,19,24,26,25,19}, 0xddf5,0x1010,0x5555,0x0480,3.7f,.57f,.64f,BOWED,KNIGHT_DUEL},
    {"Necromante - Anjo Caído", "necromante_anjo_caido_teste", 160, 35,
      {0,0,0,0,0,0,6,0,0,0,0,1,0,0,7,0}, {0,0,0,-1,8,0,0,0,7,0,5,0,1,1,0,0},
      {0,0,-2,0,0,3,-2,0}, {19,18,22,23,24,27,30,23}, 0xffff,0x1010,0xffff,0x8880,5.9f,.66f,.84f,ORGAN,FALLEN_ANGEL}
};

static const Theme *ThemeForTrack(GameTrack track) {
    if (track<TRACK_GUARDIAN || track>TRACK_NECROMANCER_FALLEN_ANGEL) return NULL;
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
            case PIANO:
                signal=(.64f*sinf(phase)+.23f*sinf(2.003f*phase)+.12f*sinf(3.009f*phase))*
                    expf(-t*2.0f)+.14f*sinf(4.021f*phase)*expf(-t*12);
                attack=fminf(1,t*1050);
                break;
            case BOWED: {
                float vibrato=.09f*sinf(tau*5.1f*t)*fminf(1,t*12);
                float bow=phase+vibrato;
                signal=.68f*sinf(bow)+.28f*sinf(2*bow)+.17f*sinf(3*bow)+.12f*sinf(4*bow)+.07f*sinf(6*bow);
                attack=fminf(1,t*55);
                break;
            }
            case HARPSICHORD:
                signal=(.48f*sinf(phase)+.34f*sinf(2*phase)+.23f*sinf(3*phase)+
                    .18f*sinf(5.02f*phase)+.10f*sinf(7.03f*phase))*expf(-t*15);
                attack=fminf(1,t*1300);
                break;
            case BRASS: {
                float swell=fminf(1,t*14);
                signal=.76f*sinf(phase)+swell*(.30f*sinf(2*phase)+.23f*sinf(3*phase)+
                    .17f*sinf(4*phase)+.12f*sinf(5*phase)+.08f*sinf(7*phase));
                attack=swell;
                break;
            }
            default:
                signal=.73f*sinf(phase+.10f*sinf(tau*5*t))+.30f*sinf(phase*2.005f)+.21f*sinf(phase*3);
                break;
        }
        Mix(r,r->at+i,gain*attack*release*signal);
    }
}

static void Hit(Render *r, Drum drum, float gain) {
    static const float lengths[]={.18f,.19f,.074f,.48f,.24f,.37f,.22f};
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
        } else if (drum==WAR_DRUM) {
            phase+=tau*(40+85*expf(-26*t))/RATE;
            body=(.78f*sinf(phase)+.24f*sinf(phase*1.49f))*f*f;
            grit=.35f*(white-low)*expf(-t*39)*f;
        } else if (drum==IRON) {
            body=(.65f*sinf(tau*481*t)+.36f*sinf(tau*837*t)+.31f*sinf(tau*1382*t))*expf(-t*23)*f;
            grit=.38f*(white-low)*expf(-t*104);
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

static void ArrangeLament(const Theme *theme, Render *r, double step) {
    for (int bar=0; bar<BARS; bar++) {
        int root=theme->key+theme->roots[bar];
        int start=(int)((bar*STEPS)*step+.5);
        const int *part=bar<4 ? theme->riff : theme->refrain;
        r->at=start;
        Pan(r,.41f);
        Tone(r,Frequency(root-12),(int)(step*19),.24f,STRINGS);
        Pan(r,.64f);
        Tone(r,Frequency(root+(bar==0 || bar==4 ? 3 : 7)),(int)(step*17),.15f,STRINGS);
        for (int beat=0; beat<STEPS; beat++) {
            if (part[beat]<0) continue;
            r->at=start+(int)(beat*step);
            Pan(r,.40f+.19f*((beat/2)%2));
            Tone(r,Frequency(root+part[beat]),(int)(step*8),beat==0 ? .35f : .19f,PIANO);
        }
        r->at=start+(int)(step*2);
        Pan(r,.53f);
        Tone(r,Frequency(theme->key+theme->melody[bar]),(int)(step*12),.36f,PIANO);
        r->at=start+(int)(step*(bar%2 ? 11 : 13));
        Pan(r,.67f);
        Tone(r,Frequency(theme->key+theme->melody[(bar+1)%BARS]),(int)(step*9),.16f,PIANO);
    }
}

static void Choir(Render *r, int note, int length, float gain, float tuning) {
    float phase=0, rate=tau*Frequency(note)*tuning/RATE;
    for (int i=0; i<length; i++) {
        float t=(float)i/RATE, end=(float)(length-i)/RATE;
        phase+=rate*(1+.004f*sinf(tau*4.8f*t));
        float vowel=.5f+.5f*sinf(tau*.65f*t);
        float voice=.72f*sinf(phase)+(.34f-.16f*vowel)*sinf(2*phase)+
            (.15f+.30f*vowel)*sinf(3*phase)+.16f*sinf(4*phase)+.11f*sinf(6*phase)+.05f*sinf(9*phase);
        float envelope=fminf(1,t*5.2f)*fminf(1,end*4.4f);
        float value=gain*voice*envelope;
        Mix(r,r->at+i,value);
        Mix(r,r->at+i+2750,.16f*value);
        Mix(r,r->at+i+4950,.09f*value);
    }
}

static void ChoirChord(Render *r, int root, int third, int length, float gain) {
    Pan(r,.49f);
    Choir(r,root-12,length,gain*.73f,1);
    Pan(r,.18f);
    Choir(r,root+7,length,gain*.63f,.997f);
    Pan(r,.80f);
    Choir(r,root+12+third,length,gain*.72f,1.003f);
    Pan(r,.61f);
    Choir(r,root+19,length,gain*.45f,1.001f);
}

static int ThemeBars(const Theme *theme) {
    switch (theme->arrangement) {
        case WAR_METAL: case KNIGHT_DUEL: return 16;
        case CELESTIAL_METAL: return 24;
        case HORDE_ROCK: return 12;
        case FALLEN_ANGEL: return 24;
        default: return BARS;
    }
}

static void DuelStrings(Render *r, int root, int beat, double step, bool refrain) {
    static const int cello[]={0,7,0,3,0,8,7,6};
    static const int violin[]={19,22,21,17,15,19,18,14};
    if (beat%2==0) {
        int index=beat/2;
        Pan(r,.53f);
        Tone(r,Frequency(root+cello[index]),(int)(step*1.86),.23f,BOWED);
        Pan(r,.34f);
        Tone(r,Frequency(root+violin[index]+(refrain ? 5 : 0)),(int)(step*1.94),.26f,BOWED);
    }
    if (beat%4==2) {
        Pan(r,.73f);
        Tone(r,Frequency(root+24+((beat==6 || beat==14) ? 7 : 3)),(int)(step*2.5),.14f,HARPSICHORD);
    } else if (beat==0 && refrain) {
        Pan(r,.67f);
        Tone(r,Frequency(root+31),(int)(step*6),.10f,BELL);
    }
}

static void SoloGuitar(Render *r, int note, int length, float gain, bool bend) {
    float phase=0, cabinet=0, hz=Frequency(note);
    Pan(r,.52f);
    for (int i=0; i<length; i++) {
        float t=(float)i/RATE, p=(float)i/length;
        float bendPitch=bend ? .124f*fminf(1,t*13) : 0;
        float vibrato=.012f*sinf(tau*6.4f*t)*fminf(1,t*7);
        phase+=tau*hz*(1+bendPitch+vibrato)/RATE;
        float string=sinf(phase)+.45f*sinf(2*phase)+.25f*sinf(3*phase)+.12f*sinf(5*phase);
        float driven=2.8f*string/(1+1.9f*fabsf(string));
        cabinet+=.34f*(driven-cabinet);
        float value=gain*cabinet*fminf(1,t*110)*(1-.37f*p)*fminf(1,(1-p)*8);
        Mix(r,r->at+i,value);
        Mix(r,r->at+i+4200,.24f*value);
    }
}

static void FallenAngelLead(Render *r, int root, int bar, int beat, double step, bool finale) {
    static const int solo[]={12,15,18,19,22,24,23,18};
    if (beat==0 || (beat==10 && finale)) {
        int length=(int)(step*(beat==0 ? 9 : 6));
        Pan(r,.27f);
        Tone(r,Frequency(root+12),length,.30f,BRASS);
        Pan(r,.73f);
        Tone(r,Frequency(root+19),length,.26f,BRASS);
    }
    if (bar>=8 && (bar<12 || finale) && beat%2==0) {
        int index=(beat/2 + (bar%4)*2)%8;
        SoloGuitar(r,root+12+solo[index],(int)(step*(beat==14 ? 3.7 : 1.88)),
                   finale ? .28f : .35f,beat==14);
    }
}

static void ArrangeHeavy(const Theme *theme, Render *r, int bars, double step) {
    const bool duel=theme->arrangement==KNIGHT_DUEL;
    const bool fallen=theme->arrangement==FALLEN_ANGEL;
    const bool war=theme->arrangement==WAR_METAL || duel;
    const bool celestial=theme->arrangement==CELESTIAL_METAL || fallen;
    static const int breakdownRiff[]={0,-1,-1,0,-1,-1,1,-1,0,-1,-1,6,-1,5,-1,-1};
    static const int ascent[]={0,7,12,15,19,15,12,7};
    for (int bar=0; bar<bars; bar++) {
        bool refrain=bar%8>=4;
        bool breakdown=(war && !duel && bar>=8 && bar<12) || (celestial && bar>=12 && bar<16);
        bool finale=celestial && bar>=16;
        bool invocation=celestial && bar<2;
        int root=theme->key+theme->roots[bar%BARS];
        const int *riff=breakdown ? breakdownRiff : refrain ? theme->refrain : theme->riff;
        if (celestial) {
            r->at=(int)(bar*STEPS*step+.5);
            ChoirChord(r,root,bar%4==1 ? 4 : 3,(int)(step*20),invocation || breakdown ? .49f : finale ? .43f : .30f);
        }
        for (int beat=0; beat<STEPS; beat++) {
            int at=(int)((bar*STEPS+beat)*step+.5);
            unsigned bit=1u<<beat;
            bool downbeat=beat%4==0;
            bool fill=beat>=12 && bar%4==3;
            unsigned kick=invocation ? 0x0111 : breakdown ? 0x0949 : theme->kick;
            unsigned snare=breakdown || invocation ? 0x0100 : theme->snare;
            r->at=at;
            Pan(r,.50f);
            if (kick&bit) Hit(r,KICK,downbeat ? .82f : celestial || war ? .64f : .48f);
            if (snare&bit) Hit(r,SNARE,breakdown ? .92f : .71f);
            if ((war || celestial) && !invocation && !breakdown && (beat==7 || (finale && beat==15))) {
                r->at=at+(int)(step*.50);
                Hit(r,KICK,.53f);
                r->at=at;
            }
            if ((war || celestial) && (beat==0 || ((war || breakdown) && beat==8))) {
                Hit(r,WAR_DRUM,breakdown ? .98f : .67f);
                if (war && (breakdown || bar%4==0)) {
                    Pan(r,.64f);
                    Hit(r,IRON,.33f);
                }
            }
            Pan(r,.65f);
            if (!invocation && (theme->hats&bit)) Hit(r,HAT,downbeat ? .27f : .15f);
            if (beat==0 && (bar%4==0 || (finale && bar%2==0))) Hit(r,CRASH,.41f);
            if (fill || (theme->tom&bit && (bar%2 || breakdown))) {
                Pan(r,.23f+.17f*(beat%4));
                Hit(r,TOM,fill ? .53f : .35f);
            }
            if (riff[beat]>=0 && (!invocation || downbeat)) {
                int note=root+riff[beat];
                bool chord=downbeat || breakdown || (finale && beat%2==0);
                float sustain=breakdown ? 1.65f : theme->sustain;
                int length=(int)(step*(invocation ? 3.5f : chord ? sustain+.28f : sustain));
                float gain=invocation ? .32f : fallen ? .68f : duel ? .54f : war || celestial ? .59f : .50f;
                Pan(r,.12f);
                Guitar(r,note,length,theme->drive,gain,1.003f);
                if (chord) Guitar(r,note+7,length,theme->drive,.34f,1);
                r->at=at+125;
                Pan(r,.88f);
                Guitar(r,note,length,theme->drive,gain,.997f);
                if (chord) Guitar(r,note+7,length,theme->drive,.30f,1.001f);
                r->at=at;
                Pan(r,.48f);
                Tone(r,Frequency(note-12),length,.24f,LEAD);
            }
            if (celestial && (invocation || breakdown || finale) && beat%2==0) {
                int note=root+ascent[beat/2]+(finale ? 12 : 0);
                Pan(r,beat%4==0 ? .34f : .66f);
                Tone(r,Frequency(note),(int)(step*4),invocation ? .22f : .15f,PIANO);
            }
            if (beat==0 || (refrain && beat==10)) {
                int lead=theme->key+theme->melody[bar%BARS]-(beat==10 ? 2 : 0);
                int length=(int)(step*(beat==0 ? 10 : 6));
                Pan(r,.59f);
                Tone(r,Frequency(lead),length,.34f*theme->accompaniment,theme->color);
                if ((war && refrain && !breakdown) || finale) {
                    Pan(r,.35f);
                    Guitar(r,lead-7,length,2.7f,.21f,1.002f);
                    if (finale) Tone(r,Frequency(lead+7),length,.14f,ORGAN);
                }
            }
            if (duel) DuelStrings(r,root,beat,step,refrain);
            if (fallen) FallenAngelLead(r,root,bar,beat,step,finale);
        }
    }
}

static void WriteLE(unsigned char *where, uint32_t value, int bytes) {
    for (int i=0; i<bytes; i++) where[i]=(unsigned char)(value>>(8*i));
}

static unsigned char *RenderTheme(const Theme *theme, int *size) {
    int bars=ThemeBars(theme);
    double step=RATE*60.0/(theme->bpm*4.0);
    int frames=(int)(step*STEPS*bars+.5);
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
    if (theme->arrangement==CLASSIC) Arrange(theme,&render);
    else if (theme->arrangement==LAMENT) ArrangeLament(theme,&render,step);
    else ArrangeHeavy(theme,&render,bars,step);
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
