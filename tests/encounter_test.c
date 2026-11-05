#include "encounter.h"
#include "menu.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void Capture(const char *name, Encounter *encounter, EnemyGroup *enemies,
                    Clearing *clearing, Vector2 player) {
    RenderTexture2D target=LoadRenderTexture(1280,720);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    Camera2D camera={.zoom=.72f};
    BeginMode2D(camera);
    DrawClearing(clearing);
    DrawEnemies(enemies,player,false);
    DrawCircleV(player,15,SKYBLUE);
    DrawEnemies(enemies,player,true);
    EndMode2D();
    DrawEncounter(encounter,enemies);
    EndTextureMode();
    Image shot=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&shot);
    assert(ExportImage(shot,name));
    UnloadImage(shot);
    UnloadRenderTexture(target);
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"Encounter audit");
    assert(IsWindowReady());
    Clearing clearing={0};
    EnemyGroup enemies={0};
    assert(InitClearing(&clearing));
    assert(InitEnemies(&enemies));
    Encounter encounter={0};
    Vector2 player={835,540};
    int health=100;
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(0),player);
    for (int wave=1;wave<=5;wave++) {
        assert(encounter.progress.stage==wave);
        assert(EnemiesRemaining(&enemies)==6+(wave-1)*3);
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
        assert(specials>=1 && specials<=1+(wave-1)/2);
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
        assert(encounter.rewardCount==(wave==5?2:3));
        if (wave==1) Capture("audit/rewards.png",&encounter,&enemies,&clearing,player);
        assert(!ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,9));
        assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
        RunProgress saved={0};
        assert(LoadClearingCheckpoint(&saved));
        assert(saved.stage==wave+1 && saved.weapon==encounter.progress.weapon);
    }
    assert(encounter.progress.stage==6 && EnemiesRemaining(&enemies)==1);
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
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
    assert(boss->state==ENEMY_FALLEN && !EnemyCanBeHit(boss));
    for (int i=0;i<ENEMY_ROOT_COUNT;i++) assert(!enemies.rootAttacks[i].active);
    assert(OpenEncounterSeal(&encounter,&enemies,boss->position));
    Capture("audit/boss_seal.png",&encounter,&enemies,&clearing,player);
    float oldPower=encounter.progress.bossPower;
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,(encounter.answer+1)%3));
    assert(encounter.progress.bossPower>oldPower && boss->health==boss->maxHealth);
    RunProgress revanche;
    assert(LoadClearingCheckpoint(&revanche));
    assert(revanche.stage==6 && revanche.bossPower>1);
    DamageEnemy(boss,boss->health);
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
    assert(OpenEncounterSeal(&encounter,&enemies,boss->position));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,encounter.answer));
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR && encounter.progress.stage==7);
    RunProgress completed;
    assert(LoadClearingCheckpoint(&completed) && completed.stage==7);
    health=0;
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_DEFEAT && EncounterPaused(&encounter));
    UnloadEnemies(&enemies);
    UnloadClearing(&clearing);
    CloseWindow();
    puts("Five waves, rewards, boss roots, seals and checkpoints passed");
    return 0;
}
