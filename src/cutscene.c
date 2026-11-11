#include "cutscene.h"
#include <math.h>
#include <stdio.h>

#define INTRO_DIR "assets/cutscenes/intro/"

enum { BATTLEFIELD, FALLEN, ANGEL, AWAKENING, OATH, HAND, CONFRONTATION, OATH_INSERT };
enum { INTRO_WIDTH = 1280, INTRO_HEIGHT = 720 };

static const struct {
    float duration;
    const char *cue;
    const char *caption;
} beats[] = {
    {5.0f, "01_campo.wav", "A cruzada terminou."},
    {6.0f, "02_sobrevivente.wav", "A maldição, não."},
    {6.0f, "03_despertar.wav", NULL},
    {7.0f, "04_juramento.wav", "Todos caíram..."},
    {6.0f, "05_maldicao.wav", NULL},
    {10.0f, "06_confronto.wav", NULL}
};
static const char *oathCaption = "Mas o juramento permanece.";
static const float outroDuration = 0.75f;

static float Clamp01(float value) {
    return fmaxf(0.0f, fminf(value, 1.0f));
}

static float Ease(float value) {
    float t = Clamp01(value);
    return t*t*(3.0f - 2.0f*t);
}

static void PlayCue(const OpeningCutscene *scene) {
    if (scene->beat < INTRO_BEAT_COUNT && scene->cues[scene->beat].stream.buffer) {
        SetSoundVolume(scene->cues[scene->beat], 0.45f);
        PlaySound(scene->cues[scene->beat]);
    }
}

static void LoadOpeningAudio(OpeningCutscene *scene) {
    // Each optional WAV is a mix of the effects for that beat; the score spans the whole scene.
    const char *musicPath = INTRO_DIR "trilha.ogg";
    char path[96];
    bool available = FileExists(musicPath);
    for (int i = 0; !available && i < INTRO_BEAT_COUNT; i++) {
        snprintf(path, sizeof(path), "%s%s", INTRO_DIR, beats[i].cue);
        available = FileExists(path);
    }
    if (!available) return;
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        scene->ownsAudio = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) return;

    for (int i = 0; i < INTRO_BEAT_COUNT; i++) {
        snprintf(path, sizeof(path), "%s%s", INTRO_DIR, beats[i].cue);
        if (FileExists(path)) scene->cues[i] = LoadSound(path);
    }
    if (FileExists(musicPath)) {
        scene->music = LoadMusicStream(musicPath);
        if (scene->music.stream.buffer) {
            SetMusicVolume(scene->music, 0.0f);
            PlayMusicStream(scene->music);
        }
    }
    PlayCue(scene);
}

bool InitOpeningCutscene(OpeningCutscene *scene) {
    *scene = (OpeningCutscene){0};
    for (int i = 0; i < OATH_INSERT; i++) {
        char path[96];
        snprintf(path, sizeof(path), INTRO_DIR "quadro_%02d.png", i+1);
        scene->art[i] = LoadTexture(path);
        if (!scene->art[i].id) {
            UnloadOpeningCutscene(scene);
            return false;
        }
        SetTextureFilter(scene->art[i], TEXTURE_FILTER_BILINEAR);
    }
    const char *insertPath = INTRO_DIR "quadro_04_complemento.png";
    if (FileExists(insertPath)) scene->art[OATH_INSERT] = LoadTexture(insertPath);
    if (scene->art[OATH_INSERT].id)
        SetTextureFilter(scene->art[OATH_INSERT], TEXTURE_FILTER_BILINEAR);
    LoadOpeningAudio(scene);
    return true;
}

void UnloadOpeningCutscene(OpeningCutscene *scene) {
    for (int i = 0; i < INTRO_ART_COUNT; i++)
        if (scene->art[i].id) UnloadTexture(scene->art[i]);
    for (int i = 0; i < INTRO_BEAT_COUNT; i++)
        if (scene->cues[i].stream.buffer) UnloadSound(scene->cues[i]);
    if (scene->music.stream.buffer) UnloadMusicStream(scene->music);
    if (scene->ownsAudio) CloseAudioDevice();
    *scene = (OpeningCutscene){0};
}

void UpdateOpeningCutscene(OpeningCutscene *scene, float dt) {
    if (scene->finished) return;
    if (IsKeyPressed(KEY_ESCAPE)) {
        scene->finished = true;
        return;
    }
    dt = fmaxf(0.0f, dt);
    scene->age += dt;
    bool advance = scene->beat < INTRO_BEAT_COUNT &&
        (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER));
    if (advance) {
        if (scene->cues[scene->beat].stream.buffer) StopSound(scene->cues[scene->beat]);
        if (scene->beat == INTRO_BEAT_COUNT-1) scene->finalBeatAtAdvance = scene->elapsed;
        scene->beat++;
        scene->elapsed = 0.0f;
        PlayCue(scene);
    } else {
        int previous = scene->beat;
        scene->elapsed += dt;
        while (scene->beat < INTRO_BEAT_COUNT && scene->elapsed >= beats[scene->beat].duration) {
            if (scene->beat == INTRO_BEAT_COUNT-1) scene->finalBeatAtAdvance = beats[scene->beat].duration;
            scene->elapsed -= beats[scene->beat].duration;
            scene->beat++;
        }
        if (scene->beat != previous) PlayCue(scene);
    }

    if (scene->beat == INTRO_BEAT_COUNT && scene->elapsed >= outroDuration) scene->finished = true;
    float fade = scene->beat == INTRO_BEAT_COUNT ? 1.0f-Ease(scene->elapsed/outroDuration) : 1.0f;
    if (scene->beat == INTRO_BEAT_COUNT) {
        for (int i = 0; i < INTRO_BEAT_COUNT; i++)
            if (scene->cues[i].stream.buffer) SetSoundVolume(scene->cues[i], 0.45f*fade);
    }
    if (scene->music.stream.buffer) {
        float swell = scene->beat == INTRO_BEAT_COUNT ? 1.0f :
            scene->beat == INTRO_BEAT_COUNT-1 ? Ease(scene->elapsed / beats[scene->beat].duration) : 0.0f;
        SetMusicVolume(scene->music, (0.28f + 0.16f*swell)*Ease(scene->age)*fade);
        UpdateMusicStream(scene->music);
    }
}

static Rectangle Place(Rectangle canvas, Rectangle logical) {
    float scale = canvas.width/INTRO_WIDTH;
    return (Rectangle){canvas.x + logical.x*scale, canvas.y + logical.y*scale,
                       logical.width*scale, logical.height*scale};
}

static void DrawPanel(Texture2D art, Rectangle canvas, Rectangle box,
                      Vector2 focus, float zoom, float shade, float opacity) {
    Rectangle dst = Place(canvas, box);
    float fit = fmaxf(dst.width/art.width, dst.height/art.height)*zoom;
    float width = dst.width/fit, height = dst.height/fit;
    Rectangle src = {
        fmaxf(0.0f, fminf(art.width-width, art.width*focus.x-width*0.5f)),
        fmaxf(0.0f, fminf(art.height-height, art.height*focus.y-height*0.5f)),
        width, height
    };
    float edge = 8.0f*canvas.width/INTRO_WIDTH;
    DrawRectangleRec((Rectangle){dst.x-edge,dst.y-edge,dst.width+2*edge,dst.height+2*edge},
                     Fade(BLACK, opacity));
    DrawTexturePro(art, src, dst, (Vector2){0}, 0, Fade(WHITE, opacity));
    if (shade > 0) DrawRectangleRec(dst, Fade(BLACK, shade*opacity));
}

static void DrawCaption(Rectangle canvas, const char *text, Vector2 location, float opacity) {
    float scale = canvas.width/INTRO_WIDTH;
    float size = 32.0f*scale;
    Font font = GetFontDefault();
    Vector2 label = MeasureTextEx(font, text, size, scale);
    Rectangle box = Place(canvas, (Rectangle){location.x,location.y,label.x/scale+48,60});
    DrawRectangleRec((Rectangle){box.x+4*scale,box.y+5*scale,box.width,box.height}, Fade(BLACK,.8f*opacity));
    DrawRectangleRec(box, Fade((Color){51,30,25,255},opacity));
    Rectangle paper = {box.x+3*scale,box.y+3*scale,box.width-6*scale,box.height-6*scale};
    DrawRectangleGradientV((int)paper.x,(int)paper.y,(int)paper.width,(int)paper.height,
                           Fade((Color){233,211,165,255},opacity), Fade((Color){187,148,104,255},opacity));
    DrawTextEx(font, text, (Vector2){box.x+24*scale,box.y+14*scale},
               size, scale, Fade((Color){42,23,20,255},opacity));
}

void DrawOpeningCutscene(const OpeningCutscene *scene) {
    float scale = fminf(GetScreenWidth()/(float)INTRO_WIDTH, GetScreenHeight()/(float)INTRO_HEIGHT);
    Rectangle canvas = {(GetScreenWidth()-INTRO_WIDTH*scale)/2, (GetScreenHeight()-INTRO_HEIGHT*scale)/2,
                        INTRO_WIDTH*scale, INTRO_HEIGHT*scale};
    int beat = scene->beat < INTRO_BEAT_COUNT ? scene->beat : INTRO_BEAT_COUNT-1;
    float t = scene->elapsed;
    if (scene->beat == INTRO_BEAT_COUNT) {
        float start = scene->finalBeatAtAdvance;
        t = start + (beats[beat].duration-start)*Ease(scene->elapsed/.42f);
    }
    float progress = Ease(t/beats[beat].duration);
    float reveal = Ease(t/.78f);
    const Texture2D *art = scene->art;
    DrawRectangleRec(canvas, (Color){17,10,14,255});
    BeginScissorMode((int)canvas.x,(int)canvas.y,(int)canvas.width,(int)canvas.height);
    switch (beat) {
        case 0:
            DrawPanel(art[BATTLEFIELD],canvas,(Rectangle){60,67,1160,540},(Vector2){.54f,.48f},
                      1.0f+.07f*progress,0,Ease(t/1.15f));
            DrawCaption(canvas, beats[beat].caption, (Vector2){93,526},Ease((t-.85f)/.5f));
            break;
        case 1:
            DrawPanel(art[BATTLEFIELD],canvas,(Rectangle){60,79,510,492},(Vector2){.32f,.49f},1.05f,.43f,1);
            DrawPanel(art[FALLEN],canvas,(Rectangle){397+(1-reveal)*370,147,818,485},
                      (Vector2){.43f,.60f},1.0f+.10f*progress,0,1);
            DrawCaption(canvas,beats[beat].caption,(Vector2){455,555},Ease((t-.58f)/.56f));
            break;
        case 2: {
            DrawPanel(art[ANGEL],canvas,(Rectangle){59,60,839,497},(Vector2){.50f,.42f},1.03f+.05f*progress,0,reveal);
            float close = Ease((t-1.25f)/1.0f);
            DrawPanel(art[AWAKENING],canvas,(Rectangle){618+(1-close)*90,259,600,377},
                      (Vector2){.40f,.44f},1.0f+.17f*progress,0,close);
            Rectangle light = Place(canvas,(Rectangle){341,65,276,210});
            DrawCircleGradient((int)(light.x+light.width/2),(int)light.y,light.width*.83f,
                               Fade((Color){255,204,132,255},.14f*reveal),BLANK);
            break;
        }
        case 3: {
            DrawPanel(art[OATH],canvas,(Rectangle){60,62,918,576},(Vector2){.49f,.51f},1.0f+.08f*progress,0,reveal);
            float insert = Ease((t-1.0f)/.8f);
            Rectangle sidePanel = {926+(1-insert)*90,132,288,432};
            if (art[OATH_INSERT].id)
                DrawPanel(art[OATH_INSERT],canvas,sidePanel,(Vector2){.5f,.5f},1.0f,0,insert);
            else
                DrawPanel(art[OATH],canvas,sidePanel,(Vector2){.419f,.73f},1.79f,.06f,insert);
            if (t < 3.1f)
                DrawCaption(canvas,beats[beat].caption,(Vector2){91,513},Ease((t-.7f)/.52f)*(1-Ease((t-2.65f)/.45f)));
            else DrawCaption(canvas,oathCaption,(Vector2){111,543},Ease((t-3.1f)/.52f));
            break;
        }
        case 4: {
            DrawPanel(art[OATH],canvas,(Rectangle){58,70,790,534},(Vector2){.47f,.49f},1.06f,.53f,1);
            float tremor = sinf(t*74.0f)*(1-Ease(t/.32f))*4.0f;
            DrawPanel(art[HAND],canvas,(Rectangle){812+tremor,56,397,591},(Vector2){.57f,.51f},
                      1.0f+.16f*progress,0,1);
            break;
        }
        case 5: {
            float expand = Ease((t-1.15f)/6.5f);
            DrawPanel(art[HAND],canvas,(Rectangle){812,56,397,591},(Vector2){.57f,.51f},1.16f,.62f,1);
            DrawPanel(art[CONFRONTATION],canvas,
                      (Rectangle){(1-expand)*83,(1-expand)*82,INTRO_WIDTH-(1-expand)*166,INTRO_HEIGHT-(1-expand)*168},
                      (Vector2){.50f,.48f},1.0f+.035f*progress,0,Ease(t/.48f));
            break;
        }
    }
    if (scene->beat < INTRO_BEAT_COUNT) {
        const char *hint = "Espaço / Enter  Avançar    |    Esc  Pular";
        int size = (int)fmaxf(10.0f,17.0f*scale);
        Vector2 at = {canvas.x+canvas.width-31*scale-MeasureText(hint,size),canvas.y+681*scale};
        DrawRectangle((int)at.x-8,(int)at.y-4,MeasureText(hint,size)+16,size+8,Fade(BLACK,.8f));
        DrawText(hint,(int)at.x,(int)at.y,size,(Color){223,207,190,255});
    }
    EndScissorMode();
    if (scene->beat == INTRO_BEAT_COUNT)
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),
                      Fade(BLACK,Ease((scene->elapsed-.15f)/(outroDuration-.15f))));
}
