/* Exercise the real screen loop with deterministic input and an isolated checkpoint. */
#include "raylib.h"
#include "menu.h"
#include "guardian_cutscene.h"
#include "encounter.h"
#include <assert.h>
#include <stdio.h>

static int frames, mode, liberated, transported, cinematicFrames;
static bool answered;
static Encounter *active;
static GameAudio *activeAudio;
static Vector2 igrisApproach;
static int igrisStarts;
static int aftermathStarts, aftermathLines, igrisDefeatFrames;
static bool igrisDefeated, igrisCardCleared;
static bool skipBonelandPassage;
static int checkedSceneStarts, checkedLastScene;
static const struct {
    int map, scenes;
    const char *firstScene;
} checkedScenes[]={
    {0,30,"depois_da_batalha"},
    {1,11,"silencio"},
    {BONELAND_MAP,7,"alem_da_capela"},
    {2,5,"cha_de_alegria"},
    {3,52,"fachada"},
};
enum { FIRST_NEW_MODE = 9 };
static void TestWindow(int w,int h,const char *title);
static void IgnoreResize(int w,int h);
static void IgnoreToggle(void);
static bool TestFocused(void);
static void IgnoreFps(int fps);
static bool TestClose(void);
static bool TestKey(int key);
static bool TestKeyDown(int key);
static float TestDelta(void);
static MenuAction TestMenu(Menu *menu);
static void TestBeginEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,RunProgress progress,Vector2 p);
static bool TestEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health);
static void TestCinematic(GuardianCutscene *scene,float dt,GuardianCutsceneInput input);
static void TestLiberation(Encounter *e);
static bool TestAltar(Encounter *e,const Clearing *map,Vector2 *p,bool interact);
static bool TestPassage(Encounter *e,EnemyGroup *enemies,Clearing *map,Vector2 *p,int health);
#define main ActualGameMain
#define InitWindow TestWindow
#define SetWindowSize IgnoreResize
#define ToggleFullscreen IgnoreToggle
#define ToggleGameFullscreen IgnoreToggle
#define IsWindowFocused TestFocused
#define SetTargetFPS IgnoreFps
#define WindowShouldClose TestClose
#define IsKeyPressed TestKey
#define IsKeyDown TestKeyDown
#define GetFrameTime TestDelta
#define UpdateMenu TestMenu
#define BeginEncounter TestBeginEncounter
#define UpdateEncounter TestEncounter
#define UpdateGuardianCutscene TestCinematic
#define CompleteGuardianLiberation TestLiberation
#define BeginGuardianPassage(e,map,p,input) TestAltar(e,map,&(p),input)
#define CompleteGuardianPassage TestPassage
#include "game.c"
#undef main
#undef InitWindow
#undef SetWindowSize
#undef ToggleFullscreen
#undef ToggleGameFullscreen
#undef IsWindowFocused
#undef SetTargetFPS
#undef WindowShouldClose
#undef IsKeyPressed
#undef IsKeyDown
#undef GetFrameTime
#undef UpdateMenu
#undef BeginEncounter
#undef UpdateEncounter
#undef UpdateGuardianCutscene
#undef CompleteGuardianLiberation
#undef BeginGuardianPassage
#undef CompleteGuardianPassage

static void TestWindow(int w,int h,const char *title) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(mode >= 2 ? 640 : w, mode >= 2 ? 360 : h, title);
}
static void IgnoreResize(int w,int h) {(void)w;(void)h;}
static void IgnoreToggle(void) {}
static bool TestFocused(void) {return true;}
static void IgnoreFps(int fps) {(void)fps;}
static float TestDelta(void) {return .05f;}
static bool TestClose(void) {return ++frames>1200 || transported>0;}
static bool TestKey(int key) {return key==KEY_E && liberated>0;}
static bool TestKeyDown(int key) {
    return mode >= 2 && mode < FIRST_NEW_MODE && key == KEY_W &&
           (!active || mode >= 3 || active->progress.map == BONELAND_MAP);
}
static MenuAction TestMenu(Menu *menu) {
    activeAudio=&menu->audio;
    RunProgress progress=NewRunProgress(2);
    progress.stage=CLEARING_BOSS_STAGE;
    progress.guardianIntroSeen=true;
    if (mode >= FIRST_NEW_MODE) {
        progress.map=checkedScenes[(mode-FIRST_NEW_MODE)/2].map;
        if (progress.map<2 || progress.map==BONELAND_MAP)
            progress.stage=CampaignWaveCount(progress.map);
        progress.guardianIntroSeen=false;
    } else if (mode >= 2) {
        progress.map = mode == 2 ? BONELAND_MAP : TEMPLAR_FIELD_MAP;
        progress.stage = CLEARING_COMPLETE_STAGE;
        if (mode >= 5) progress.stage = CLEARING_BOSS_STAGE;
        if (mode == 8) progress.igrisAftermathPending = true;
        progress.guardianAftermath = GUARDIAN_PASSAGE_COMPLETE;
        progress.health = 83;
        progress.weaponLevel[2] = 2;
    }
    assert(SaveClearingCheckpoint(&progress));
    return MENU_CONTINUE;
}
static void TestBeginEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,RunProgress progress,Vector2 p) {
    BeginEncounter(e,enemies,map,progress,p);
    active=e;
}

static bool InteractInEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health) {
    // AutomationEvent exposes numeric event types in raylib 5.5 (INPUT_KEY_UP and INPUT_KEY_DOWN).
    enum { KEY_UP_EVENT=1, KEY_DOWN_EVENT=2 };
    PlayAutomationEvent((AutomationEvent){.type=KEY_DOWN_EVENT,.params={KEY_E}});
    assert(IsKeyPressed(KEY_E));
    bool changed=UpdateEncounter(e,enemies,map,p,health);
    PlayAutomationEvent((AutomationEvent){.type=KEY_UP_EVENT,.params={KEY_E}});
    return changed;
}

static bool TestCutsceneEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health) {
    int expectedMap=checkedScenes[(mode-FIRST_NEW_MODE)/2].map;
    assert(e->progress.map==expectedMap);
    if (map->village && e->villageHordeStarted) {
        if (activeAudio->tracks[TRACK_VILLAGE_HORDE].stream.buffer) {
            assert(activeAudio->playing[TRACK_VILLAGE_HORDE]);
            assert(!activeAudio->playing[TRACK_COMBAT]);
        }
        transported=1;
        return false;
    }
    if (EncounterPaused(e) && e->phase!=ENCOUNTER_REWARD && e->phase!=ENCOUNTER_LETTER)
        return UpdateEncounter(e,enemies,map,p,health);

    RunProgress saved={0};
    if (map->chapel) {
        Vector2 altar={map->background.width*.50f,map->background.height*.44f};
        Vector2 exit={map->exit.x+map->exit.width/2,map->exit.y+map->exit.height/2};
        if (!e->chapelAltarDone) {
            assert(!checkedSceneStarts && !CanEnterBasement(e,map,exit,false));
            bool changed=InteractInEncounter(e,enemies,map,altar,health);
            assert(e->phase==ENCOUNTER_ANGEL_CINEMATIC);
            return changed;
        }
        assert(LoadClearingCheckpoint(&saved) && saved.angelSceneCompleted && saved.angelBlessingUses==1);
        assert(CanEnterBasement(e,map,exit,false));
    } else if (map->village) {
        if (e->phase==ENCOUNTER_VILLAGE_LETTER_READY) {
            assert(LoadClearingCheckpoint(&saved) && saved.villageAftermath==VILLAGE_SCENE_PENDING);
            bool changed=InteractInEncounter(e,enemies,map,enemies->list[0].position,health);
            assert(e->phase==ENCOUNTER_VILLAGE_CINEMATIC);
            return changed;
        }
        if (e->phase!=ENCOUNTER_LETTER) {
            enemies->list[0].health=0;
            enemies->list[0].state=ENEMY_DIE;
            return UpdateEncounter(e,enemies,map,p,health);
        }
        assert(LoadClearingCheckpoint(&saved) && saved.villageAftermath==VILLAGE_SCENE_SEEN);
        assert(!e->villageHordeStarted);
        if (activeAudio->tracks[TRACK_VILLAGE_LEADER].stream.buffer) {
            assert(activeAudio->playing[TRACK_VILLAGE_LEADER]);
            assert(!activeAudio->playing[TRACK_VILLAGE_HORDE]);
        }
        assert(InteractInEncounter(e,enemies,map,p,health));
        assert(e->villageHordeStarted && EnemiesRemaining(enemies)==82);
        return true;
    } else {
        if (e->phase==ENCOUNTER_REWARD) {
            assert(ChooseEncounterReward(e,enemies,map,p,health,0));
            assert(e->guardianIntroPending && !EncounterBossVisible(e,enemies));
            return true;
        }
        if (e->progress.stage!=CLEARING_BOSS_STAGE) {
            for (int i=0;i<ENEMY_COUNT;i++) enemies->list[i].state=ENEMY_GONE;
            return UpdateEncounter(e,enemies,map,p,health);
        }
        assert(EncounterBossVisible(e,enemies) && !EncounterPaused(e));
        assert(LoadClearingCheckpoint(&saved) && saved.stage==CLEARING_BOSS_STAGE);
        if (!map->cemetery) assert(saved.guardianIntroSeen);
    }
    assert(checkedSceneStarts==1);
    transported++;
    return false;
}

static bool TestEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health) {
    active=e;
    if (mode >= FIRST_NEW_MODE) return TestCutsceneEncounter(e,enemies,map,p,health);
    if (mode >= 7 && map->necromancerArena) {
        RunProgress saved={0};
        assert(aftermathStarts == 1 && e->phase == ENCOUNTER_FIGHT);
        assert(LoadClearingCheckpoint(&saved) && saved.map == NECROMANCER_MAP);
        assert(saved.stage == CLEARING_BOSS_STAGE && !saved.igrisAftermathPending);
        assert(saved.health == 83 && saved.weaponLevel[2] == 2 && *health == 83);
        assert(enemies->list[0].necromancerForm == 1);
        if (p.y < map->spawn.y - 3) {
            assert(!EncounterPaused(e));
            transported++;
        }
        return UpdateEncounter(e,enemies,map,p,health);
    }
    if (mode >= 7 && e->phase == ENCOUNTER_IGRIS_AFTERMATH) {
        assert(!EncounterBossVisible(e,enemies) && !EnemyCanBeHit(&enemies->list[0]));
        return UpdateEncounter(e,enemies,map,p,health);
    }
    if (mode==7 && igrisDefeated) {
        Enemy *boss=&enemies->list[0];
        assert(e->phase==ENCOUNTER_FIGHT && boss->state==ENEMY_DIE && !EnemyCanBeHit(boss));
        bool changed=UpdateEncounter(e,enemies,map,p,health);
        if (boss->stateTime < IGRIS_DEFEAT_DURATION) {
            assert(e->phase==ENCOUNTER_FIGHT);
            igrisDefeatFrames++;
        } else assert(e->phase==ENCOUNTER_IGRIS_AFTERMATH);
        return changed;
    }
    if (mode >= 5) {
        assert(map->templarField && *health==83);
        if (e->phase==ENCOUNTER_IGRIS_APPROACH) {
            assert(!EncounterBossVisible(e,enemies) && EnemiesRemaining(enemies)==0);
            bool changed=UpdateEncounter(e,enemies,map,p,health);
            if (e->phase==ENCOUNTER_IGRIS_CINEMATIC) {
                igrisApproach=p;
                igrisStarts++;
                assert(igrisStarts==1 && EncounterPaused(e) && !EnemyCanBeHit(&enemies->list[0]));
            }
            return changed;
        }
        if (e->presentation.igrisIntro && e->presentation.remaining > 0) {
            assert(EncounterPaused(e) && !EncounterBossVisible(e,enemies));
            return UpdateEncounter(e,enemies,map,p,health);
        }
        assert(e->presentation.igrisIntro && e->presentation.remaining == 0);
        if (!igrisCardCleared) {
            // O áudio seleciona a faixa antes de atualizar a cartela no loop.
            igrisCardCleared = true;
            return UpdateEncounter(e,enemies,map,p,health);
        }
        assert(e->phase==ENCOUNTER_FIGHT && !EncounterPaused(e) && EncounterBossVisible(e,enemies));
        assert(igrisStarts==1 && enemies->list[0].state==ENEMY_IDLE && enemies->list[0].attackCooldown>1);
        if (activeAudio->tracks[TRACK_IGRIS_DUEL].stream.buffer) {
            assert(activeAudio->playing[TRACK_IGRIS_DUEL]);
            assert(!activeAudio->playing[TRACK_IGRIS]);
        }
        if (mode == 7) {
            Enemy *boss=&enemies->list[0];
            DamageEnemy(boss,boss->health);
            assert(!BeginIgrisAftermath(e,enemies,*health));
            UpdateEnemies(enemies,map,p,2,true,health,100);
            DamageEnemy(boss,boss->health);
            assert(!BeginIgrisAftermath(e,enemies,*health));
            UpdateEnemies(enemies,map,p,2,true,health,100);
            DamageEnemy(boss,boss->health);
            assert(boss->igrisFinalForm && boss->health == 0);
            UpdateEncounter(e,enemies,map,p,health);
            assert(e->phase == ENCOUNTER_FIGHT && boss->state==ENEMY_DIE);
            RunProgress saved={0};
            assert(LoadClearingCheckpoint(&saved) && saved.igrisAftermathPending);
            igrisDefeated=true;
            return false;
        }
        if (p.y < igrisApproach.y-3) transported++;
        return UpdateEncounter(e,enemies,map,p,health);
    }
    if (mode >= 2) {
        if (mode==4 && map->necromancerArena) {
            RunProgress saved={0};
            assert(LoadClearingCheckpoint(&saved) && saved.map==NECROMANCER_MAP);
            assert(saved.stage==CLEARING_BOSS_STAGE && saved.health==83 && saved.weaponLevel[2]==2);
            assert(enemies->list[0].necromancerForm==1 && p.y==map->spawn.y);
            transported++;
        }
        if (map->templarField && (mode == 2 || (mode==3 && p.y < map->exit.height - 15))) {
            RunProgress saved = {0};
            assert(*health == 83 && e->progress.weaponLevel[2] == 2);
            assert(LoadClearingCheckpoint(&saved) && saved.map == TEMPLAR_FIELD_MAP);
            if (mode == 2) {
                assert(cinematicFrames >= (skipBonelandPassage ? 1 : 8));
                assert(e->progress.stage == 1 && saved.stage == 1);
                assert(map->exitLocked && EnemiesRemaining(enemies) == 8);
                assert(p.x == map->spawn.x && p.y == map->spawn.y);
            } else {
                assert(saved.stage == CLEARING_COMPLETE_STAGE && e->phase == ENCOUNTER_CLEAR);
                assert(!map->exitLocked && EnemiesRemaining(enemies) == 0);
            }
            transported++;
        }
        return UpdateEncounter(e,enemies,map,p,health);
    }
    if (!answered && !EncounterPaused(e)) {
        Enemy *boss=&enemies->list[0];
        boss->position=p;
        boss->state=ENEMY_FALLEN;
        boss->health=0;
        assert(OpenEncounterSeal(e,enemies,p));
        assert(AnswerEncounterSeal(e,enemies,health,(e->answer+1)%3));
        assert(!GuardianLiberationPending(e) && boss->health>0);
        boss->state=ENEMY_FALLEN;
        boss->health=0;
        assert(OpenEncounterSeal(e,enemies,p));
        assert(AnswerEncounterSeal(e,enemies,health,e->answer));
        assert(GuardianLiberationPending(e));
        answered=true;
        return true;
    }
    return UpdateEncounter(e,enemies,map,p,health);
}
static void TestCinematic(GuardianCutscene *scene,float dt,GuardianCutsceneInput input) {
    cinematicFrames++;
    assert(active && EncounterPaused(active));
    for (int i=0;i<TRACK_COUNT;i++) {
        assert(!activeAudio->playing[i]);
        if (activeAudio->tracks[i].stream.buffer) assert(!IsMusicStreamPlaying(activeAudio->tracks[i]));
    }
    input=(GuardianCutsceneInput){0};
    if (mode >= FIRST_NEW_MODE) {
        int index=(mode-FIRST_NEW_MODE)/2;
        assert(scene->count==checkedScenes[index].scenes);
        assert(!strcmp(scene->scenes[0].id,checkedScenes[index].firstScene));
        for (int i=0;i<scene->count;i++) {
            assert(scene->scenes[i].artIndex>=0);
            assert(strncmp(scene->scenes[i].image,"abertura/",9)==0);
        }
        if (scene->current==0 && scene->elapsed==0) checkedSceneStarts++;
        checkedLastScene=scene->current;
    }
    if (mode == 2) {
        assert(active->progress.map == TEMPLAR_FIELD_MAP && active->progress.stage == 1);
        assert(scene->count == 8 && scene->artCount == 7);
        assert(scene->scenes[scene->current].audioEvent[0] == 0);
    }
    if (mode >= 7 && active->phase == ENCOUNTER_IGRIS_AFTERMATH) {
        if (scene->current == 0 && scene->elapsed == 0) aftermathStarts++;
        GuardianScene *shot=&scene->scenes[scene->current];
        if (shot->waitsForInput && scene->letters == 0) aftermathLines++;
    }
    if (mode==0 || mode==5 || mode==7 || (mode==2 && skipBonelandPassage) ||
        (mode>=FIRST_NEW_MODE && (mode-FIRST_NEW_MODE)%2==0)) input.skip=true;
    else if (scene->scenes[scene->current].waitsForInput) input.advance=true;
    else scene->elapsed=scene->scenes[scene->current].duration-.01f;
    UpdateGuardianCutscene(scene,dt,input);
}
static void TestLiberation(Encounter *e) {
    assert(!liberated && GuardianLiberationPending(e));
    CompleteGuardianLiberation(e);
    assert(!EncounterPaused(e) && e->progress.guardianAftermath==GUARDIAN_ALTAR_READY);
    liberated++;
}
static bool TestAltar(Encounter *e,const Clearing *map,Vector2 *p,bool interact) {
    if (liberated && interact) {
        /* Walking/pathfinding has its own audit. Place the actor at a free altar approach. */
        assert(FindFreeMapPosition(map,map->pedestal,p));
        assert(!IsMapActorBlocked(map,*p));
    }
    return BeginGuardianPassage(e,map,*p,interact);
}
static bool TestPassage(Encounter *e,EnemyGroup *enemies,Clearing *map,Vector2 *p,int health) {
    assert(liberated==1 && !transported && GuardianPassagePending(e));
    assert(CompleteGuardianPassage(e,enemies,map,p,health));
    assert(e->progress.map==1 && e->progress.stage==1 && map->cemetery);
    RunProgress saved={0};
    assert(LoadClearingCheckpoint(&saved) && saved.map==1 &&
           saved.guardianAftermath==GUARDIAN_PASSAGE_COMPLETE);
    transported++;
    return true;
}
static void CheckIgrisDefeatFlow(void) {
    for (mode=7;mode<=8;mode++) {
        igrisCardCleared=false;
        frames=transported=cinematicFrames=igrisStarts=aftermathStarts=aftermathLines=igrisDefeatFrames=0;
        igrisDefeated=false;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1 && aftermathStarts==1);
        assert(mode==7 ? igrisDefeated && igrisStarts==1 && igrisDefeatFrames>=20 :
               aftermathLines==6 && igrisStarts==0);
    }
    puts("Igris defeat animation preceded the aftermath, and the saved aftermath resumed.");
}

int main(int argc, char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    SetTraceLogLevel(LOG_WARNING);
    if (argc==2 && !strcmp(argv[1],"--igris-defeat")) {
        CheckIgrisDefeatFlow();
        return 0;
    }
    for (int i=0;i<(int)(sizeof(checkedScenes)/sizeof(*checkedScenes));i++) {
        for (int complete=0;complete<2;complete++) {
            mode=FIRST_NEW_MODE+2*i+complete;
            frames=transported=cinematicFrames=checkedSceneStarts=0;
            checkedLastScene=-1;
            active=NULL;
            printf("Real story loop: %s %s.\n",
                checkedScenes[i].firstScene,complete ? "complete" : "skip");
            assert(ActualGameMain()==0 && transported==1);
            assert(checkedSceneStarts==1 && checkedLastScene==(complete ? checkedScenes[i].scenes-1 : 0));
        }
    }
    for (mode=0;mode<2;mode++) {
        frames=liberated=transported=cinematicFrames=0;
        answered=false;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(liberated==1 && transported==1 && cinematicFrames>=2);
        printf("Real story loop: %s reached cemetery after %d frames.\n",mode ? "complete":"skip",frames);
    }
    for (mode=2;mode<5;mode++) {
        frames=liberated=transported=cinematicFrames=0;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1);
    }
    mode=2;
    skipBonelandPassage=true;
    frames=transported=cinematicFrames=0;
    active=NULL;
    assert(ActualGameMain()==0 && transported==1);
    for (mode=5;mode<7;mode++) {
        igrisCardCleared=false;
        frames=transported=cinematicFrames=igrisStarts=0;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1 && igrisStarts==1 && cinematicFrames>0);
        printf("Real Igris loop: %s restored gameplay after %d frames.\n",mode==5 ? "skip" : "complete",frames);
    }
    CheckIgrisDefeatFlow();
    puts("The campaign entered the Templar Field and reached the Necromancer arena.");
    return 0;
}
