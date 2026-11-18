#include "guardian_cutscene.h"
#include "game_text.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void CheckMotion(GuardianCutscene *scene) {
    SetWindowSize(800,450);
    RenderTexture2D target=LoadRenderTexture(800,450);
    Color *pixels[2];
    for (int frame=0; frame<2; frame++) {
        scene->elapsed=1.0f+frame*.7f;
        BeginTextureMode(target);
        ClearBackground(BLACK);
        DrawGuardianCutscene(scene);
        EndTextureMode();
        Image shot=LoadImageFromTexture(target.texture);
        pixels[frame]=LoadImageColors(shot);
        assert(pixels[frame]);
        UnloadImage(shot);
    }
    int changed=0;
    for (int i=0; i<800*450; i++)
        if (memcmp(&pixels[0][i],&pixels[1][i],sizeof(Color))) changed++;
    assert(changed>5000);
    UnloadImageColors(pixels[0]);
    UnloadImageColors(pixels[1]);
    UnloadRenderTexture(target);
    scene->elapsed=0;
}

static void Capture(const GuardianCutscene *scene, int width, const char *label) {
    int height=width*9/16;
    SetWindowSize(width,height);
    RenderTexture2D target=LoadRenderTexture(width,height);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    DrawGuardianCutscene(scene);
    EndTextureMode();
    Image shot=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&shot);
    assert(ExportImage(shot,TextFormat("audit/guardian_cinematic_%s_%d.png",label,width)));
    UnloadImage(shot);
    UnloadRenderTexture(target);
}

static void CheckSceneProgress(GuardianCutscene *scene) {
    int dialogues=0;
    const char *expected[] = {
        "Minhas raízes... já não obedecem à minha vontade.",
        "A floresta que jurei proteger... agora sofre pelas minhas próprias mãos.",
        "Então ainda existe algo de você aí dentro...",
        "Esse selo não está protegendo a clareira. Está aprisionando seu guardião.",
        "Afaste-se... antes que eu também o destrua.",
        "Não. Já perdi companheiros demais para essa maldição.",
        "Se ainda resta vida sob essa corrupção...",
        "Então, pela luz que me guia, eu vou libertá-lo.",
        "Então prove... que sua luz é mais forte que minhas raízes."
    };
    while (!scene->finished) {
        int current=scene->current;
        GuardianScene *shot=&scene->scenes[current];
        if (shot->waitsForInput) {
            assert(dialogues<9 && !strcmp(shot->dialogue,expected[dialogues++]));
            UpdateGuardianCutscene(scene,.02f,(GuardianCutsceneInput){0});
            assert(scene->letters>0 && scene->letters<GetCodepointCount(shot->dialogue));
            float elapsed=scene->elapsed, letters=scene->letters;
            UpdateGuardianCutscene(scene,.1f,(GuardianCutsceneInput){.pause=true});
            UpdateGuardianCutscene(scene,10,(GuardianCutsceneInput){.advance=true});
            assert(scene->paused && scene->elapsed==elapsed && scene->letters==letters);
            assert(scene->current==current);
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.pause=true});
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.advance=true});
            assert(scene->current==current);
            assert(scene->letters==GetCodepointCount(shot->dialogue));
            for (int i=0; i<700; i++) UpdateGuardianCutscene(scene,.1f,(GuardianCutsceneInput){0});
            assert(scene->current==current && !scene->finished);
            if (dialogues==2) {
                Capture(scene,1280,"dialogue");
                Capture(scene,800,"dialogue");
            }
            if (dialogues==4) Capture(scene,800,"protagonist");
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.advance=true});
        } else {
            if (current==6 && IsAudioDeviceReady()) {
                assert(shot->cue.stream.buffer && !scene->cueStarted);
                UpdateGuardianCutscene(scene,.01f,(GuardianCutsceneInput){0});
                assert(!scene->cueStarted && !IsSoundPlaying(shot->cue));
                SetGuardianCutscenePaused(scene,true);
                float frozen=scene->elapsed;
                UpdateGuardianCutscene(scene,.1f,(GuardianCutsceneInput){0});
                assert(scene->elapsed==frozen && !scene->cueStarted);
                SetGuardianCutscenePaused(scene,false);
                UpdateGuardianCutscene(scene,.05f,(GuardianCutsceneInput){0});
                assert(scene->cueStarted && IsSoundPlaying(shot->cue));
                SetGuardianCutscenePaused(scene,true);
                assert(scene->cuePaused && !IsSoundPlaying(shot->cue));
                SetGuardianCutscenePaused(scene,false);
                assert(!scene->cuePaused && IsSoundPlaying(shot->cue));
            }
            bool capture= current==0 || current==6 || current==9 || shot->title[0];
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.pause=true});
            float elapsed=scene->elapsed;
            UpdateGuardianCutscene(scene,1,(GuardianCutsceneInput){0});
            assert(scene->elapsed==elapsed && scene->paused);
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.pause=true});
            while (scene->current==current && !scene->finished) {
                UpdateGuardianCutscene(scene,.016f,(GuardianCutsceneInput){0});
                if (capture && scene->elapsed>=1.0f) {
                    Capture(scene,1280,shot->id);
                    Capture(scene,800,shot->id);
                    capture=false;
                }
            }
        }
    }
    assert(dialogues==9 && scene->current==scene->count-1);
    assert(!scene->music.stream.buffer || !IsMusicStreamPlaying(scene->music));
    for (int i=0; i<scene->count; i++)
        if (scene->scenes[i].cue.stream.buffer) assert(!IsSoundPlaying(scene->scenes[i].cue));
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"Guardian cinematic audit");
    assert(IsWindowReady());
    InitAudioDevice();
    LoadGameFonts();
    GuardianCutscene scene={0};
    assert(InitGuardianCutscene(&scene,.12f,0));
    assert(scene.count==24 && scene.artCount==16);
    for (int i=0; i<scene.count; i++) {
        assert(scene.scenes[i].artIndex>=0);
        assert(scene.scenes[i].artIndex<scene.artCount);
        assert(scene.art[scene.scenes[i].artIndex].id!=0);
        assert(!strcmp(scene.scenes[i].music,"assets/cutscenes/boss_clareira/audio/trilha.ogg"));
    }
    if (IsAudioDeviceReady()) {
        int effects=0;
        for (int i=0; i<scene.count; i++) if (scene.scenes[i].cue.stream.buffer) effects++;
        assert(effects==4);
        assert(scene.music.stream.buffer && scene.music.looping);
        assert(GetMusicTimeLength(scene.music)>57 && GetMusicTimeLength(scene.music)<59);
        SetGuardianCutscenePaused(&scene,true);
        assert(scene.paused && !IsMusicStreamPlaying(scene.music));
        SetGuardianCutscenePaused(&scene,false);
        assert(!scene.paused && IsMusicStreamPlaying(scene.music));
    }
    assert(!strcmp(scene.scenes[0].id,"depois_da_batalha"));
    assert(!strcmp(scene.scenes[scene.count-1].title,"GUARDIÃO DAS RAÍZES"));
    assert(!strcmp(scene.scenes[scene.count-1].subtitle,"O PROTETOR APRISIONADO"));
    CheckMotion(&scene);
    CheckSceneProgress(&scene);
    UnloadGuardianCutscene(&scene);
    assert(scene.artCount==0 && !scene.music.stream.buffer);

    assert(InitGuardianCutscene(&scene,0,0));
    for (int i=0; i<12; i++)
        UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.advance=true});
    assert(scene.current==12 && scene.scenes[scene.current].waitsForInput);
    UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
    assert(scene.letters>0 && scene.letters<GetCodepointCount(scene.scenes[scene.current].dialogue));
    UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){.pause=true});
    UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
    assert(scene.finished && !scene.paused);
    assert(!scene.music.stream.buffer || !IsMusicStreamPlaying(scene.music));
    for (int i=0; i<scene.count; i++)
        if (scene.scenes[i].cue.stream.buffer) assert(!IsSoundPlaying(scene.scenes[i].cue));
    UpdateGuardianCutscene(&scene,1,(GuardianCutsceneInput){.advance=true});
    assert(scene.finished && scene.current==12);
    UnloadGuardianCutscene(&scene);
    UnloadGuardianCutscene(&scene);
    UnloadGameFonts();
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    puts("Guardian cinematic sequence, dialogue, pause, skip and cleanup passed.");
    return 0;
}
