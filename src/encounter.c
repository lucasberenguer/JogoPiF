#include "encounter.h"
#include "menu.h"
#include "raymath.h"
#include <stdio.h>

enum { REWARD_DAMAGE, REWARD_HEALTH, REWARD_SPEED, REWARD_WEAPON, REWARD_SWAP };
static const struct {
    const char *expression;
    const char *choices[3]; /* correct first; the slot is shuffled */
} bossQuestions[] = {
    {"P e (Q ou nao Q)", {"P", "nao P", "Q"}},
    {"nao (P ou Q)", {"nao P e nao Q", "nao P ou nao Q", "P e Q"}},
    {"(P e Q) ou (nao P e Q)", {"Q", "P", "P ou Q"}},
    {"nao (P e Q)", {"nao P ou nao Q", "nao P e nao Q", "P ou Q"}},
};

static const char *WeaponTitle(int weapon) {
    return weapon == 0 ? "Arma de fogo" : weapon == 1 ? "Biblia" : "Katana";
}

static void StartStage(Encounter *e, EnemyGroup *enemies, const Clearing *clearing, Vector2 player) {
    if (e->progress.stage == 7) {
        for (int i=0; i<ENEMY_COUNT; i++) enemies->list[i] = (Enemy){.state=ENEMY_GONE};
        ClearEnemyAttacks(enemies);
        e->phase = ENCOUNTER_CLEAR;
    } else {
        e->phase = ENCOUNTER_FIGHT;
        if (e->progress.stage == 6) StartClearingBoss(enemies, clearing, player, e->progress.bossPower);
        else StartEnemyWave(enemies, clearing, e->progress.stage, player, e->progress.damage);
    }
}

void BeginEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                    RunProgress progress, Vector2 player) {
    *e = (Encounter){.progress=progress, .sealEnemy=-1};
    StartStage(e, enemies, clearing, player);
}

bool EncounterPaused(const Encounter *e) {
    return e->phase == ENCOUNTER_REWARD || e->phase == ENCOUNTER_SEAL || e->phase == ENCOUNTER_DEFEAT;
}

float EncounterDamage(const Encounter *e) {
    int weapon = e->progress.weapon;
    return e->progress.damage * (1 + .2f*e->progress.weaponLevel[weapon]) *
           (e->progress.strongWeapon == weapon ? 1.5f : 1.0f);
}

static void RollRewards(Encounter *e) {
    if (e->progress.stage == 5) {
        e->rewardCount=2;
        e->rewards[0]=(Reward){REWARD_WEAPON,e->progress.weapon};
        e->rewards[1]=(Reward){REWARD_HEALTH,-1,GetRandomValue(3,6)*10};
        return;
    }
    e->rewardCount=3;
    int swap = GetRandomValue(0,99) < 80 ? GetRandomValue(0,2) : -1;
    for (int i=0; i<3; i++) {
        int kind=GetRandomValue(0,3);
        if (kind == REWARD_WEAPON && e->progress.weaponLevel[e->progress.weapon] >= 3)
            kind=REWARD_DAMAGE;
        if (i==swap) kind=REWARD_SWAP;
        e->rewards[i]=(Reward){kind, kind==REWARD_SWAP ?
            (e->progress.weapon+GetRandomValue(1,2))%3 : e->progress.weapon};
    }
}

static Rectangle ChoiceRect(int index, int count) {
    float w=fminf(330, (GetScreenWidth()-80.0f)/count-16);
    return (Rectangle){GetScreenWidth()/2.0f-(count*w+(count-1)*16)/2+index*(w+16),
                       GetScreenHeight()/2.0f-30,w,140};
}

static int ChoiceInput(int count) {
    for (int i=0; i<count; i++) {
        if (IsKeyPressed(KEY_ONE+i)) return i;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(),ChoiceRect(i,count))) return i;
    }
    return -1;
}

static void ApplyReward(Encounter *e, Reward reward, int *health) {
    bool strong=e->progress.stage==5;
    e->notice[0]=0;
    if (reward.kind==REWARD_DAMAGE) e->progress.damage+=.15f;
    else if (reward.kind==REWARD_HEALTH) {
        int added=strong?reward.amount:20;
        e->progress.maxHealth+=added;
        *health+=added;
        if (strong) { e->progress.damage+=.2f; e->progress.speed+=15; }
    } else if (reward.kind==REWARD_SPEED) e->progress.speed+=15;
    else if (reward.kind==REWARD_WEAPON) {
        if (strong) e->progress.strongWeapon=e->progress.weapon;
        else if (e->progress.weaponLevel[e->progress.weapon]<3) e->progress.weaponLevel[e->progress.weapon]++;
    } else {
        int old=e->progress.weapon;
        e->progress.weapon=reward.weapon;
        int bonus=GetRandomValue(0,2);
        float strength=.08f+.03f*e->progress.weaponLevel[old];
        if (bonus==0) {
            e->progress.damage+=strength;
            snprintf(e->notice,sizeof(e->notice),"Troca: dano +%d%%",(int)(strength*100));
        } else if (bonus==1) {
            e->progress.maxHealth+=15; *health+=15;
            snprintf(e->notice,sizeof(e->notice),"Troca: vida maxima +15");
        } else {
            e->progress.speed+=12;
            snprintf(e->notice,sizeof(e->notice),"Troca: velocidade +12");
        }
    }
}

bool ChooseEncounterReward(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                           Vector2 player, int *health, int choice) {
    if (e->phase!=ENCOUNTER_REWARD || choice<0 || choice>=e->rewardCount) return false;
    ApplyReward(e,e->rewards[choice],health);
    e->progress.stage++;
    e->progress.health=*health;
    SaveClearingCheckpoint(&e->progress);
    StartStage(e,enemies,clearing,player);
    return true;
}

bool OpenEncounterSeal(Encounter *e, const EnemyGroup *enemies, Vector2 player) {
    if (e->phase!=ENCOUNTER_FIGHT) return false;
    for (int i=0;i<ENEMY_COUNT;i++) if (enemies->list[i].state==ENEMY_FALLEN &&
        Vector2Distance(enemies->list[i].position,player)<95) {
        e->sealEnemy=i; e->question=GetRandomValue(0,3); e->answer=GetRandomValue(0,2);
        e->phase=ENCOUNTER_SEAL;
        return true;
    }
    return false;
}

bool AnswerEncounterSeal(Encounter *e, EnemyGroup *enemies, int *health, int choice) {
    if (e->phase!=ENCOUNTER_SEAL || e->sealEnemy<0 || e->sealEnemy>=ENEMY_COUNT) return false;
    Enemy *enemy=&enemies->list[e->sealEnemy];
    int count=enemy->type==ENEMY_BOSS?3:2;
    if (choice<0 || choice>=count) return false;
    int correct=enemy->type==ENEMY_BOSS?e->answer:(e->question==0 || e->question==3?0:1);
    ClearEnemyAttacks(enemies);
    if (choice==correct) enemy->state=ENEMY_GONE;
    else {
        enemy->maxHealth=(int)fminf(65000,enemy->maxHealth*1.2f);
        enemy->health=enemy->maxHealth;
        enemy->power*=1.15f;
        enemy->state=ENEMY_IDLE; enemy->stateTime=0; enemy->attackCooldown=1;
        snprintf(e->notice,sizeof(e->notice),"Selo incorreto: o inimigo voltou mais forte");
        if (enemy->type==ENEMY_BOSS) {
            e->progress.bossPower=fminf(100,e->progress.bossPower*1.2f);
            e->progress.health=*health;
            SaveClearingCheckpoint(&e->progress);
        }
    }
    e->phase=ENCOUNTER_FIGHT;
    return true;
}

bool UpdateEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                     Vector2 player, int *health) {
    if (*health<=0) { e->phase=ENCOUNTER_DEFEAT; return false; }
    if (e->phase==ENCOUNTER_FIGHT) {
        if (EnemiesRemaining(enemies)==0) {
            ClearEnemyAttacks(enemies);
            if (e->progress.stage==6) {
                e->progress.stage=7;
                e->progress.health=*health;
                e->phase=ENCOUNTER_CLEAR;
                SaveClearingCheckpoint(&e->progress);
            } else {
                e->phase=ENCOUNTER_REWARD; e->rerolled=false; RollRewards(e);
            }
        } else if (IsKeyPressed(KEY_E)) {
            OpenEncounterSeal(e,enemies,player);
        }
    } else if (e->phase==ENCOUNTER_REWARD) {
        if (IsKeyPressed(KEY_R) && !e->rerolled) { e->rerolled=true; RollRewards(e); }
        int selected=ChoiceInput(e->rewardCount);
        if (selected>=0) {
            return ChooseEncounterReward(e,enemies,clearing,player,health,selected);
        }
    } else if (e->phase==ENCOUNTER_SEAL) {
        Enemy *enemy=&enemies->list[e->sealEnemy];
        int selected=ChoiceInput(enemy->type==ENEMY_BOSS?3:2);
        if (selected>=0) return AnswerEncounterSeal(e,enemies,health,selected);
    }
    return false;
}

static const char *RewardText(const Encounter *e, Reward reward) {
    if (e->progress.stage==5) return reward.kind==REWARD_WEAPON ?
        "Arma atual\nDano +50%" : TextFormat("Status\nVida +%d / dano +20%%\nVelocidade +15",reward.amount);
    switch (reward.kind) {
        case REWARD_DAMAGE: return "Dano\n+15% de dano base";
        case REWARD_HEALTH: return "Vida maxima\n+20 de vida";
        case REWARD_SPEED: return "Velocidade\n+15 de velocidade";
        case REWARD_WEAPON: return TextFormat("%s\nUpgrade %d/3\nDano +20%%",WeaponTitle(reward.weapon),e->progress.weaponLevel[reward.weapon]+1);
        default: return TextFormat("Trocar por %s\nSubstitui a arma atual\nBonus surpresa",WeaponTitle(reward.weapon));
    }
}

void DrawEncounter(const Encounter *e, const EnemyGroup *enemies) {
    if (e->progress.stage<=5) DrawText(TextFormat("Clareira - onda %d/5 | Restantes: %d",
        e->progress.stage,EnemiesRemaining(enemies)),24,205,20,GOLD);
    else DrawText(e->phase==ENCOUNTER_CLEAR ? "Clareira selada - E no pedestal" : "Boss da clareira",24,205,20,GOLD);
    if (e->notice[0]) DrawText(e->notice,24,232,18,RAYWHITE);
    if (e->progress.stage==6 && enemies->list[0].health>0) {
        const Enemy *boss=&enemies->list[0];
        int w=(int)fminf(600,GetScreenWidth()*.5f), x=(GetScreenWidth()-w)/2;
        DrawRectangle(x,GetScreenHeight()-55,w,18,DARKGRAY);
        DrawRectangle(x,GetScreenHeight()-55,w*boss->health/boss->maxHealth,18,MAROON);
        DrawText(TextFormat("Guardiao da clareira  %d/%d",boss->health,boss->maxHealth),x,GetScreenHeight()-82,20,GOLD);
    }
    if (!EncounterPaused(e)) return;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.86f));
    int titleY=(int)(GetScreenHeight()/2-160);
    const char *title=e->phase==ENCOUNTER_REWARD ? "Escolha seu bonus" :
                      e->phase==ENCOUNTER_SEAL ? "Selo - combate pausado" : "Fim da tentativa";
    DrawText(title,(GetScreenWidth()-MeasureText(title,30))/2,titleY,30,GOLD);
    if (e->phase==ENCOUNTER_DEFEAT) {
        DrawText("Esc: menu | Continuar reinicia esta onda ou boss",GetScreenWidth()/2-270,titleY+80,20,RAYWHITE);
        return;
    }
    int count=e->phase==ENCOUNTER_REWARD ? e->rewardCount :
              enemies->list[e->sealEnemy].type==ENEMY_BOSS ? 3 : 2;
    static const char *questions[]={"V e V = V", "V e F = V", "nao V = V", "V ou F = V"};
    if (e->phase==ENCOUNTER_SEAL) {
        const char *prompt=count==3 ? TextFormat("Qual expressao equivale a: %s ?",bossQuestions[e->question].expression)
                                    : questions[e->question];
        DrawText(prompt,(GetScreenWidth()-MeasureText(prompt,22))/2,titleY+60,22,RAYWHITE);
    }
    for (int i=0;i<count;i++) {
        Rectangle box=ChoiceRect(i,count);
        DrawRectangleRec(box,CheckCollisionPointRec(GetMousePosition(),box)?(Color){80,45,50,255}:(Color){40,25,35,255});
        DrawRectangleLinesEx(box,2,GOLD);
        const char *label;
        if (e->phase==ENCOUNTER_REWARD) label=RewardText(e,e->rewards[i]);
        else if (count==2) label=i==0?"Verdadeiro":"Falso";
        else label=bossQuestions[e->question].choices[(i+3-e->answer)%3];
        DrawText(TextFormat("%d. %s",i+1,label),(int)box.x+14,(int)box.y+20,19,RAYWHITE);
    }
    if (e->phase==ENCOUNTER_REWARD) DrawText(e->rerolled?"Novo sorteio utilizado":"R: sortear novamente (1 gratuito)",
        GetScreenWidth()/2-180,GetScreenHeight()/2+145,20,GOLD);
}
