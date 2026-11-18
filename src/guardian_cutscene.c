#include "guardian_cutscene.h"
#include "game_text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GUARDIAN_DIR "assets/cutscenes/boss_clareira/"

static float Ease(float t) {
    t = fminf(1, fmaxf(0, t));
    return t*t*(3-2*t);
}

static char *Trim(char *text) {
    while (*text == ' ' || *text == '\t') text++;
    size_t size = strlen(text);
    while (size && (text[size-1] == ' ' || text[size-1] == '\r' || text[size-1] == '\t'))
        text[--size] = 0;
    return text;
}

static CinematicTransition Transition(const char *value) {
    if (!strcmp(value, "fade")) return CINEMATIC_FADE;
    if (!strcmp(value, "white")) return CINEMATIC_WHITE;
    if (!strcmp(value, "crossfade")) return CINEMATIC_CROSSFADE;
    return CINEMATIC_CUT;
}

static bool LoadSequence(GuardianCutscene *scene) {
    char *data = LoadFileText(GUARDIAN_DIR "data/sequencia.ini");
    if (!data) return false;
    char *content=data;
    if (!strncmp(content,"\xEF\xBB\xBF",3)) content+=3;
    GuardianScene *shot = NULL;
    bool valid = true;
    for (char *line = strtok(content, "\n"); line; line = strtok(NULL, "\n")) {
        line = Trim(line);
        if (*line == '#' || *line == ';' || !*line) continue;
        if (*line == '[') {
            if (scene->count == GUARDIAN_SCENE_LIMIT) { valid = false; break; }
            shot = &scene->scenes[scene->count++];
            *shot = (GuardianScene){.duration=4, .fadeIn=.55f, .fadeOut=.55f,
                .narrationAt=.65f, .narrationDuration=3.6f, .narrationFade=.5f,
                .cueGain=.55f,
                .zoomStart=1, .zoomEnd=1.04f, .focusStart={.5f,.5f}, .focusEnd={.5f,.5f},
                .sealPosition={.65f,.72f}, .transitionIn=CINEMATIC_CROSSFADE,
                .transitionOut=CINEMATIC_CROSSFADE, .artIndex=-1};
            if (sscanf(line, "[scene %63[^]]]", shot->id) != 1) { valid = false; break; }
            continue;
        }
        char *equals = strchr(line, '=');
        if (!shot || !equals) { valid = false; break; }
        *equals = 0;
        char *key = Trim(line), *value = Trim(equals+1);
        if (!strcmp(key,"type")) {
            const char *types[]={"cinematic","narration","dialogue","effect","transition"};
            int type=0;
            while (type<5 && strcmp(value,types[type])) type++;
            if (type==5) valid=false;
            else shot->type=(GuardianEventType)type;
        }
        else if (!strcmp(key,"narration")) {
            if (strlen(value)>=sizeof(shot->narration)) valid=false;
            snprintf(shot->narration,sizeof(shot->narration),"%s",value);
        }
        else if (!strcmp(key,"narration_at")) shot->narrationAt=strtof(value,NULL);
        else if (!strcmp(key,"narration_duration")) shot->narrationDuration=strtof(value,NULL);
        else if (!strcmp(key,"narration_fade")) shot->narrationFade=strtof(value,NULL);
        else if (!strcmp(key,"image")) snprintf(shot->image,sizeof(shot->image),"%s",value);
        else if (!strcmp(key,"duration")) shot->duration = strtof(value,NULL);
        else if (!strcmp(key,"fade_in")) shot->fadeIn = strtof(value,NULL);
        else if (!strcmp(key,"fade_out")) shot->fadeOut = strtof(value,NULL);
        else if (!strcmp(key,"audio_at")) shot->cueAt = strtof(value,NULL);
        else if (!strcmp(key,"audio_gain")) shot->cueGain = strtof(value,NULL);
        else if (!strcmp(key,"transition_in")) shot->transitionIn = Transition(value);
        else if (!strcmp(key,"transition_out")) shot->transitionOut = Transition(value);
        else if (!strcmp(key,"camera")) {
            if (sscanf(value,"%f %f %f %f %f %f", &shot->focusStart.x,&shot->focusStart.y,
                &shot->zoomStart,&shot->focusEnd.x,&shot->focusEnd.y,&shot->zoomEnd) != 6) valid=false;
        } else if (!strcmp(key,"seal")) {
            if (sscanf(value,"%f %f",&shot->sealPosition.x,&shot->sealPosition.y) != 2) valid=false;
        } else if (!strcmp(key,"effects")) {
            if (strstr(value,"leaves")) shot->effects |= CINEMATIC_LEAVES;
            if (strstr(value,"fog")) shot->effects |= CINEMATIC_FOG;
            if (strstr(value,"shake")) shot->effects |= CINEMATIC_SHAKE;
            if (strstr(value,"seal")) shot->effects |= CINEMATIC_SEAL;
            if (strstr(value,"corruption")) shot->effects |= CINEMATIC_CORRUPTION;
            if (strstr(value,"dust")) shot->effects |= CINEMATIC_DUST;
        } else if (!strcmp(key,"speaker")) snprintf(shot->speaker,sizeof(shot->speaker),"%s",value);
        else if (!strcmp(key,"dialogue")) snprintf(shot->dialogue,sizeof(shot->dialogue),"%s",value);
        else if (!strcmp(key,"audio_event")) snprintf(shot->audioEvent,sizeof(shot->audioEvent),"%s",value);
        else if (!strcmp(key,"music")) snprintf(shot->music,sizeof(shot->music),"%s",value);
        else if (!strcmp(key,"title")) snprintf(shot->title,sizeof(shot->title),"%s",value);
        else if (!strcmp(key,"subtitle")) snprintf(shot->subtitle,sizeof(shot->subtitle),"%s",value);
        else if (!strcmp(key,"next")) {
            if (strcmp(value,"duration") && strcmp(value,"advance")) valid=false;
            shot->waitsForInput = !strcmp(value,"advance");
        } else valid=false;
    }
    UnloadFileText(data);
    for (int i=0; i<scene->count; i++) {
        GuardianScene *s = &scene->scenes[i];
        const float numbers[] = {s->duration,s->fadeIn,s->fadeOut,s->cueAt,s->cueGain,s->zoomStart,s->zoomEnd,
            s->narrationAt,s->narrationDuration,s->narrationFade,
            s->focusStart.x,s->focusStart.y,s->focusEnd.x,s->focusEnd.y,s->sealPosition.x,s->sealPosition.y};
        for (int n=0; n<(int)(sizeof(numbers)/sizeof(*numbers)); n++)
            if (!isfinite(numbers[n])) valid=false;
        if (s->duration < .5f || s->duration > 60 || s->fadeIn < 0 || s->fadeOut < 0 ||
            s->cueAt < 0 || s->cueAt >= s->duration || s->cueGain < 0 || s->cueGain > 1 ||
            s->fadeIn > s->duration || s->fadeOut > s->duration ||
            s->zoomStart < 1 || s->zoomStart > 1.2f || s->zoomEnd < 1 || s->zoomEnd > 1.2f ||
            s->focusStart.x < 0 || s->focusStart.x > 1 || s->focusStart.y < 0 || s->focusStart.y > 1 ||
            s->focusEnd.x < 0 || s->focusEnd.x > 1 || s->focusEnd.y < 0 || s->focusEnd.y > 1 ||
            (s->dialogue[0] != 0) != s->waitsForInput ||
            (s->type==GUARDIAN_DIALOGUE) != (s->dialogue[0]!=0) ||
            (s->type==GUARDIAN_NARRATION) != (s->narration[0]!=0) ||
            (s->narration[0] && (s->dialogue[0] || s->speaker[0] || s->title[0])) ||
            s->narrationAt<0 || s->narrationFade<=0 || s->narrationDuration<2*s->narrationFade ||
            (s->narration[0] && s->narrationAt+s->narrationDuration>s->duration) ||
            (s->type==GUARDIAN_EFFECT && !s->effects && !s->audioEvent[0])) valid=false;
        for (int j=0; j<i; j++) if (!strcmp(s->id,scene->scenes[j].id)) valid=false;
    }
    return valid && scene->count > 0;
}

static void StopCinematicAudio(GuardianCutscene *scene) {
    if (scene->music.stream.buffer) StopMusicStream(scene->music);
    for (int i=0; i<scene->count; i++)
        if (scene->scenes[i].cue.stream.buffer) StopSound(scene->scenes[i].cue);
    scene->cuePaused=false;
}

static void PlaySceneCue(GuardianCutscene *scene) {
    GuardianScene *shot=&scene->scenes[scene->current];
    if (!scene->cueStarted && shot->cue.stream.buffer && scene->elapsed >= shot->cueAt) {
        SetSoundVolume(shot->cue,shot->cueGain*scene->effectsVolume);
        PlaySound(shot->cue);
        scene->cueStarted=true;
    }
}

static void EnterScene(GuardianCutscene *scene) {
    GuardianScene *shot = &scene->scenes[scene->current];
    for (int i=0; i<scene->count; i++)
        if (scene->scenes[i].cue.stream.buffer) StopSound(scene->scenes[i].cue);
    scene->cueStarted=scene->cuePaused=false;
    PlaySceneCue(scene);
    if (strcmp(shot->music,scene->musicPath)) {
        if (scene->music.stream.buffer) UnloadMusicStream(scene->music);
        scene->music = (Music){0};
        scene->musicGain = 0;
        snprintf(scene->musicPath,sizeof(scene->musicPath),"%s",shot->music);
        if (IsAudioDeviceReady() && shot->music[0] && FileExists(shot->music)) {
            scene->music = LoadMusicStream(shot->music);
            if (scene->music.stream.buffer) {
                scene->music.looping = true;
                SetMusicVolume(scene->music,0);
                PlayMusicStream(scene->music);
            }
        }
    }
}

bool InitGuardianCutscene(GuardianCutscene *scene, float musicVolume, float effectsVolume) {
    *scene = (GuardianCutscene){.musicVolume=musicVolume,.effectsVolume=effectsVolume};
    if (!LoadSequence(scene)) { UnloadGuardianCutscene(scene); return false; }
    for (int i=0; i<scene->count; i++) {
        GuardianScene *shot = &scene->scenes[i];
        shot->artIndex = -1;
        for (int j=0; j<i; j++)
            if (!strcmp(shot->image,scene->scenes[j].image)) shot->artIndex=scene->scenes[j].artIndex;
        if (shot->artIndex < 0 && shot->image[0]) {
            if (scene->artCount == GUARDIAN_ART_LIMIT) { UnloadGuardianCutscene(scene); return false; }
            char path[256];
            snprintf(path,sizeof(path),GUARDIAN_DIR "images/%s",shot->image);
            if (FileExists(path)) {
                Texture2D art = LoadTexture(path);
                if (art.id) {
                    SetTextureFilter(art,TEXTURE_FILTER_BILINEAR);
                    shot->artIndex=scene->artCount;
                    scene->art[scene->artCount++]=art;
                }
            }
            if (shot->artIndex < 0) TraceLog(LOG_WARNING,"Imagem da cutscene ausente: %s",path);
        }
        if (IsAudioDeviceReady() && shot->audioEvent[0]) {
            char path[256];
            snprintf(path,sizeof(path),GUARDIAN_DIR "audio/%s.wav",shot->audioEvent);
            if (FileExists(path)) shot->cue = LoadSound(path);
        }
    }
    EnterScene(scene);
    return true;
}

void UnloadGuardianCutscene(GuardianCutscene *scene) {
    StopCinematicAudio(scene);
    for (int i=0; i<scene->artCount; i++) UnloadTexture(scene->art[i]);
    for (int i=0; i<scene->count; i++)
        if (scene->scenes[i].cue.stream.buffer) UnloadSound(scene->scenes[i].cue);
    if (scene->music.stream.buffer) UnloadMusicStream(scene->music);
    *scene = (GuardianCutscene){0};
}

GuardianCutsceneInput ReadGuardianCutsceneInput(void) {
    bool controller=IsGamepadAvailable(0);
    return (GuardianCutsceneInput){
        .advance=IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            (controller && IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN)),
        .skip=IsKeyPressed(KEY_ESCAPE) ||
            (controller && IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)),
        .pause=IsKeyPressed(KEY_P) ||
            (controller && IsGamepadButtonPressed(0,GAMEPAD_BUTTON_MIDDLE_RIGHT))};
}

void SetGuardianCutscenePaused(GuardianCutscene *scene, bool paused) {
    if (scene->paused==paused || scene->count<=0) return;
    scene->paused=paused;
    if (scene->music.stream.buffer) {
        if (paused) PauseMusicStream(scene->music);
        else ResumeMusicStream(scene->music);
    }
    Sound cue=scene->scenes[scene->current].cue;
    if (cue.stream.buffer) {
        if (paused && scene->cueStarted && IsSoundPlaying(cue)) {
            PauseSound(cue);
            scene->cuePaused=true;
        } else if (!paused && scene->cuePaused) {
            ResumeSound(cue);
            scene->cuePaused=false;
        }
    }
}

void UpdateGuardianCutscene(GuardianCutscene *scene, float dt, GuardianCutsceneInput input) {
    if (scene->finished || scene->count <= 0) return;
    if (input.skip) {
        StopCinematicAudio(scene);
        scene->finished=true;
        scene->paused=false;
        return;
    }
    if (input.pause) {
        SetGuardianCutscenePaused(scene,!scene->paused);
        return;
    }
    if (scene->paused) return;
    dt=fminf(.1f,fmaxf(0,dt));
    GuardianScene *shot=&scene->scenes[scene->current];
    int letters=GetCodepointCount(shot->dialogue);
    bool next=input.advance || (!shot->waitsForInput && scene->elapsed+dt >= shot->duration);
    if (input.advance && shot->dialogue[0] && scene->letters < letters) {
        scene->letters=(float)letters;
        next=false;
    }
    scene->elapsed+=dt;
    scene->letters=fminf((float)letters,scene->letters+dt*34);
    if (next) {
        if (scene->current == scene->count-1) {
            scene->finished=true;
            StopCinematicAudio(scene);
            return;
        }
        scene->current++;
        scene->elapsed=scene->letters=0;
        EnterScene(scene);
        shot=&scene->scenes[scene->current];
    }
    PlaySceneCue(scene);
    if (scene->music.stream.buffer) {
        float gain=shot->dialogue[0] ? .78f : 1.0f;
        bool changesMusic=scene->current==scene->count-1 ||
            strcmp(shot->music,scene->scenes[scene->current+1].music);
        if (changesMusic && !shot->dialogue[0] && shot->fadeOut>0)
            gain=Ease((shot->duration-scene->elapsed)/shot->fadeOut);
        float step=dt*1.5f;
        scene->musicGain+=fmaxf(-step,fminf(step,gain-scene->musicGain));
        SetMusicVolume(scene->music,.42f*scene->musicVolume*scene->musicGain);
        UpdateMusicStream(scene->music);
    }
}

static Vector2 ArtPoint(Vector2 point, Rectangle source, Rectangle destination, Texture2D art) {
    return (Vector2){destination.x+(point.x*art.width-source.x)*destination.width/source.width,
                     destination.y+(point.y*art.height-source.y)*destination.height/source.height};
}

static void DrawShot(const GuardianCutscene *scene, int index, Rectangle canvas, float elapsed, float alpha) {
    const GuardianScene *shot=&scene->scenes[index];
    DrawRectangleRec(canvas,Fade((Color){15,7,13,255},alpha));
    if (shot->artIndex < 0) return;
    Texture2D art=scene->art[shot->artIndex];
    float progress=Ease(elapsed/shot->duration), scale=canvas.width/1280;
    float zoom=shot->zoomStart+(shot->zoomEnd-shot->zoomStart)*progress;
    Vector2 focus={shot->focusStart.x+(shot->focusEnd.x-shot->focusStart.x)*progress,
                   shot->focusStart.y+(shot->focusEnd.y-shot->focusStart.y)*progress};
    float fit=fmaxf(canvas.width/art.width,canvas.height/art.height)*zoom;
    Rectangle source={0,0,canvas.width/fit,canvas.height/fit};
    source.x=fmaxf(0,fminf(art.width-source.width,art.width*focus.x-source.width/2));
    source.y=fmaxf(0,fminf(art.height-source.height,art.height*focus.y-source.height/2));
    Rectangle destination=canvas;
    if (shot->effects & CINEMATIC_SHAKE) {
        float intensity=(1-Ease(elapsed/1.5f))*2.4f*scale;
        destination.x+=sinf(elapsed*53)*intensity;
        destination.y+=cosf(elapsed*47)*intensity*.7f;
    }
    DrawTexturePro(art,source,destination,(Vector2){0},0,Fade(WHITE,alpha));
    if (shot->effects & CINEMATIC_CORRUPTION)
        DrawRectangleRec(canvas,Fade((Color){66,4,58,255},alpha*(.035f+.025f*sinf(elapsed*1.8f))));
    if (shot->effects & CINEMATIC_SEAL) {
        Vector2 seal=ArtPoint(shot->sealPosition,source,destination,art);
        float pulse=.5f+.5f*sinf(elapsed*3.1f);
        DrawCircleGradient((int)seal.x,(int)seal.y,canvas.width*.10f,
            Fade((Color){238,77,173,255},alpha*(.055f+.07f*pulse)),BLANK);
    }
    if (shot->effects & CINEMATIC_FOG) {
        for (int i=0; i<4; i++) {
            float x=canvas.x+canvas.width*(i*.32f-.12f)+sinf(elapsed*.15f+i)*canvas.width*.045f;
            DrawEllipse((int)x,(int)(canvas.y+canvas.height*(.79f+.025f*sinf(i+elapsed*.24f))),
                canvas.width*.27f,canvas.height*.095f,Fade((Color){185,158,167,255},alpha*.018f));
        }
    }
    if (shot->effects & (CINEMATIC_LEAVES|CINEMATIC_DUST)) {
        for (int i=0; i<28; i++) {
            float seed=i*37.37f, speed=(shot->effects&CINEMATIC_DUST)? .015f:.022f;
            float x=canvas.x+canvas.width*fmodf(seed*.137f+elapsed*speed,1);
            float y=canvas.y+canvas.height*fmodf(seed*.079f+elapsed*(.012f+i*.0004f),1);
            float radius=scale*(1.0f+(i%3)*.5f);
            Color ink=(shot->effects&CINEMATIC_DUST)?(Color){212,174,141,255}:(Color){211,101,48,255};
            DrawEllipse((int)x,(int)(y+sinf(elapsed+i)*3*scale),radius*1.8f,radius,Fade(ink,.3f*alpha));
        }
    }
}

static void VisibleDialogue(const char *text, int letters, char *out) {
    const char *end=text;
    for (int i=0; i<letters && *end; i++) {
        int bytes=0;
        GetCodepointNext(end,&bytes);
        end+=bytes;
    }
    size_t bytes=(size_t)(end-text);
    memcpy(out,text,bytes);
    out[bytes]=0;
}

static int WrapDialogue(const char *text, float size, float width, char lines[8][512]) {
    int count=0;
    const char *start=text;
    while (*start && count<8) {
        const char *end=start, *space=NULL;
        char *line=lines[count];
        while (*end) {
            int bytes=0;
            GetCodepointNext(end,&bytes);
            size_t length=(size_t)(end+bytes-start);
            if (length>=512) break;
            memcpy(line,start,length);
            line[length]=0;
            if (MeasureGameTextEx(line,size,false).x>width) break;
            if (*end==' ') space=end;
            end+=bytes;
        }
        if (*end && space && space>start) end=space;
        if (end==start) end++;
        size_t length=(size_t)(end-start);
        memcpy(line,start,length);
        line[length]=0;
        count++;
        start=end;
        while (*start==' ') start++;
    }
    return count;
}

int LayoutGuardianNarration(const char *text, float scale, float width,
                           char lines[8][512], float *fontSize) {
    float size=fmaxf(22,36*scale);
    int count=WrapDialogue(text,size,width,lines);
    while (count>2 && size>1) {
        size-=.5f;
        count=WrapDialogue(text,size,width,lines);
    }
    if (count==2) {
        float best=width;
        for (const char *space=strchr(text,' '); space; space=strchr(space+1,' ')) {
            char left[512];
            size_t length=(size_t)(space-text);
            memcpy(left,text,length);
            left[length]=0;
            float a=MeasureGameTextEx(left,size,false).x;
            float b=MeasureGameTextEx(space+1,size,false).x;
            if (a<=width && b<=width && fabsf(a-b)<best) {
                best=fabsf(a-b);
                snprintf(lines[0],512,"%s",left);
                snprintf(lines[1],512,"%s",space+1);
            }
        }
    }
    *fontSize=size;
    return count;
}

float GuardianNarrationOpacity(const GuardianScene *shot, float elapsed) {
    if (shot->type!=GUARDIAN_NARRATION) return 0;
    float time=elapsed-shot->narrationAt;
    return Ease(time/shot->narrationFade)*
           Ease((shot->narrationDuration-time)/shot->narrationFade);
}

static void DrawNarration(const GuardianScene *shot, Rectangle canvas, float scale, float elapsed) {
    float alpha=GuardianNarrationOpacity(shot,elapsed);
    if (alpha<=0) return;
    char lines[8][512];
    float size;
    int count=LayoutGuardianNarration(shot->narration,scale,canvas.width-240*scale,lines,&size);
    float top=canvas.y+canvas.height-52*scale-count*size*1.22f-18*scale;
    float gradientTop=top-48*scale;
    DrawRectangleGradientV((int)canvas.x,(int)gradientTop,(int)canvas.width,
        (int)(canvas.y+canvas.height-gradientTop),BLANK,Fade(BLACK,.87f*alpha));
    for (int i=0; i<count; i++) {
        float width=MeasureGameTextEx(lines[i],size,false).x;
        Vector2 pos={canvas.x+(canvas.width-width)/2,top+i*size*1.22f};
        DrawGameTextEx(lines[i],(Vector2){pos.x+1.5f,pos.y+2},size,Fade(BLACK,.85f*alpha),false);
        DrawGameTextEx(lines[i],pos,size,Fade((Color){255,237,207,255},alpha),false);
    }
}

void DrawGuardianCutscene(const GuardianCutscene *scene) {
    if (scene->finished || !scene->count) return;
    const GuardianScene *shot=&scene->scenes[scene->current];
    float scale=fminf(GetScreenWidth()/1280.0f,GetScreenHeight()/720.0f);
    Rectangle canvas={(GetScreenWidth()-1280*scale)/2,(GetScreenHeight()-720*scale)/2,1280*scale,720*scale};
    BeginScissorMode((int)canvas.x,(int)canvas.y,(int)canvas.width,(int)canvas.height);
    float enter=shot->fadeIn>0?Ease(scene->elapsed/shot->fadeIn):1;
    if (scene->current>0 && shot->transitionIn==CINEMATIC_CROSSFADE && enter<1) {
        DrawShot(scene,scene->current-1,canvas,scene->scenes[scene->current-1].duration,1);
        DrawShot(scene,scene->current,canvas,scene->elapsed,enter);
    } else DrawShot(scene,scene->current,canvas,scene->elapsed,1);
    if (shot->transitionIn==CINEMATIC_FADE || shot->transitionIn==CINEMATIC_WHITE || scene->current==0)
        DrawRectangleRec(canvas,Fade(shot->transitionIn==CINEMATIC_WHITE?WHITE:BLACK,1-enter));
    DrawNarration(shot,canvas,scale,scene->elapsed);
    if (shot->type==GUARDIAN_DIALOGUE) {
        Rectangle box={canvas.x+52*scale,canvas.y+508*scale,1176*scale,155*scale};
        float textSize=fmaxf(21,30*scale);
        float speakerSize=fmaxf(22,31*scale);
        DrawRectangleRec(box,(Color){24,11,21,241});
        DrawRectangleLinesEx(box,1.4f*scale,(Color){161,98,65,255});
        DrawGameTextEx(shot->speaker,(Vector2){box.x+24*scale,box.y+10*scale},speakerSize,
                       (Color){246,205,141,255},false);
        char lines[8][512], visible[512];
        int count=WrapDialogue(shot->dialogue,textSize,box.width-48*scale,lines);
        int remaining=(int)scene->letters;
        for (int i=0; i<count; i++) {
            int letters=GetCodepointCount(lines[i]);
            VisibleDialogue(lines[i],remaining<0?0:remaining,visible);
            DrawGameTextEx(visible,(Vector2){box.x+24*scale,box.y+52*scale+i*textSize*1.15f},textSize,
                           (Color){255,240,219,255},false);
            remaining-=letters+1;
        }
    }
    if (shot->title[0]) {
        Rectangle band={canvas.x,canvas.y+440*scale,canvas.width,160*scale};
        DrawRectangleRec(band,Fade((Color){18,9,22,255},.9f*enter));
        DrawGameTextIn(shot->title,(Rectangle){band.x+40*scale,band.y+20*scale,band.width-80*scale,58*scale},
                       48*scale,Fade((Color){255,232,190,255},enter),false);
        DrawGameTextIn(shot->subtitle,(Rectangle){band.x+40*scale,band.y+85*scale,band.width-80*scale,36*scale},
                       27*scale,Fade(RAYWHITE,enter),false);
    }
    Rectangle hint={canvas.x+24*scale,canvas.y+680*scale,canvas.width-48*scale,28*scale};
    DrawRectangleRec(hint,Fade(BLACK,.78f));
    const char *controls;
    if (shot->type==GUARDIAN_DIALOGUE)
        controls=IsGamepadAvailable(0)?"A  Completar / avançar    ·    B  Pular    ·    Start  Pausar":
            "Espaço / Enter  Completar / avançar    ·    Esc  Pular    ·    P  Pausar";
    else controls=IsGamepadAvailable(0)?"A  Avançar    ·    B  Pular    ·    Start  Pausar":
            "Espaço / Enter  Avançar    ·    Esc  Pular    ·    P  Pausar";
    DrawGameTextIn(controls,hint,fmaxf(16,18*scale),(Color){244,226,202,255},false);
    if (!shot->dialogue[0] && shot->fadeOut>0) {
        float exit=Ease((scene->elapsed-shot->duration+shot->fadeOut)/shot->fadeOut);
        if (shot->transitionOut==CINEMATIC_FADE || shot->transitionOut==CINEMATIC_WHITE)
            DrawRectangleRec(canvas,Fade(shot->transitionOut==CINEMATIC_WHITE?WHITE:BLACK,exit));
    }
    if (scene->paused) {
        DrawRectangleRec(canvas,Fade(BLACK,.72f));
        DrawGameTextIn("Pausa",(Rectangle){canvas.x,canvas.y+280*scale,canvas.width,60*scale},
                       52*scale,(Color){255,232,190,255},true);
        DrawGameTextIn("P ou Start para continuar",(Rectangle){canvas.x,canvas.y+350*scale,canvas.width,40*scale},
                       26*scale,RAYWHITE,false);
    }
    EndScissorMode();
}
