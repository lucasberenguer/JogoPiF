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
static int aftermathStarts, aftermathLines;
static bool igrisDefeated, igrisCardCleared;
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
    return mode >= 2 && key == KEY_W &&
           (!active || mode >= 3 || active->progress.map == BONELAND_MAP);
}
static MenuAction TestMenu(Menu *menu) {
    activeAudio=&menu->audio;
    RunProgress progress=NewRunProgress(2);
    progress.stage=CLEARING_BOSS_STAGE;
    progress.guardianIntroSeen=true;
    if (mode >= 2) {
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
static bool TestEncounter(Encounter *e,EnemyGroup *enemies,const Clearing *map,Vector2 p,int *health) {
    active=e;
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
        if (activeAudio->tracks[TRACK_MINIBOSS].stream.buffer) assert(activeAudio->playing[TRACK_MINIBOSS]);
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
            assert(e->phase == ENCOUNTER_IGRIS_AFTERMATH && !BeginIgrisAftermath(e,enemies,*health));
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
                assert(cinematicFrames >= 8);
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
    if (mode==0 || mode==5 || mode==7) input.skip=true;
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
    for (mode=2;mode<5;mode++) {
        frames=liberated=transported=cinematicFrames=0;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1);
    }
    for (mode=5;mode<7;mode++) {
        igrisCardCleared=false;
        frames=transported=cinematicFrames=igrisStarts=0;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1 && igrisStarts==1 && cinematicFrames>0);
        printf("Real Igris loop: %s restored gameplay after %d frames.\n",mode==5 ? "skip" : "complete",frames);
    }
    for (mode=7;mode<=8;mode++) {
        igrisCardCleared=false;
        frames=transported=cinematicFrames=igrisStarts=aftermathStarts=aftermathLines=0;
        igrisDefeated=false;
        active=NULL;
        assert(ActualGameMain()==0);
        assert(transported==1 && aftermathStarts==1);
        assert(mode==7 ? igrisDefeated && igrisStarts==1 : aftermathLines==6 && igrisStarts==0);
    }
    puts("The campaign entered the Templar Field and reached the Necromancer arena.");
    return 0;
}
