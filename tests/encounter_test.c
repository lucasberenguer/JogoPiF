#include "encounter.h"
#include "menu.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void Capture(const char *name, Encounter *encounter, EnemyGroup *enemies,
                    Clearing *clearing, Vector2 player) {
    RenderTexture2D target=LoadRenderTexture(1280,720);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    Camera2D camera={.zoom=.72f};
    BeginMode2D(camera);
    DrawClearing(clearing);
    if (ClearingAltarOpen(encounter, clearing)) DrawClearingAltarGlow(clearing, 0.5f);
    if (strstr(name,"collision")) DrawClearingCollisionGuide(clearing);
    DrawEnemies(enemies,player,false);
    DrawCircleV(player,15,SKYBLUE);
    DrawEnemies(enemies,player,true);
    DrawEnemyProjectiles(enemies);
    EndMode2D();
    if (!strstr(name,"collision") && !strstr(name,"directions") && !strstr(name,"attack_poses")) DrawEncounter(encounter,enemies);
    EndTextureMode();
    Image shot=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&shot);
    assert(ExportImage(shot,name));
    UnloadImage(shot);
    UnloadRenderTexture(target);
}

static void CheckLegacyCheckpoints(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/partida_pif.sav", GetApplicationDirectory());
    const int legacyStages[] = {6, 7, 8, 11, 12, 13, 14};
    const int maps[] = {0, 0, 1, 1, 1, 1, 1};
    const int stages[] = {CLEARING_BOSS_STAGE, CLEARING_COMPLETE_STAGE, 1, 4,
                          CLEARING_BOSS_STAGE, CLEARING_BOSS_STAGE, CLEARING_COMPLETE_STAGE};
    for (int i = 0; i < (int)(sizeof(legacyStages)/sizeof(legacyStages[0])); i++) {
        char contents[128];
        snprintf(contents, sizeof(contents), "PIF-CLEARING-3 2 %d 90 120 1.25 240 1.2 0 0 2 -1\n",
                 legacyStages[i]);
        assert(SaveFileText(path, contents));
        RunProgress progress;
        assert(LoadClearingCheckpoint(&progress));
        assert(progress.stage == stages[i] && progress.map == maps[i]);
        assert(progress.weapon == 2 && progress.health == 90 && progress.weaponLevel[2] == 2);
    }
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"Encounter audit");
    assert(IsWindowReady());
    CheckLegacyCheckpoints();
    Clearing clearing={0};
    EnemyGroup enemies={0};
    assert(InitClearing(&clearing));
    Vector2 pedestalApproach;
    assert(FindFreeMapPosition(&clearing, clearing.pedestal, &pedestalApproach));
    assert(CheckCollisionPointCircle(pedestalApproach, clearing.pedestal, CLEARING_ALTAR_RADIUS));
    assert(clearing.exit.width == 0);
    assert(InitEnemies(&enemies));
    Encounter encounter={0};
    Vector2 player={835,540};
    int health=100;
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(0),player);
    assert(encounter.presentation.areaEntry && EncounterPaused(&encounter));
    assert(strcmp(encounter.presentation.mapName,"Clareira do Selo")==0);
    UpdateStagePresentation(&encounter.presentation,.7f);
    Capture("audit/clearing_area_entry.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,2.2f);
    assert(!encounter.presentation.areaEntry && encounter.presentation.wave==1);
    assert(EncounterPaused(&encounter));
    Capture("audit/clearing_collision_updated.png",&encounter,&enemies,&clearing,clearing.spawn);
    for (int wave=1;wave<=CLEARING_WAVES;wave++) {
        assert(encounter.progress.stage==wave);
        assert(EnemiesRemaining(&enemies)==7+(wave-1)*4);
        assert(EncounterPaused(&encounter));
        UpdateStagePresentation(&encounter.presentation,.1f);
        assert(EncounterPaused(&encounter));
        UpdateStagePresentation(&encounter.presentation,10);
        assert(!EncounterPaused(&encounter));
        int specials=0;
        for (int i=0;i<ENEMY_COUNT;i++) {
            Enemy *enemy=&enemies.list[i];
            if (enemy->state==ENEMY_GONE) continue;
            assert(enemy->state==ENEMY_SPAWNING);
            assert(!EnemyCanBeHit(enemy));
            assert(!IsClearingBlocked(&clearing,enemy->position));
            if (enemy->type==ENEMY_SPECIAL) specials++;
            enemy->state=ENEMY_IDLE;
            DamageEnemy(enemy,enemy->health);
        }
        assert(specials>=1+(wave-1)/2 && specials<=1+(wave-1)/2+(wave>=3));
        UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
        assert(EnemiesRemaining(&enemies)==specials);
        for (int i=0;i<ENEMY_COUNT;i++) if (enemies.list[i].state==ENEMY_FALLEN) {
            Enemy *enemy=&enemies.list[i];
            assert(OpenEncounterSeal(&encounter,&enemies,enemy->position));
            assert(EncounterPaused(&encounter));
            int correct=encounter.question==0 || encounter.question==3?0:1;
            if (wave==1) {
                int old=enemy->maxHealth;
                assert(AnswerEncounterSeal(&encounter,&enemies,&health,1-correct));
                assert(enemy->maxHealth>old && EnemyCanBeHit(enemy));
                DamageEnemy(enemy,enemy->health);
                UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
                assert(OpenEncounterSeal(&encounter,&enemies,enemy->position));
                correct=encounter.question==0 || encounter.question==3?0:1;
            }
            assert(AnswerEncounterSeal(&encounter,&enemies,&health,correct));
        }
        assert(EnemiesRemaining(&enemies)==0);
        UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
        assert(encounter.phase==ENCOUNTER_REWARD);
        assert(encounter.rewardCount==(wave==CLEARING_WAVES?2:3));
        if (wave==1) Capture("audit/rewards.png",&encounter,&enemies,&clearing,player);
        assert(!ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,9));
        assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
        RunProgress saved={0};
        assert(LoadClearingCheckpoint(&saved));
        assert(saved.stage==wave+1 && saved.weapon==encounter.progress.weapon);
    }
    assert(encounter.progress.stage==CLEARING_BOSS_STAGE && EnemiesRemaining(&enemies)==1);
    assert(encounter.presentation.boss && EncounterPaused(&encounter));
    UpdateStagePresentation(&encounter.presentation,.7f);
    Capture("audit/boss_intro.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,10);
    assert(!EncounterPaused(&encounter));
    Enemy *boss=&enemies.list[0];
    assert(boss->type==ENEMY_BOSS && EnemyHurtRadius(boss)>20);
    boss->state=ENEMY_IDLE;
    boss->position=(Vector2){player.x-200,player.y};
    boss->attackCooldown=0;
    health=100;
    UpdateEnemies(&enemies,&clearing,player,.05f,false,&health,100);
    assert(boss->state==ENEMY_ATTACK);
    DamageEnemy(boss,10);
    assert(boss->state==ENEMY_ATTACK); /* normal hits cannot cancel every special */
    UpdateEnemies(&enemies,&clearing,player,.65f,false,&health,100);
    assert(health==100); /* visible warning has not expired */
    UpdateEnemies(&enemies,&clearing,player,.15f,false,&health,100);
    assert(health<100);
    Capture("audit/boss_roots.png",&encounter,&enemies,&clearing,player);
    DamageEnemy(boss,boss->health);
    assert(boss->health==boss->maxHealth/2 && !EnemyCanBeHit(boss));
    UpdateEnemies(&enemies,&clearing,player,1.0f,true,&health,100);
    assert(boss->guardianPhase==GUARDIAN_PHASE_TWO && EnemyCanBeHit(boss));
    DamageEnemy(boss,boss->health);
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
    assert(boss->state==ENEMY_FALLEN && !EnemyCanBeHit(boss));
    assert(!ClearingAltarOpen(&encounter,&clearing));
    assert(!CanEnterCemetery(&encounter,&clearing,pedestalApproach,true));
    for (int i=0;i<ENEMY_ROOT_COUNT;i++) assert(!enemies.rootAttacks[i].active);
    assert(OpenEncounterSeal(&encounter,&enemies,boss->position));
    Capture("audit/boss_seal.png",&encounter,&enemies,&clearing,player);
    float oldPower=encounter.progress.bossPower;
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,(encounter.answer+1)%3));
    assert(encounter.progress.bossPower>oldPower && boss->health==boss->maxHealth);
    RunProgress revanche;
    assert(LoadClearingCheckpoint(&revanche));
    assert(revanche.stage==CLEARING_BOSS_STAGE && revanche.bossPower>1);
    DamageEnemy(boss,boss->health);
    assert(boss->health==boss->maxHealth/2 && !EnemyCanBeHit(boss));
    UpdateEnemies(&enemies,&clearing,player,1.0f,true,&health,100);
    assert(boss->guardianPhase==GUARDIAN_PHASE_TWO && EnemyCanBeHit(boss));
    DamageEnemy(boss,boss->health);
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
    assert(OpenEncounterSeal(&encounter,&enemies,boss->position));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,encounter.answer));
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR && encounter.progress.stage==CLEARING_COMPLETE_STAGE);
    assert(!encounter.notice[0]);
    assert(ClearingAltarOpen(&encounter,&clearing));
    assert(!CanEnterCemetery(&encounter,&clearing,pedestalApproach,true));
    assert(encounter.presentation.announcement && EncounterPaused(&encounter));
    assert(strcmp(encounter.presentation.announcement,"CLAREIRA LIMPA")==0);
    UpdateStagePresentation(&encounter.presentation,3);
    assert(encounter.presentation.areaUnlock && EncounterPaused(&encounter));
    assert(strcmp(encounter.presentation.mapName,"Cemiterio das Raizes")==0);
    Capture("audit/area_unlock.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,4);
    assert(!EncounterPaused(&encounter));
    assert(!CanEnterCemetery(&encounter,&clearing,pedestalApproach,false));
    Vector2 formerExit = {clearing.background.width*.46f, clearing.background.height*.985f};
    assert(!CanEnterCemetery(&encounter,&clearing,formerExit,true));
    assert(CanEnterCemetery(&encounter,&clearing,pedestalApproach,true));
    Capture("audit/clearing_altar_open.png",&encounter,&enemies,&clearing,pedestalApproach);
    RunProgress completed;
    assert(LoadClearingCheckpoint(&completed) && completed.stage==CLEARING_COMPLETE_STAGE);
    health=0;
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_DEFEAT && EncounterPaused(&encounter));
    UnloadClearing(&clearing);
    clearing=(Clearing){0};
    assert(InitCemetery(&clearing));
    enemies.cemetery=true;
    encounter.phase=ENCOUNTER_FIGHT;
    encounter.notice[0]=0;
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    for(int kind=0;kind<3;kind++) for(int face=0;face<4;face++) {
        int at=kind*4+face;
        enemies.list[at]=(Enemy){.position={380+face*270,300+kind*185},
            .type=kind==0?ENEMY_COMMON_MELEE:kind==1?ENEMY_COMMON_RANGED:ENEMY_BOSS,
            .state=ENEMY_WALK,.walkTime=.25f,.facing=face,.health=100,.maxHealth=100};
    }
    encounter.presentation.remaining=0;
    Capture("audit/cemetery_directions_v3.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    for(int attack=0;attack<3;attack++) for(int beat=0;beat<4;beat++) {
        enemies.list[attack*4+beat]=(Enemy){.position={380+beat*270,300+attack*185},
            .type=ENEMY_BOSS,.state=ENEMY_ATTACK,.bossAttack=attack,
            .stateTime=(float[]){.15f,.55f,.95f,1.8f}[beat],.facing=3,
            .health=2420,.maxHealth=2420};
    }
    Capture("audit/cemetery_attack_poses_v3.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    enemies.list[0]=(Enemy){.position={500,470},.type=ENEMY_BOSS,.state=ENEMY_ATTACK,
        .bossAttack=COVEIRO_SHOVEL,.facing=3,.stateTime=.95f,.health=2420,.maxHealth=2420};
    for(int i=0;i<3;i++) enemies.cemeteryProjectiles[i]=(CemeteryProjectile){.active=true,
        .position={850+i*150,350+i*110},.velocity={740,(i-1)*180},.life=1.9f-i*.08f};
    Capture("audit/cemetery_attack_poses_shovel_v4.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    ClearEnemyAttacks(&enemies);
    player=clearing.spawn;health=100;
    RunProgress cemeteryProgress=NewRunProgress(0);
    cemeteryProgress.damage=1.4f;
    BeginEncounter(&encounter,&enemies,&clearing,cemeteryProgress,player);
    assert(encounter.progress.map==1 && encounter.progress.damage==1.4f);
    UpdateStagePresentation(&encounter.presentation,.6f);
    Capture("audit/cemetery_wave.png",&encounter,&enemies,&clearing,player);
    Capture("audit/cemetery_collision_updated.png",&encounter,&enemies,&clearing,player);
    for (int wave=1;wave<=CLEARING_WAVES;wave++) {
        assert(encounter.progress.stage==wave);
        assert(EnemiesRemaining(&enemies)==7+(wave-1)*4);
        UpdateStagePresentation(&encounter.presentation,10);
        for (int i=0;i<ENEMY_COUNT;i++) {
            Enemy *enemy=&enemies.list[i];
            if (enemy->state==ENEMY_GONE) continue;
            assert(!IsMapActorBlocked(&clearing,enemy->position));
            enemy->state=ENEMY_IDLE;
            DamageEnemy(enemy,enemy->health);
        }
        UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,encounter.progress.maxHealth);
        for (int i=0;i<ENEMY_COUNT;i++) if (enemies.list[i].state==ENEMY_FALLEN) {
            assert(OpenEncounterSeal(&encounter,&enemies,enemies.list[i].position));
            int correct=encounter.question==0 || encounter.question==3?0:1;
            assert(AnswerEncounterSeal(&encounter,&enemies,&health,correct));
        }
        UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
        assert(encounter.phase==ENCOUNTER_REWARD);
        assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
        RunProgress saved;
        assert(LoadClearingCheckpoint(&saved) && saved.map==1 && saved.stage==wave+1);
    }
    assert(encounter.progress.stage==CLEARING_BOSS_STAGE && EnemiesRemaining(&enemies)==1);
    UpdateStagePresentation(&encounter.presentation,.7f);
    Capture("audit/cemetery_boss_intro.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,10);
    assert(EncounterBossVisible(&encounter,&enemies) && enemies.list[0].health==2420);
    Capture("audit/cemetery_boss_hud.png",&encounter,&enemies,&clearing,player);
    enemies.list[0].state=ENEMY_IDLE;
    DamageEnemy(&enemies.list[0],enemies.list[0].health);
    UpdateEnemies(&enemies,&clearing,player,1.0f,true,&health,encounter.progress.maxHealth);
    assert(enemies.list[0].guardianPhase==GUARDIAN_PHASE_TWO);
    DamageEnemy(&enemies.list[0],enemies.list[0].health);
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,encounter.progress.maxHealth);
    assert(enemies.list[0].state==ENEMY_FALLEN);
    assert(OpenEncounterSeal(&encounter,&enemies,enemies.list[0].position));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,encounter.answer));
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR);
    assert(strcmp(encounter.presentation.announcement,"CEMITERIO LIMPO")==0);

    RunProgress normalSave=NewRunProgress(1);normalSave.stage=3;
    assert(SaveClearingCheckpoint(&normalSave));
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(0),player);
    encounter.practice=true;
    UpdateStagePresentation(&encounter.presentation,10);
    for (int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
    RunProgress preserved;
    assert(LoadClearingCheckpoint(&preserved));
    assert(preserved.weapon==1 && preserved.stage==3 && preserved.map==0);
    UpdateStagePresentation(&encounter.presentation,10);
    enemies.list[0]=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_FALLEN,
        .position=player,.maxHealth=100,.power=1};
    assert(OpenEncounterSeal(&encounter,&enemies,player));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,(encounter.answer+1)%3));
    assert(LoadClearingCheckpoint(&preserved) && preserved.stage==3 && preserved.map==0);
    for (int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    encounter.progress.stage=CLEARING_BOSS_STAGE;
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR);
    assert(LoadClearingCheckpoint(&preserved) && preserved.stage==3 && preserved.map==0);

    /* Audit only the test executable's checkpoint, never the player's save. */
    const char *saveFile=TextFormat("%s/partida_pif.sav",GetApplicationDirectory());
    const char *invalidSaves[]={
        "", "PIF-RUN-5 0 1", "PIF-RUN-5 9 1 100 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 99 100 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 100 100 nan 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 100 100 1 inf 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 101 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 100 100 1 240 1 0 0 0 -1 2",
        "PIF-RUN-5 0 1 100 100 1 240 1 4 0 0 -1 0"
    };
    for (unsigned i=0;i<sizeof(invalidSaves)/sizeof(invalidSaves[0]);i++) {
        assert(SaveFileText(saveFile,(char *)invalidSaves[i]));
        RunProgress rejected=NewRunProgress(2);
        assert(!LoadClearingCheckpoint(&rejected));
        assert(rejected.weapon==2); /* Failure must not partly overwrite the caller. */
    }
    assert(SaveFileText(saveFile,"PIF-CLEARING-3 1 7 80 100 1 240 1 0 0 0 -1"));
    RunProgress migrated;
    assert(LoadClearingCheckpoint(&migrated));
    assert(migrated.map==0 && migrated.stage==CLEARING_COMPLETE_STAGE);
    assert(SaveFileText(saveFile,"PIF-CLEARING-3 1 6 80 100 1 240 1 0 0 0 -1"));
    assert(LoadClearingCheckpoint(&migrated) && migrated.stage==CLEARING_BOSS_STAGE);
    assert(SaveFileText(saveFile,"PIF-CLEARING-4 1 4 80 100 1 240 1 0 0 0 -1"));
    assert(LoadClearingCheckpoint(&migrated) && migrated.stage==4 && migrated.map==0);
    assert(SaveFileText(saveFile,"PIF-CLEARING-2 2"));
    assert(LoadClearingCheckpoint(&migrated) && migrated.weapon==2 && migrated.stage==1);
    assert(SaveClearingCheckpoint(&encounter.progress));
    UnloadEnemies(&enemies);
    UnloadClearing(&clearing);
    CloseWindow();
    puts("Four waves, boss fifth wave, rewards, boss roots, seals and checkpoints passed");
    return 0;
}
