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
static void TestWindow(int w,int h,const char *title);
static void IgnoreResize(int w,int h);
static void IgnoreToggle(void);
static void IgnoreFps(int fps);
static bool TestClose(void);
static bool TestKey(int key);
static bool TestKeyDown(int key);
static float TestDelta(void);
static MenuAction TestMenu(Menu *menu);
static bool TestEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health);
static void TestCinematic(GuardianCutscene *scene,float dt,GuardianCutsceneInput input);
static void TestLiberation(Encounter *e);
static bool TestAltar(Encounter *e,const Clearing *map,Vector2 *p,bool interact);
static bool TestPassage(Encounter *e,EnemyGroup *enemies,Clearing *map,Vector2 *p,int health);
#define main ActualGameMain
#define InitWindow TestWindow
#define SetWindowSize IgnoreResize
#define ToggleFullscreen IgnoreToggle
#define SetTargetFPS IgnoreFps
#define WindowShouldClose TestClose
#define IsKeyPressed TestKey
#define IsKeyDown TestKeyDown
#define GetFrameTime TestDelta
#define UpdateMenu TestMenu
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
#undef SetTargetFPS
#undef WindowShouldClose
#undef IsKeyPressed
#undef IsKeyDown
#undef GetFrameTime
#undef UpdateMenu
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
static void IgnoreFps(int fps) {(void)fps;}
static float TestDelta(void) {return .05f;}
static bool TestClose(void) {return ++frames>1200 || transported>0;}
static bool TestKey(int key) {return key==KEY_E && liberated>0;}
static bool TestKeyDown(int key) {
    return mode >= 2 && key == KEY_W &&
           (!active || mode == 3 || active->progress.map == BONELAND_MAP);
}
static MenuAction TestMenu(Menu *menu) {
    activeAudio=&menu->audio;
    RunProgress progress=NewRunProgress(2);
    progress.stage=CLEARING_BOSS_STAGE;
    progress.guardianIntroSeen=true;
    if (mode >= 2) {
        progress.map = mode == 2 ? BONELAND_MAP : TEMPLAR_FIELD_MAP;
        progress.stage = CLEARING_COMPLETE_STAGE;
        progress.guardianAftermath = GUARDIAN_PASSAGE_COMPLETE;
        progress.health = 83;
        progress.weaponLevel[2] = 2;
    }
    assert(SaveClearingCheckpoint(&progress));
    return MENU_CONTINUE;
}
static bool TestEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health) {
    active=e;
    if (mode >= 2) {
        if (map->templarField && (mode == 2 || p.y < map->exit.height - 15)) {
            RunProgress saved = {0};
            assert(*health == 83 && e->progress.weaponLevel[2] == 2);
            assert(LoadClearingCheckpoint(&saved) && saved.map == TEMPLAR_FIELD_MAP);
            if (mode == 2) {
                assert(e->progress.stage == 1 && saved.stage == 1);
                assert(map->exitLocked && EnemiesRemaining(enemies) == 5);
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
    if (mode==0) input.skip=true;
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
int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    for (mode=0;mode<2;mode++) {
        frames=liberated=transported=cinematicFrames=0;
        answered=false;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(liberated==1 && transported==1 && cinematicFrames>=2);
        printf("Real story loop: %s reached cemetery after %d frames.\n",mode ? "complete":"skip",frames);
    }
    for (mode=2;mode<4;mode++) {
        frames=liberated=transported=0;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1);
    }
    puts("The campaign entered the Templar Field and resumed with the stairs open.");
    return 0;
}
