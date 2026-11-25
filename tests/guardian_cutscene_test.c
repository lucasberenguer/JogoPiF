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
    int dialogues=0, narrations=0;
    const char *narrator[]={
        "Nem mesmo a morte lhes concedera descanso.",
        "E àquele que sobreviveu... restou o fardo de erguer a espada contra seus próprios irmãos.",
        "Além dos corpos, estendia-se uma floresta que já não conhecia a luz.",
        "Ali, até o silêncio parecia guardar um lamento.",
        "Sob raízes antigas, um símbolo profano ainda pulsava.",
        "Não era uma bênção. Era uma sentença.",
        "Então, a terra gemeu.",
        "E das profundezas ergueu-se aquele que outrora fora o escudo da floresta.",
        "Houve um tempo em que suas raízes não conheciam a violência.",
        "Sob sua sombra, a vida florescia sem temor.",
        "Mas até os mais antigos juramentos podem ser profanados.",
        "E o protetor tornou-se prisioneiro daquilo que jurara combater.",
        "Por trás da madeira retorcida, uma vontade ainda resistia.",
        "E diante dela, uma última promessa seria feita.",
        "Mas a maldição não permitiria que seu cativo partisse em paz.",
        "Naquela noite, não se ergueram dois inimigos.",
        "Mas duas vontades... contra uma única maldição."
    };
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
        assert(!(shot->narration[0] && shot->dialogue[0]));
        if (shot->type==GUARDIAN_NARRATION) {
            assert(narrations<17 && !strcmp(shot->narration,narrator[narrations++]));
            assert(!shot->waitsForInput && !shot->speaker[0]);
            float start=shot->narrationAt, end=start+shot->narrationDuration;
            assert(GuardianNarrationOpacity(shot,start)==0);
            assert(GuardianNarrationOpacity(shot,end)==0);
            assert(GuardianNarrationOpacity(shot,start+.25f)>.4f);
            assert(GuardianNarrationOpacity(shot,start+.5f)==1);
            assert(end<=shot->duration);
            for (int width=800; width<=1280; width+=480) {
                float scale=width/1280.0f, size;
                char lines[8][512], rebuilt[512]={0};
                int count=LayoutGuardianNarration(shot->narration,scale,width-240*scale,lines,&size);
                assert(count>0 && count<=2 && size>=20);
                for (int i=0; i<count; i++) {
                    assert(MeasureGameTextEx(lines[i],size,GAME_TEXT_CUTSCENE).x<=width-240*scale);
                    if (i) strcat(rebuilt," ");
                    strcat(rebuilt,lines[i]);
                }
                assert(!strcmp(rebuilt,shot->narration));
            }
        }
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
            if (!strcmp(shot->id,"raizes_rompem_o_solo") && IsAudioDeviceReady()) {
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
            bool capture=shot->narration[0] || shot->title[0];
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.pause=true});
            float elapsed=scene->elapsed;
            UpdateGuardianCutscene(scene,1,(GuardianCutsceneInput){0});
            assert(scene->elapsed==elapsed && scene->paused);
            UpdateGuardianCutscene(scene,0,(GuardianCutsceneInput){.pause=true});
            while (scene->current==current && !scene->finished) {
                UpdateGuardianCutscene(scene,.016f,(GuardianCutsceneInput){0});
                if (capture && scene->elapsed>=1.7f) {
                    Capture(scene,1280,shot->id);
                    Capture(scene,800,shot->id);
                    capture=false;
                }
            }
        }
    }
    assert(dialogues==9 && narrations==17 && scene->current==scene->count-1);
    assert(!scene->music.stream.buffer || !IsMusicStreamPlaying(scene->music));
    for (int i=0; i<scene->count; i++)
        if (scene->scenes[i].cue.stream.buffer) assert(!IsSoundPlaying(scene->scenes[i].cue));
}

static void CheckAftermath(void) {
    const char *dialogues[]={
        "Tanto tempo... aprisionado às minhas próprias raízes...",
        "Acabou. Você está livre.",
        "Livre... depois de tantos invernos...",
        "Sob estas terras... repousam aqueles que foram esquecidos.",
        "O cemitério?",
        "Foi lá que a morte deixou de ser um fim... e se tornou uma corrente.",
        "Siga as raízes... e encontrará a origem deste tormento.",
        "Então é para lá que devo ir...",
        "Que esta luz alcance aqueles que ainda podem ser salvos.",
        "Os mortos não descansam... enquanto seus nomes forem lembrados."
    };
    const char *speakers[]={"Guardião","Você","Guardião","Guardião","Você",
        "Guardião","Guardião","Você","Você","Voz desconhecida"};
    int spoken=0;
    for (int part=0; part<2; part++) {
        GuardianCutscene scene={0};
        assert(InitGuardianAftermath(&scene,part,.12f,.12f));
        assert(scene.count==(part ? 7 : 16));
        assert(scene.artCount==(part ? 5 : 6));
        if (part) {
            GuardianScene *card=&scene.scenes[scene.count-1];
            assert(card->chapterCard && !strcmp(card->chapter,"CAPÍTULO II"));
            assert(!strcmp(card->title,"O Cemitério") && !strcmp(card->subtitle,"dos Esquecidos"));
            assert(!strcmp(card->tagline,"Onde a morte perdeu seu silêncio"));
            assert(card->duration-card->fadeIn-card->fadeOut>=3);
        }
        void *music=scene.music.stream.buffer;
        if (IsAudioDeviceReady()) {
            assert(music && scene.music.looping);
            assert(GetMusicTimeLength(scene.music)>57 && GetMusicTimeLength(scene.music)<59);
            if (part) assert(GetMusicTimePlayed(scene.music)>39);
        }
        while (!scene.finished) {
            int current=scene.current;
            GuardianScene *shot=&scene.scenes[current];
            assert(shot->artIndex>=0 && !shot->narration[0]);
            if (shot->audioEvent[0] && IsAudioDeviceReady()) assert(shot->cue.stream.buffer);
            if (shot->dialogue[0]) {
                assert(spoken<10 && !strcmp(shot->dialogue,dialogues[spoken]));
                assert(!strcmp(shot->speaker,speakers[spoken++]));
                UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.advance=true});
                assert(scene.current==current && scene.letters==GetCodepointCount(shot->dialogue));
                for (int i=0;i<700;i++) UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
                assert(scene.current==current && !scene.finished);
                scene.elapsed=1.7f;
            }
            bool capture=shot->dialogue[0] || shot->chapterCard || shot->effects&CINEMATIC_PURIFY;
            if (capture) {
                scene.elapsed=1.7f;
                Capture(&scene,1280,shot->id);
                Capture(&scene,800,shot->id);
            }
            SetGuardianCutscenePaused(&scene,true);
            float frozen=scene.elapsed;
            UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){.advance=true});
            assert(scene.elapsed==frozen && scene.current==current);
            SetGuardianCutscenePaused(&scene,false);
            if (shot->dialogue[0]) {
                UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.advance=true});
                if (shot->transitionOut==CINEMATIC_FADE) assert(scene.exiting && scene.current==current);
            }
            int steps=0;
            while (!scene.finished && scene.current==current) {
                UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
                assert(++steps<700);
            }
            assert(scene.music.stream.buffer==music); /* No restart between shots. */
        }
        assert(!scene.skipped && !scene.paused);
        assert(!music || !IsMusicStreamPlaying(scene.music));
        for (int i=0;i<scene.count;i++)
            if (scene.scenes[i].cue.stream.buffer) assert(!IsSoundPlaying(scene.scenes[i].cue));
        UnloadGuardianCutscene(&scene);
        assert(InitGuardianAftermath(&scene,part,0,0));
        SetGuardianCutscenePaused(&scene,true);
        UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
        assert(scene.finished && scene.skipped && !scene.paused);
        assert(!scene.music.stream.buffer || !IsMusicStreamPlaying(scene.music));
        for (int i=0;i<scene.count;i++)
            if (scene.scenes[i].cue.stream.buffer) assert(!IsSoundPlaying(scene.scenes[i].cue));
        UnloadGuardianCutscene(&scene);
    }
    assert(spoken==10);
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
    assert(scene.count==30 && scene.artCount==16);
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
    while (!scene.scenes[scene.current].waitsForInput)
        UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.advance=true});
    int dialogueIndex=scene.current;
    UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
    assert(scene.letters>0 && scene.letters<GetCodepointCount(scene.scenes[scene.current].dialogue));
    UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){.pause=true});
    UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
    assert(scene.finished && !scene.paused);
    assert(!scene.music.stream.buffer || !IsMusicStreamPlaying(scene.music));
    for (int i=0; i<scene.count; i++)
        if (scene.scenes[i].cue.stream.buffer) assert(!IsSoundPlaying(scene.scenes[i].cue));
    UpdateGuardianCutscene(&scene,1,(GuardianCutsceneInput){.advance=true});
    assert(scene.finished && scene.current==dialogueIndex);
    UnloadGuardianCutscene(&scene);
    UnloadGuardianCutscene(&scene);

    assert(InitGuardianCutscene(&scene,0,0));
    UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){.pause=true});
    UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
    assert(scene.finished && !scene.paused);
    assert(!scene.music.stream.buffer || !IsMusicStreamPlaying(scene.music));
    UnloadGuardianCutscene(&scene);
    CheckAftermath();
    for (int skip=0;skip<2;skip++) {
        assert(InitCoveiroCutscene(&scene,.12f,.12f));
        assert(scene.count==11 && scene.artCount==9);
        if (IsAudioDeviceReady()) assert(scene.music.stream.buffer);
        float duration=0;
        for (int i=0;i<scene.count;i++) {
            assert(scene.scenes[i].artIndex>=0);
            if (IsAudioDeviceReady()) assert(scene.scenes[i].cue.stream.buffer);
            duration+=scene.scenes[i].duration;
        }
        assert(duration==43);
        if (skip) UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
        else for (int i=0;i<500 && !scene.finished;i++)
            UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
        assert(scene.finished && scene.skipped==(skip!=0));
        UnloadGuardianCutscene(&scene);
    }
    assert(InitVillageCutscene(&scene,.12f,.12f));
    assert(scene.count==5 && scene.artCount==4);
    const char *villageImages[]={"cha_de_alegria.png","febre.png","selo.png","presente.png","presente.png"};
    for (int i=0;i<scene.count;i++) {
        const GuardianScene *shot=&scene.scenes[i];
        assert(!strcmp(shot->image,villageImages[i]));
        assert(shot->type==GUARDIAN_NARRATION && shot->narration[0] && !shot->waitsForInput);
        float readingTime=shot->narrationAt+shot->narrationDuration/2;
        assert(GuardianNarrationOpacity(shot,readingTime)==1);
        scene.current=i;
        scene.elapsed=readingTime;
        Capture(&scene,800,shot->id);
    }
    scene.current=0;
    scene.elapsed=0;
    assert(!strcmp(scene.scenes[3].narration,"Eu só queria salvá-los..."));
    assert(!strcmp(scene.scenes[4].narration,"Mas fui eu quem condenou todos eles."));
    if (IsAudioDeviceReady()) assert(scene.music.stream.buffer && scene.scenes[1].cue.stream.buffer);
    for (int i=0;i<400 && !scene.finished;i++)
        UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
    assert(scene.finished && !scene.skipped);
    UnloadGuardianCutscene(&scene);
    assert(InitBonelandCutscene(&scene,.12f,.12f));
    assert(scene.count==7 && scene.artCount>=4);
    const float bonelandDurations[] = {5,5,4,6,5,4,6};
    for (int i=0; i<7; i++) {
        const char *image = TextFormat("cena_%02d.png",i+1);
        assert(strcmp(scene.scenes[i].image,image)==0);
        bool exists = FileExists(TextFormat("assets/cutscenes/terra_dos_ossos/images/%s",image));
        assert((scene.scenes[i].artIndex>=0)==exists);
        assert(scene.scenes[i].duration==bonelandDurations[i]);
        assert(strcmp(scene.scenes[0].music,scene.scenes[i].music)==0);
    }
    assert(scene.scenes[0].narration[0] && scene.scenes[6].narration[0]);
    assert(strcmp(scene.scenes[5].audioEvent,"rugido")==0);
    if (IsAudioDeviceReady()) assert(scene.scenes[5].cue.stream.buffer);
    for (int i=0; i<400 && !scene.finished; i++)
        UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){0});
    assert(scene.finished && !scene.skipped);
    UnloadGuardianCutscene(&scene);
    assert(InitBonelandCutscene(&scene,0,0));
    UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
    assert(scene.finished && scene.skipped);
    UnloadGuardianCutscene(&scene);
    assert(InitAngelCutscene(&scene,.12f,.12f));
    assert(scene.count==52 && scene.artCount==15 && scene.mouseAdvance);
    if (IsAudioDeviceReady()) assert(scene.music.stream.buffer);
    int angelLines=0;
    for (int step=0;step<200 && !scene.finished;step++) {
        GuardianScene *shot=&scene.scenes[scene.current];
        assert(shot->artIndex>=0);
        if (shot->waitsForInput && scene.letters==0 && !scene.exiting) angelLines++;
        UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){.advance=true});
    }
    assert(scene.finished && !scene.skipped && angelLines==46);
    UnloadGuardianCutscene(&scene);
    assert(InitAngelCutscene(&scene,0,0));
    UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
    assert(scene.finished && scene.skipped);
    UnloadGuardianCutscene(&scene);
    for (int skip=0; skip<2; skip++) {
        assert(InitIgrisCutscene(&scene,.12f,.12f));
        assert(scene.artCount==5 && scene.count==9);
        if (IsAudioDeviceReady()) assert(scene.music.stream.buffer);
        for (int i=0; i<scene.count; i++) {
            const GuardianScene *shot = &scene.scenes[i];
            if (i==2) {
                assert(!strcmp(shot->audioEvent,"piscada") && shot->cueAt==0);
                if (IsAudioDeviceReady()) assert(shot->cue.stream.buffer);
            } else assert(!shot->audioEvent[0]);
        }
        const char *speakers[] = {"Você", "Igris", "Você", "Igris"};
        const char *dialogue[] = {
            "Seus cavaleiros caíram defendendo essa escadaria.", "Por minha ordem.",
            "E foi ele quem mandou você descer?",
            "Meu senhor só me pediu que guardasse o portão. Descer foi ideia minha."
        };
        for (int i=0; i<4; i++) {
            const GuardianScene *line = &scene.scenes[4+i];
            assert(line->waitsForInput && line->scoreGain < scene.scenes[3].scoreGain);
            assert(!strcmp(line->speaker,speakers[i]) && !strcmp(line->dialogue,dialogue[i]));
        }
        if (skip) UpdateGuardianCutscene(&scene,0,(GuardianCutsceneInput){.skip=true});
        else {
            bool blink=false;
            int previous=-1;
            for (int i=0; i<500 && !scene.finished; i++) {
                if (scene.current==2) {
                    assert(scene.scenes[2].chapterCard && !scene.music.stream.buffer);
                    if (IsAudioDeviceReady()) assert(scene.cueStarted);
                    blink=true;
                }
                if (scene.current!=previous) {
                    if (scene.current==4 || scene.current==8) {
                        GuardianCutscene preview = scene;
                        preview.elapsed = .8f;
                        preview.letters = GetCodepointCount(scene.scenes[scene.current].dialogue);
                        Capture(&preview,800,scene.current==4 ? "igris_dialogue" : "igris_challenge");
                    }
                    previous=scene.current;
                }
                UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){
                    .advance=scene.scenes[scene.current].waitsForInput});
            }
            assert(blink);
        }
        assert(scene.finished && scene.skipped==(skip!=0));
        assert(!scene.music.stream.buffer || !IsMusicStreamPlaying(scene.music));
        UnloadGuardianCutscene(&scene);
    }
    assert(InitIgrisAftermath(&scene,.12f,.12f));
    assert(scene.count==11 && scene.artCount>=5);
    const char *aftermathSpeakers[]={"Igris","Você","Igris","Você","Igris","Igris"};
    int aftermathLines=0;
    for (int i=0; i<scene.count; i++) {
        assert(scene.scenes[i].artIndex>=0);
        assert(scene.scenes[i].musicLoop==(i<9));
        assert(!scene.scenes[i].audioEvent[0] && !scene.scenes[i].cue.stream.buffer);
        if (scene.scenes[i].waitsForInput) {
            assert(aftermathLines<6 && !strcmp(scene.scenes[i].speaker,aftermathSpeakers[aftermathLines]));
            aftermathLines++;
        }
    }
    assert(aftermathLines==6 && !strcmp(scene.scenes[7].id,"pausa"));
    bool soundReady=IsAudioDeviceReady();
    if (soundReady) assert(scene.music.stream.buffer && scene.music.looping);
    for (int i=0; i<300 && !scene.finished; i++) {
        if (scene.current==1 && scene.letters==0) {
            GuardianCutscene preview=scene;
            preview.elapsed=.8f;
            preview.letters=GetCodepointCount(scene.scenes[1].dialogue);
            Capture(&preview,800,"igris_aftermath_monologue");
        }
        if (soundReady && scene.current>=9) assert(scene.music.stream.buffer && !scene.music.looping);
        UpdateGuardianCutscene(&scene,.1f,(GuardianCutsceneInput){.advance=scene.scenes[scene.current].waitsForInput});
    }
    assert(scene.finished && !scene.skipped);
    if (soundReady) assert(!IsMusicStreamPlaying(scene.music));
    UnloadGuardianCutscene(&scene);
    UnloadGameFonts();
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    puts("Guardian cinematic sequence, dialogue, pause, skip and cleanup passed.");
    return 0;
}
