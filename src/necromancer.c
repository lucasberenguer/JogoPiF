#include "enemy.h"
#include "raymath.h"
#include <math.h>

enum {
    SEPULCHER, CIRCLES, STAFF, PROFANE_STEP, HALO, RUNES, BLADE, FLIGHT, CONDEMNED, BONE_BEAM, CHAINS, CORPSE
};

static const float cageRadius=116, cageClose=.67f, cageLifespan=2.22f, cageThickness=17;
static const float beamLength=1400, beamWidth=48, beamActive=.83f;
static const float chainLength=510, chainWidth=34, chainPull=.29f, chainCut=1.52f;
static const float corpseRadius=156, chainReach=220;
static const float flightStop=1.06f, flightImpact=1.37f, flightRadius=194;
static const float bladeTimes[]={.39f,.75f,1.19f};
static const float bladeReach[]={174,189,221};
static const float automaticPeriod=1.12f, automaticWarning=.32f;
enum { SPECTRAL_SWORDS=8 };

static const float collapseDuration=1.15f, circleCadence=.28f, runeCadence=.30f, runeTravel=.40f;
static const Vector2 runes[] = {{548,382}, {1126,382}, {836,677}};
static const struct {
    float windup, duration;
    int damage;
} skills[] = {
    [SEPULCHER] = {.47f, 1.19f, 27}, [CIRCLES] = {.46f, 1.53f, 34},
    [STAFF] = {.68f, 1.39f, 0}, [PROFANE_STEP] = {.46f, 1.18f, 31},
    [HALO] = {.49f, 1.34f, 30}, [RUNES] = {.52f, 1.71f, 37},
    [BLADE] = {.39f, 1.64f, 49}, [FLIGHT] = {.49f, 1.82f, 62},
    [CONDEMNED] = {.66f, 2.66f, 49}, [BONE_BEAM] = {1.17f, 2.55f, 82},
    [CHAINS] = {.61f, 2.16f, 91}, [CORPSE] = {.91f, 1.52f, 69},
};

bool LoadNecromancerSprites(EnemyGroup *group) {
    group->necromancerCollapse=LoadTexture("assets/necromante_selamento.png");
    if (!group->necromancerCollapse.id) return false;
    SetTextureFilter(group->necromancerCollapse,TEXTURE_FILTER_POINT);
    group->necromancerCombo=LoadTexture("assets/necromante_combos.png");
    if (!group->necromancerCombo.id) return false;
    SetTextureFilter(group->necromancerCombo,TEXTURE_FILTER_POINT);
    group->necromancerEffects=LoadTexture("assets/necromante_combate_v2.png");
    group->necromancerBlade=LoadTexture("assets/necromante_lamina_v2.png");
    if (!group->necromancerEffects.id || !group->necromancerBlade.id) return false;
    SetTextureFilter(group->necromancerBlade,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->necromancerEffects,TEXTURE_FILTER_POINT);
    group->necromancerThrone=LoadTexture("assets/necromante_trono.png");
    group->necromancerPortrait=LoadTexture("assets/necromante_retrato.png");
    if (!group->necromancerThrone.id || !group->necromancerPortrait.id) return false;
    SetTextureFilter(group->necromancerThrone,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->necromancerPortrait,TEXTURE_FILTER_POINT);
    for (int i=0; i<NECROMANCER_FORMS; i++) {
        group->necromancerWalk[i]=LoadTexture(TextFormat("assets/necromante_fase_%d.png",i+1));
        group->necromancerSkills[i]=LoadTexture(TextFormat("assets/necromante_habilidades_%d.png",i+1));
        if (!group->necromancerWalk[i].id || !group->necromancerSkills[i].id) return false;
        SetTextureFilter(group->necromancerWalk[i], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->necromancerSkills[i], TEXTURE_FILTER_POINT);
    }
    return true;
}

void UnloadNecromancerSprites(EnemyGroup *group) {
    if (group->necromancerCollapse.id) {
        UnloadTexture(group->necromancerCollapse);
        group->necromancerCollapse=(Texture2D){0};
    }
    if (group->necromancerCombo.id) {
        UnloadTexture(group->necromancerCombo);
        group->necromancerCombo=(Texture2D){0};
    }
    if (group->necromancerBlade.id) UnloadTexture(group->necromancerBlade);
    if (group->necromancerEffects.id) UnloadTexture(group->necromancerEffects);
    if (group->necromancerThrone.id) UnloadTexture(group->necromancerThrone);
    if (group->necromancerPortrait.id) UnloadTexture(group->necromancerPortrait);
    for (int i=0;i<NECROMANCER_FORMS;i++) {
        if (group->necromancerWalk[i].id) UnloadTexture(group->necromancerWalk[i]);
        if (group->necromancerSkills[i].id) UnloadTexture(group->necromancerSkills[i]);
    }
}

void StartNecromancer(EnemyGroup *group, float power) {
    int hp = (int)(4200 * fmaxf(1,power));
    for(int i=1;i<=NECROMANCER_WRAITHS;i++) group->list[i].state=ENEMY_GONE;
    group->list[0]=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_SPAWNING,.necromancerForm=1,
        .position={836,263},.speed=140,.power=power,.health=hp,.maxHealth=hp,
        .attackCooldown=.65f,.lastKatanaSwing=-1};
    group->necromancer=(NecromancerFight){.lastPlayer={836,845},.previousPlayer={836,845},.automaticClock=1.65f};
}

void DamageNecromancer(Enemy *boss,int damage) {
    int threshold=boss->necromancerForm==1 ? boss->maxHealth*70/100 :
        boss->necromancerForm==2 ? boss->maxHealth*35/100 : 0;
    boss->health-=damage;
    if (boss->health>threshold) return;
    boss->health=threshold;
    boss->stateTime=0;
    if (threshold>0) {
        boss->guardianPhase=GUARDIAN_PHASE_TRANSITION;
        boss->state=ENEMY_IDLE;
    } else boss->state=ENEMY_DIE;
}

static int Facing(Vector2 direction) {
    if (fabsf(direction.x)>fabsf(direction.y)*.75f) return direction.x<0?2:3;
    return direction.y<0?1:0;
}

static void Hurt(EnemyGroup *g,const Enemy *boss,bool dodging,int *health,int damage) {
    if (dodging || g->practiceInvincible || g->playerHitCooldown>0 || *health<=0) return;
    int amount=(int)(damage*boss->power);
    *health=amount<*health ? *health-amount : 0;
    g->playerHitCooldown=.27f;
}

static void Shot(EnemyGroup *g,Vector2 origin,Vector2 direction,float speed,int damage,bool sword) {
    for(int i=0;i<NECROMANCER_SHOTS;i++) if (g->necromancer.shots[i].life<=0) {
        g->necromancer.shots[i]=(NecromancerShot){origin,Vector2Scale(direction,speed),2.7f,damage,sword};
        return;
    }
}

static Vector2 Aim(Vector2 origin, Vector2 target) {
    Vector2 delta=Vector2Subtract(target,origin);
    return Vector2LengthSqr(delta)>1 ? Vector2Normalize(delta) : (Vector2){0,1};
}

static void Fan(EnemyGroup *g,Vector2 origin,Vector2 dir,int count,float spread,float speed,int damage) {
    for(int i=0;i<count;i++)
        Shot(g,origin,Vector2Rotate(dir,(i-(count-1)*.5f)*spread),speed,damage,false);
}

static void SummonWraiths(EnemyGroup *g,const Clearing *map,Vector2 player) {
    for(int i=0;i<NECROMANCER_WRAITHS;i++) {
        Enemy *wraith=&g->list[i+1];
        if(wraith->necromancerWraith && wraith->state!=ENEMY_GONE) continue;
        Vector2 origin;
        Vector2 requested={i==0?525:1147,Clamp(player.y-110,355,670)};
        if(FindFreeMapPosition(map,requested,&origin))
            *wraith=(Enemy){.position=origin,.type=ENEMY_COMMON_RANGED,.state=ENEMY_IDLE,
                .necromancerWraith=true,.health=230,.maxHealth=230,.lastKatanaSwing=-1,
                .dropRolled=true,.attackCooldown=1.0f};
    }
}

static bool WraithAlive(const Enemy *wraith) {
    return wraith->necromancerWraith && wraith->health>0 &&
           wraith->state!=ENEMY_DIE && wraith->state!=ENEMY_GONE;
}

static void UpdateWraiths(EnemyGroup *g,Vector2 player,float dt) {
    const Enemy *boss=&g->list[0];
    for(int i=0;i<NECROMANCER_WRAITHS;i++) {
        Enemy *wraith=&g->list[i+1];
        if(!wraith->necromancerWraith || wraith->state==ENEMY_GONE) continue;
        if(wraith->state==ENEMY_DIE) {
            wraith->stateTime+=dt;
            if(wraith->stateTime>=.32f) wraith->state=ENEMY_GONE;
            continue;
        }
        wraith->bibleHitCooldown=fmaxf(0,wraith->bibleHitCooldown-dt);
        if(boss->state==ENEMY_ATTACK && boss->bossAttack==CORPSE &&
           g->necromancer.markedWraith==i && boss->stateTime<=skills[CORPSE].windup)
            continue;
        wraith->walkTime+=dt;
        if(wraith->walkTime>=5.9f) {
            wraith->state=ENEMY_GONE;
            continue;
        }
        wraith->attackCooldown-=dt;
        if(wraith->attackCooldown<=0) {
            Shot(g,wraith->position,Aim(wraith->position,player),590,22,false);
            wraith->attackCooldown+=1.0f;
        }
    }
}

static bool Crossed(float before,float now,float instant) { return before<instant && now>=instant; }

static void Sweep(EnemyGroup *g,const Clearing *map,const Enemy *boss,Vector2 player,
                  bool dodging,int *health,float reach,float dot,int damage) {
    Vector2 delta=Vector2Subtract(player,boss->position);
    float distance=Vector2Length(delta);
    if (distance<=reach && Vector2DotProduct(delta,boss->attackDirection)>=distance*dot &&
        MapHasClearPath(map,boss->position,player))
        Hurt(g,boss,dodging,health,damage);
}

static bool ActiveSkill(const Enemy *boss,int skill) {
    return boss->state==ENEMY_ATTACK && boss->bossAttack==skill &&
           boss->guardianPhase!=GUARDIAN_PHASE_TRANSITION;
}

static bool InLane(Vector2 origin,Vector2 dir,Vector2 target,float length,float width) {
    Vector2 delta=Vector2Subtract(target,origin);
    float forward=Vector2DotProduct(delta,dir);
    return forward>=0 && forward<=length && fabsf(delta.x*dir.y-delta.y*dir.x)<=width;
}

void MoveNecromancerPlayer(const EnemyGroup *g,const Clearing *map,Vector2 *player,Vector2 delta) {
    const Enemy *boss=&g->list[0];
    if(!map->necromancerArena || !ActiveSkill(boss,BONE_BEAM) ||
       boss->stateTime<cageClose || boss->stateTime>=cageLifespan) {
        MoveMapPlayer(map,player,delta);
        return;
    }

    int steps=(int)(Vector2Length(delta)/8)+1;
    Vector2 step=Vector2Scale(delta,1.0f/steps);
    for(int i=0;i<steps;i++) {
        Vector2 next=*player;
        MoveMapPlayer(map,&next,step);
        float before=Vector2Distance(*player,g->necromancer.cageCenter)-cageRadius;
        float after=Vector2Distance(next,g->necromancer.cageCenter)-cageRadius;
        // If the wall appears underfoot, only allow stepping away on the same side.
        if((before<0)!=(after<0) ||
           (fabsf(after)<cageThickness && fabsf(after)<fabsf(before))) break;
        *player=next;
    }
}

void ApplyNecromancerPull(const EnemyGroup *g,const Clearing *map,Vector2 *player) {
    const Enemy *boss=&g->list[0];
    if(!map->necromancerArena || !ActiveSkill(boss,CHAINS) || g->necromancer.pullTime<=0) return;
    float distance=Vector2Distance(*player,boss->position);
    float step=fminf(fmaxf(0,distance-84),g->necromancer.pullTime*1270);
    MoveMapPlayer(map,player,Vector2Scale(Aim(*player,boss->position),step));
}

static Vector2 SwordOrigin(Vector2 boss,int index,float time) {
    float angle=index*(2*PI/SPECTRAL_SWORDS)+time*1.5f;
    return Vector2Add(boss,(Vector2){cosf(angle)*83,sinf(angle)*52});
}

static float SwordRelease(int index) {
    return skills[CONDEMNED].windup+index*.205f;
}

static float ClearBeamLength(const Clearing *map,Vector2 origin,Vector2 dir);

static void StartSkill(EnemyGroup *g,const Clearing *map,Enemy *boss,Vector2 player,int skill) {
    NecromancerFight *fight=&g->necromancer;
    boss->state=ENEMY_ATTACK;
    boss->stateTime=0;
    boss->bossAttack=skill;
    boss->attackConnected=false;
    Vector2 delta=Vector2Subtract(player,boss->position);
    boss->attackDirection=Vector2LengthSqr(delta)>1 ? Vector2Normalize(delta) : (Vector2){0,1};
    boss->facing=Facing(boss->attackDirection);
    if(skill==BONE_BEAM) {
        fight->cageCenter=player;
        fight->beamReach=ClearBeamLength(map,boss->position,boss->attackDirection);
    } else if(skill==CORPSE) {
        fight->markedWraith=-1;
        for(int i=0;i<NECROMANCER_WRAITHS;i++) {
            Enemy *wraith=&g->list[i+1];
            if(WraithAlive(wraith)) {
                fight->markedWraith=i;
                fight->marks[0]=wraith->position;
                break;
            }
        }
    } else if (skill==CIRCLES) {
        fight->marks[0]=player;
        Vector2 travel=Vector2Subtract(player,fight->previousPlayer);
        Vector2 direction=Vector2LengthSqr(travel)>100 ? Vector2Normalize(travel) :
            (Vector2){player.x<836?1:-1,0};
        for(int i=1;i<3;i++)
            if(!FindFreeMapPosition(map,Vector2Add(player,Vector2Scale(direction,i*122)),&fight->marks[i]))
                fight->marks[i]=player;
    } else if (skill==PROFANE_STEP) {
        Vector2 offset={player.x<836?210:-210,-135};
        if (!FindFreeMapPosition(map,Vector2Add(player,offset),&fight->marks[0]))
            fight->marks[0]=boss->position;
    } else if (skill==RUNES) {
        int start=GetRandomValue(0,2);
        for(int i=0;i<3;i++) fight->marks[i]=runes[(start+i)%3];
    }
}

static void UpdateSkill(EnemyGroup *g,const Clearing *map,Enemy *boss,Vector2 player,
                        float dt,bool dodging,int *health) {
    NecromancerFight *fight=&g->necromancer;
    int skill=boss->bossAttack, damage=skills[skill].damage;
    float t=boss->stateTime, before=t-dt, windup=skills[skill].windup;
    Vector2 dir=boss->attackDirection;
    if(skill==CONDEMNED) {
        for(int i=0;i<SPECTRAL_SWORDS;i++) {
            float release=SwordRelease(i);
            if(Crossed(before,t,release-.18f)) fight->marks[i%3]=player;
            if(Crossed(before,t,release)) {
                Vector2 origin=SwordOrigin(boss->position,i,release);
                Shot(g,origin,Aim(origin,fight->marks[i%3]),920,damage,true);
            }
        }
    } else if(skill==BONE_BEAM) {
        if(t>=windup && t<windup+beamActive && !boss->attackConnected &&
           InLane(boss->position,dir,player,fight->beamReach,beamWidth) &&
           !dodging && !g->practiceInvincible && g->playerHitCooldown<=0 &&
           MapHasClearPath(map,boss->position,player)) {
            boss->attackConnected=true;
            Hurt(g,boss,false,health,damage);
        }
    } else if(skill==CHAINS) {
        if(Crossed(before,t,windup) && !dodging && !g->practiceInvincible &&
           InLane(boss->position,dir,player,chainLength,chainWidth) &&
           MapHasClearPath(map,boss->position,player)) boss->attackConnected=true;
        if(boss->attackConnected) {
            fight->pullTime=fmaxf(0,fminf(t,windup+chainPull)-fmaxf(before,windup));
            if(Crossed(before,t,windup+chainPull+.12f)) {
                boss->attackDirection=Aim(boss->position,player);
                boss->facing=Facing(boss->attackDirection);
            }
            if(Crossed(before,t,chainCut))
                Sweep(g,map,boss,player,dodging,health,chainReach,.0f,damage);
        }
    } else if(skill==CORPSE) {
        if(fight->markedWraith>=0 && t>=windup && before<windup) {
            Enemy *wraith=&g->list[fight->markedWraith+1];
            if(wraith->health>0 && wraith->state!=ENEMY_GONE && wraith->state!=ENEMY_DIE) {
                wraith->health=0;
                wraith->state=ENEMY_DIE;
                wraith->stateTime=0;
                boss->attackConnected=true;
                if(Vector2Distance(player,fight->marks[0])<=corpseRadius)
                    Hurt(g,boss,dodging,health,damage);
            }
        }
    } else if (skill==SEPULCHER) {
        if(Crossed(before,t,windup))
            Fan(g,boss->position,dir,5,.20f,700,damage);
        if(Crossed(before,t,windup+.31f))
            Fan(g,boss->position,Aim(boss->position,player),4,.24f,615,damage);
    } else if (skill==CIRCLES) {
        for(int i=0;i<3;i++) if(Crossed(before,t,windup+i*circleCadence) &&
            Vector2Distance(player,fight->marks[i])<=93)
            Hurt(g,boss,dodging,health,damage);
    } else if (skill==STAFF && Crossed(before,t,windup)) {
        SummonWraiths(g,map,player);
    } else if (skill==PROFANE_STEP) {
        if(Crossed(before,t,windup)) {
            boss->position=fight->marks[0];
            Vector2 aim=Vector2Subtract(player,boss->position);
            if (Vector2LengthSqr(aim)>1) boss->attackDirection=Vector2Normalize(aim);
            boss->facing=Facing(boss->attackDirection);
        }
        if(Crossed(before,t,windup+.27f))
            Fan(g,boss->position,boss->attackDirection,5,.22f,760,damage);
    } else if (skill==HALO) {
        if(Crossed(before,t,windup))
            Fan(g,boss->position,dir,7,.18f,665,damage);
        if(Crossed(before,t,windup+.36f))
            Fan(g,boss->position,Vector2Rotate(dir,.09f),6,.18f,745,damage);
    } else if (skill==RUNES) {
        int count=2;
        if(Crossed(before,t,windup))
            SummonWraiths(g,map,player);
        for(int i=0;i<count;i++) {
            float radius=190, age=t-windup-i*runeCadence;
            if (age<0 || age>runeTravel) continue;
            float distance=Vector2Distance(player,fight->marks[i]);
            float ring=radius*age/runeTravel;
            float oldRing=radius*fmaxf(0,age-dt)/runeTravel;
            if (distance>=oldRing-14 && distance<=ring+14)
                Hurt(g,boss,dodging,health,damage);
        }
    } else if (skill==BLADE) {
        for(int i=0;i<3;i++) {
            if(i>0 && Crossed(before,t,bladeTimes[i]-.24f)) {
                boss->attackDirection=Aim(boss->position,player);
                boss->facing=Facing(boss->attackDirection);
            }
            if(Crossed(before,t,bladeTimes[i]))
                Sweep(g,map,boss,player,dodging,health,bladeReach[i],.0f,damage+i*8);
        }
    } else if (skill==FLIGHT) {
        float active=fmaxf(0,fminf(t,flightStop)-fmaxf(before,windup));
        if(active>0) {
            Vector2 origin=boss->position;
            MoveEnemy(map,boss,Vector2Scale(dir,active*1190));
            Vector2 travel=Vector2Subtract(boss->position,origin);
            float length=Vector2LengthSqr(travel);
            float along=length>0 ? Clamp(Vector2DotProduct(Vector2Subtract(player,origin),travel)/length,0,1) : 0;
            Vector2 nearest=Vector2Add(origin,Vector2Scale(travel,along));
            if(!boss->attackConnected && !dodging && Vector2Distance(player,nearest)<72 && MapHasClearPath(map,nearest,player)) {
                boss->attackConnected=true;
                Hurt(g,boss,dodging,health,damage);
            }
        }
    }
    if(skill==FLIGHT && Crossed(before,t,flightImpact) &&
       !boss->attackConnected && Vector2Distance(player,boss->position)<=flightRadius &&
       MapHasClearPath(map,boss->position,player))
        Hurt(g,boss,dodging,health,78);

    if(t>=skills[skill].duration || (skill==CHAINS && !boss->attackConnected && t>=1.32f)) {
        boss->state=ENEMY_IDLE;
        boss->stateTime=0;
        boss->attackCooldown=boss->necromancerForm==3?.13f:.27f;
    }
}

void UpdateNecromancer(EnemyGroup *g,const Clearing *map,Vector2 player,float dt,bool dodging,int *health) {
    Enemy *boss=&g->list[0];
    NecromancerFight *fight=&g->necromancer;
    if(dt<=0 || boss->state==ENEMY_GONE || boss->state==ENEMY_FALLEN) return;
    fight->pullTime=0;
    g->playerHitCooldown=fmaxf(0,g->playerHitCooldown-dt);
    boss->stateTime+=dt;
    if(boss->guardianPhase==GUARDIAN_PHASE_TRANSITION || boss->state==ENEMY_DIE) {
        for(int i=0;i<NECROMANCER_SHOTS;i++) fight->shots[i].life=0;
        for(int i=0;i<NECROMANCER_WRAITHS;i++) g->list[i+1].state=ENEMY_GONE;
        fight->automaticClock=1.65f;
        fight->automaticVolley=0;
        if(boss->guardianPhase==GUARDIAN_PHASE_TRANSITION && boss->stateTime>=1.22f) {
            boss->necromancerForm++;
            boss->guardianPhase=GUARDIAN_PHASE_ONE;
            boss->stateTime=0;
            boss->attackCooldown=.62f;
            fight->rotation=0;
        } else if(boss->state==ENEMY_DIE && boss->stateTime>=collapseDuration) {
            boss->state=ENEMY_FALLEN;
        }
        return;
    }
    if(boss->state==ENEMY_SPAWNING) {
        if(boss->stateTime<1.2f) return;
        MoveEnemy(map,boss,(Vector2){0,105*dt});
        boss->walkTime+=dt;
        if(boss->stateTime>=2.05f) {boss->state=ENEMY_IDLE;boss->stateTime=0;}
        return;
    }
    UpdateWraiths(g,player,dt);
    fight->sampleClock+=dt;
    if(fight->sampleClock>=.34f) {
        fight->previousPlayer=fight->lastPlayer;
        fight->lastPlayer=player;
        fight->sampleClock=0;
    }
    if(boss->state==ENEMY_ATTACK) {
        UpdateSkill(g,map,boss,player,dt,dodging,health);
    } else {
        Vector2 delta=Vector2Subtract(player,boss->position);
        float distance=Vector2Length(delta);
        int form=boss->necromancerForm;
        if(distance>1) boss->facing=Facing(delta);
        boss->attackCooldown=fmaxf(0,boss->attackCooldown-dt);
        static const int order[3][6]={
            {SEPULCHER,CIRCLES,STAFF,CONDEMNED,SEPULCHER,CIRCLES},
            {PROFANE_STEP,HALO,RUNES,CORPSE,BONE_BEAM,PROFANE_STEP},
            {BLADE,FLIGHT,CHAINS,BLADE,FLIGHT,CHAINS}};
        int skill=order[form-1][fight->rotation%6];
        if(skill==CORPSE && !WraithAlive(&g->list[1]) && !WraithAlive(&g->list[2])) skill=HALO;
        float desired=form==3?118:365;
        if(skill==BLADE && distance>258 && MapHasClearPath(map,boss->position,player)) skill=FLIGHT;
        if(boss->attackCooldown<=0 && (skill!=BLADE || distance<=192)) {
            fight->rotation++;
            StartSkill(g,map,boss,player,skill);
        } else {
            bool retreat=form<3 && distance<245;
            boss->state=distance>desired || retreat ? ENEMY_WALK : ENEMY_IDLE;
            if (boss->state==ENEMY_WALK && distance>1) {
                boss->walkTime+=dt*(form==3?1.45f:1);
                float pace=form==3?2.40f:retreat?-1.02f:1.12f;
                MoveEnemy(map,boss,Vector2Scale(delta,dt*boss->speed*pace/distance));
            }
        }
    }
    if(boss->necromancerForm==3 && ActiveSkill(boss,CHAINS)) {
        fight->automaticClock=fmaxf(fight->automaticClock,.49f);
    } else if(boss->necromancerForm==3) {
        float before=fight->automaticClock;
        fight->automaticClock-=dt;
        if(before>automaticWarning && fight->automaticClock<=automaticWarning)
            fight->automaticAim=player;
        if(fight->automaticClock<=0) {
            Vector2 dir=Aim(boss->position,fight->automaticAim);
            bool wide=fight->automaticVolley++%2==1;
            Fan(g,boss->position,dir,wide?11:7,wide?.25f:.16f,wide?720:880,wide?35:43);
            fight->automaticClock=automaticPeriod;
        }
    }
    for(int i=0;i<NECROMANCER_SHOTS;i++) {
        NecromancerShot *shot=&fight->shots[i];
        if(shot->life<=0) continue;
        int steps=(int)(Vector2Length(shot->velocity)*dt/9)+1;
        Vector2 step=Vector2Scale(shot->velocity,dt/steps);
        shot->life-=dt;
        for(int j=0;j<steps && shot->life>0;j++) {
            shot->position=Vector2Add(shot->position,step);
            if(IsClearingBlocked(map,shot->position)) shot->life=0;
            else if(Vector2Distance(shot->position,player)<=28) {
                shot->life=0;
                Hurt(g,boss,dodging,health,shot->damage);
            }
        }
    }
}

static void DrawCombatSprite(Texture2D texture,int row,int frame,Vector2 center,float size,float angle,Color tint) {
    float width=texture.width/6.0f, height=texture.height/4.0f;
    DrawTexturePro(texture,(Rectangle){frame*width,row*height,width,height},
                   (Rectangle){center.x,center.y,size,size},(Vector2){size*.5f,size*.5f},angle,tint);
}

enum { ART_SWORD, ART_BONE, ART_CHAIN, ART_SKULL, ART_SHOCKWAVE };

static void DrawComboArt(const EnemyGroup *g,int art,Vector2 point,Vector2 extent,
                         Vector2 anchor,float angle,Color color) {
    Texture2D texture=g->necromancerCombo;
    DrawTexturePro(texture,(Rectangle){art*256,0,256,256},
                   (Rectangle){point.x,point.y,extent.x,extent.y},anchor,angle,color);
}

static void DrawShockwave(const EnemyGroup *g,Vector2 center,float radius,float age) {
    if(age<0 || age>=1) return;
    float reach=radius*(.26f+.84f*sqrtf(age));
    Vector2 half={reach,reach*.70f};
    DrawComboArt(g,ART_SHOCKWAVE,center,Vector2Scale(half,2),half,0,Fade(WHITE,1-age));
    DrawRing(center,reach*.87f,reach,0,360,48,Fade(VIOLET,.74f*(1-age)));
}

static float ClearBeamLength(const Clearing *map,Vector2 origin,Vector2 dir) {
    for(float distance=8;distance<beamLength;distance+=8)
        if(IsClearingBlocked(map,Vector2Add(origin,Vector2Scale(dir,distance))))
            return distance;
    return beamLength;
}

static void DrawLane(Vector2 origin,Vector2 dir,float length,float width,float opacity) {
    Vector2 end=Vector2Add(origin,Vector2Scale(dir,length));
    DrawLineEx(origin,end,width*2,Fade(PURPLE,opacity*.27f));
    DrawLineEx(origin,end,3,Fade(VIOLET,opacity));
}

static void DrawComboFloor(const EnemyGroup *g,const Enemy *boss) {
    const NecromancerFight *fight=&g->necromancer;
    Vector2 origin=boss->position, dir=boss->attackDirection;
    float t=boss->stateTime, hit=skills[boss->bossAttack].windup;
    float heading=atan2f(dir.y,dir.x)*RAD2DEG;
    if(boss->bossAttack==BONE_BEAM) {
        Vector2 center=fight->cageCenter;
        if(t<cageLifespan)
            DrawRing(center,cageRadius-cageThickness,cageRadius+cageThickness,0,360,64,
                     Fade(VIOLET,t<cageClose?.34f:.65f));
        if(t<hit+beamActive) DrawLane(origin,dir,fight->beamReach,beamWidth,.78f);
    } else if(boss->bossAttack==CHAINS) {
        if(t<hit) DrawLane(origin,dir,chainLength,chainWidth,.46f+.40f*t/hit);
        if(boss->attackConnected && t>=hit+chainPull+.12f && t<chainCut+.24f) {
            if(t<chainCut)
                DrawCircleSector(origin,chainReach,heading-90,heading+90,32,Fade(VIOLET,.33f));
            else
                DrawRing(origin,144,chainReach,heading-90,heading+90,32,
                         Fade(VIOLET,1-(t-chainCut)/.24f));
        }
    } else if(boss->bossAttack==CORPSE && fight->markedWraith>=0) {
        const Enemy *wraith=&g->list[fight->markedWraith+1];
        Vector2 mark=fight->marks[0];
        if(t<hit && wraith->health>0) {
            float pulse=.28f+.52f*powf(sinf(PI*3*t/hit),2);
            DrawCircleV(mark,corpseRadius,Fade(PURPLE,pulse*.24f));
            DrawRing(mark,corpseRadius-4,corpseRadius,0,360,48,Fade(VIOLET,pulse));
        } else if(boss->attackConnected && t<hit+.42f) {
            float age=(t-hit)/.42f;
            DrawRing(mark,corpseRadius*age*.8f,corpseRadius*age+8,0,360,48,Fade(VIOLET,1-age));
        }
    }
}

static void DrawComboForeground(const EnemyGroup *g,const Enemy *boss,Vector2 player) {
    const NecromancerFight *fight=&g->necromancer;
    Vector2 origin=boss->position, dir=boss->attackDirection;
    float t=boss->stateTime, hit=skills[boss->bossAttack].windup;
    float heading=atan2f(dir.y,dir.x)*RAD2DEG;
    if(boss->bossAttack==CONDEMNED) {
        for(int i=0;i<SPECTRAL_SWORDS;i++) {
            float release=SwordRelease(i);
            if(t>=release) continue;
            Vector2 point=SwordOrigin(origin,i,t);
            Vector2 aim=t>=release-.18f?Aim(point,fight->marks[i%3]):Aim(origin,point);
            float angle=atan2f(aim.y,aim.x)*RAD2DEG;
            float opacity=Clamp(t*4-i*.07f,0,1);
            point.y-=20;
            Vector2 trace=Vector2Subtract(point,Vector2Scale(aim,29));
            DrawCircleV(trace,5+2*sinf(t*12+i),Fade(VIOLET,opacity*.58f));
            DrawComboArt(g,ART_SWORD,point,(Vector2){88,37},(Vector2){44,18.5f},angle,
                         Fade(t>=release-.18f?WHITE:(Color){185,140,255,255},opacity));
        }
    } else if(boss->bossAttack==BONE_BEAM) {
        float growth=Clamp((t-(cageClose-.19f))/.19f,0,1)*Clamp((cageLifespan-t)/.18f,0,1);
        if(growth>0) for(int i=0;i<20;i++) {
            float angle=2*PI*i/20;
            Vector2 radial={cosf(angle),sinf(angle)};
            Vector2 point=Vector2Add(fight->cageCenter,Vector2Scale(radial,cageRadius));
            DrawComboArt(g,ART_BONE,point,(Vector2){40,88*growth},
                         (Vector2){20,88*growth},0,Fade(WHITE,growth));
        }
        Vector2 focus=Vector2Add(origin,Vector2Scale(dir,26));
        focus.y-=74;
        if(t<hit) {
            float charge=Clamp(t/hit,0,1);
            DrawCombatSprite(g->necromancerEffects,1,(int)(charge*5),focus,52+67*charge,0,
                             Fade(WHITE,.37f+.59f*charge));
        }
        if(t>=hit && t<hit+beamActive) {
            Vector2 end=Vector2Add(origin,Vector2Scale(dir,fight->beamReach));
            float pulse=.8f+.2f*sinf((t-hit)*45);
            DrawLineEx(origin,end,2*beamWidth,Fade(PURPLE,.55f*pulse));
            DrawLineEx(origin,end,26*pulse,Fade(VIOLET,.85f));
            DrawLineEx(origin,end,8*pulse,RAYWHITE);
            DrawLineEx(focus,origin,12*pulse,Fade(VIOLET,.70f));
            DrawCombatSprite(g->necromancerEffects,1,(int)((t-hit)*25)%6,
                             origin,128*pulse,heading,WHITE);
            for(int i=0;i<9;i++) {
                float d=fmodf(i*91+(t-hit)*930,fight->beamReach);
                Vector2 point=Vector2Add(origin,Vector2Scale(dir,d));
                Vector2 side={-dir.y,dir.x};
                point=Vector2Add(point,Vector2Scale(side,22*sinf(i*2.1f+t*15)));
                DrawPoly(point,4,4+(i%3),heading,Fade(WHITE,.75f));
            }
        }
    } else if(boss->bossAttack==CHAINS) {
        float age=t-hit;
        if(age>=-.14f && age<.36f) {
            float length=chainLength*Clamp((age+.14f)/.14f,0,1);
            if(boss->attackConnected) {
                dir=Aim(origin,player);
                heading=atan2f(dir.y,dir.x)*RAD2DEG;
                length=Vector2Distance(origin,player);
            }
            for(float d=0;d<length;d+=64) {
                float size=fminf(64,length-d);
                Vector2 point=Vector2Add(origin,Vector2Scale(dir,d));
                DrawComboArt(g,ART_CHAIN,point,(Vector2){size,22},(Vector2){0,11},heading,
                             Fade(WHITE,Clamp((.36f-age)/.12f,0,1)));
            }
        }
        if(boss->attackConnected && t>=chainCut && t<chainCut+.24f) {
            float age=(t-chainCut)/.24f;
            Vector2 center=Vector2Add(origin,Vector2Scale(boss->attackDirection,59));
            center.y-=36;
            DrawCombatSprite(g->necromancerEffects,3,(int)(age*6),center,204,heading,WHITE);
        }
    } else if(boss->bossAttack==CORPSE && boss->attackConnected && t>=hit && t<hit+.42f) {
        float age=(t-hit)/.42f;
        Vector2 point=fight->marks[0];
        point.y-=24+45*age;
        DrawCombatSprite(g->necromancerEffects,1,(int)(age*6),point,146+80*age,0,Fade(WHITE,1-age));
    }
}

static int BladeStrike(float time) {
    for(int i=2;i>0;i--) if(time>=bladeTimes[i]-.24f) return i;
    return 0;
}

static void DrawFloor(const EnemyGroup *g,const Enemy *boss) {
    const NecromancerFight *fight=&g->necromancer;
    bool sealed=boss->state==ENEMY_GONE;
    bool ready=sealed || boss->state==ENEMY_FALLEN;
    for(int i=0;i<3;i++) if (i<boss->necromancerForm-1 || ready) {
        DrawCircleV(runes[i],20,Fade(sealed?GOLD:MAROON,.32f));
        DrawPolyLines(runes[i],4,22,0,Fade(sealed?GOLD:ORANGE,.72f));
    }
    if (ready) {
        static const Vector2 rim[]={{676,311},{995,311},{1190,416},{1230,540},
            {1028,681},{645,681},{444,541},{487,414}};
        Color color=sealed?GOLD:VIOLET;
        for(int i=0;i<8;i++) DrawLineEx(rim[i],rim[(i+1)%8],3,Fade(color,.65f));
    }
    if(boss->state!=ENEMY_ATTACK) return;
    int skill=boss->bossAttack;
    float t=boss->stateTime, windup=skills[skill].windup;
    float heading=atan2f(boss->attackDirection.y,boss->attackDirection.x)*RAD2DEG;
    Color warning=(Color){237,94,133,255};
    if(skill==FLIGHT) DrawShockwave(g,boss->position,flightRadius,(t-flightImpact)/.40f);
    if(skill==CORPSE && boss->attackConnected)
        DrawShockwave(g,fight->marks[0],corpseRadius,(t-windup)/.42f);
    DrawComboFloor(g,boss);
    if(skill==CIRCLES || skill==RUNES) {
        int count=skill==RUNES?2:3;
        for(int i=0;i<count;i++) {
            Vector2 mark=fight->marks[i];
            float age=t-windup-i*(skill==CIRCLES?circleCadence:runeCadence);
            float range=skill==CIRCLES?93:190;
            if(age<0) {
                float glow=Clamp(1+age*.7f,.20f,.80f);
                DrawCircleV(mark,range,Fade(MAROON,.22f*glow));
                DrawCircleLines((int)mark.x,(int)mark.y,range,Fade(warning,glow));
                DrawPolyLines(mark,4,16+i*5,0,Fade(GOLD,glow));
            } else if (age<runeTravel) {
                float radius=skill==CIRCLES?range:fmaxf(4,range*age/runeTravel);
                DrawRing(mark,fmaxf(0,radius-9),radius,0,360,40,Fade(warning,1-age/runeTravel));
            }
        }
    } else if(skill==PROFANE_STEP && t<windup+.27f) {
        DrawEllipse((int)fight->marks[0].x,(int)fight->marks[0].y,39,18,Fade(PURPLE,.45f));
        DrawCircleLines((int)fight->marks[0].x,(int)fight->marks[0].y,37,Fade(VIOLET,.67f));
    } else if(skill==FLIGHT) {
        if(t<windup) DrawLane(boss->position,boss->attackDirection,678,72,.85f);
        else if(t>=flightStop && t<flightImpact) {
            float charge=(t-flightStop)/(flightImpact-flightStop);
            DrawCircleV(boss->position,flightRadius,Fade(PURPLE,.22f+.14f*charge));
            DrawRing(boss->position,flightRadius-5,flightRadius,0,360,56,Fade(VIOLET,.86f));
            float rim=Lerp(flightRadius,20,charge);
            DrawRing(boss->position,rim-4,rim,0,360,56,Fade(PINK,.82f));
        }
    } else if(skill==BLADE) {
        int strike=BladeStrike(t);
        float age=t-bladeTimes[strike], radius=bladeReach[strike];
        if(age<0) DrawCircleSector(boss->position,radius,heading-90,heading+90,32,Fade(warning,.34f));
        else if(age<.24f) DrawRing(boss->position,radius*.65f,radius,heading-90,heading+90,
                                   32,Fade(warning,1-age/.24f));
    } else if((skill==SEPULCHER || skill==HALO) && t<windup) {
        DrawCircleV(boss->position,24+22*t/windup,Fade(skill==HALO?GOLD:warning,.28f));
        if(skill==HALO)
            for(int i=0;i<7;i++) {
                Vector2 shard=Vector2Add(boss->position,Vector2Scale(Vector2Rotate(boss->attackDirection,(i-3)*.18f),62));
                DrawPoly(shard,4,6,45,Fade(GOLD,.8f));
            }
    }
}

void DrawNecromancer(const EnemyGroup *g,Vector2 player,bool foreground) {
    const Enemy *boss=&g->list[0];
    const NecromancerFight *fight=&g->necromancer;
    if(!foreground) DrawFloor(g,boss);
    for(int i=0;i<NECROMANCER_WRAITHS;i++) {
        const Enemy *wraith=&g->list[i+1];
        if(!wraith->necromancerWraith || wraith->state==ENEMY_GONE) continue;
        float age=wraith->walkTime;
        float opacity=wraith->state==ENEMY_DIE ? 1-wraith->stateTime/.32f : Clamp(age*4,0,1);
        Vector2 feet=wraith->position;
        if(!foreground) {
            int frame=(int)Clamp(age*9,0,5);
            float glow=wraith->attackCooldown<.25f?.92f:.58f;
            DrawCombatSprite(g->necromancerEffects,1,frame,feet,126,0,Fade(WHITE,glow*opacity));
            DrawCircleLinesV(feet,29,Fade(VIOLET,glow*opacity));
        }
        bool marked=ActiveSkill(boss,CORPSE) && fight->markedWraith==i &&
            boss->stateTime<skills[CORPSE].windup && wraith->health>0;
        if((feet.y>=player.y)==foreground) {
            Vector2 center={feet.x,feet.y-37+7*sinf(age*5)};
            if(marked) {
                float pulse=.5f+.5f*powf(sinf(PI*3*boss->stateTime/skills[CORPSE].windup),2);
                DrawComboArt(g,ART_SKULL,center,(Vector2){84,100},(Vector2){42,50},0,Fade(WHITE,pulse));
            } else DrawCombatSprite(g->necromancerEffects,2,(int)(age*10)%6,center,105,0,Fade(WHITE,opacity));
            DrawEnemyHealthBar(wraith,center.y-48);
        }
    }
    if(foreground && boss->necromancerForm==3 && boss->health>0 &&
       boss->guardianPhase!=GUARDIAN_PHASE_TRANSITION && fight->automaticClock<=automaticWarning) {
        float age=1-fight->automaticClock/automaticWarning;
        Vector2 cast={boss->position.x,boss->position.y-136};
        DrawCombatSprite(g->necromancerEffects,1,(int)Clamp(age*6,0,5),cast,142,0,WHITE);
        DrawPoly(cast,4,8+19*age,age*90,Fade(VIOLET,.85f));
        Vector2 direction=Aim(boss->position,fight->automaticAim);
        for(int i=0;i<3;i++) {
            Vector2 mote=Vector2Add(cast,Vector2Scale(direction,27+18*i+13*age));
            DrawPoly(mote,4,5-i,45,Fade(RAYWHITE,.78f));
        }
    }
    bool defeated=boss->state==ENEMY_DIE || boss->state==ENEMY_FALLEN;
    bool bossLayer=(boss->position.y>=player.y)==foreground;
    if(defeated && bossLayer) {
        Texture2D texture=g->necromancerCollapse;
        float age=boss->state==ENEMY_FALLEN?collapseDuration:boss->stateTime;
        int frame=age<.26f?0:age<.62f?1:age<.96f?2:3;
        float tile=texture.width/4.0f;
        Vector2 foot=boss->position;
        DrawEllipse((int)foot.x,(int)foot.y,48,15,Fade(BLACK,.58f));
        DrawTexturePro(texture,(Rectangle){frame*tile,0,tile,texture.height},
                       (Rectangle){foot.x,foot.y,320,240},(Vector2){160,218},0,WHITE);
    } else if(boss->state!=ENEMY_GONE && bossLayer) {
        int form=boss->necromancerForm-1;
        static const int facingRows[]={0,3,1,2};
        int row=facingRows[boss->facing], frame=0;
        bool action=boss->state==ENEMY_ATTACK;
        Texture2D texture=g->necromancerWalk[form];
        float t=boss->stateTime, offset=0, alpha=1;
        bool throne=boss->state==ENEMY_SPAWNING && t<1.2f;
        if(throne) {
            texture=g->necromancerThrone;
            row=0;
            frame=(int)Clamp((t-.32f)*4.2f,0,3);
            offset= -26 + frame*8;
        } else if(boss->state==ENEMY_WALK || boss->state==ENEMY_SPAWNING)
            frame=boss->walkTime>0?1+(int)(boss->walkTime*7)%2:0;
        if(action) {
            int skill=boss->bossAttack;
            float windup=skills[skill].windup;
            texture=g->necromancerSkills[form];
            row=skill-(form==0?SEPULCHER:form==1?PROFANE_STEP:BLADE);
            if(skill==CONDEMNED) row=2;
            if(skill==BONE_BEAM) row=1;
            if(skill==CORPSE) row=2;
            if(skill==CHAINS) row=2;
            float impact=windup;
            frame=t<impact*.55f?0:t<impact?1:t<impact+.24f?2:3;
            if(skill==CONDEMNED && t>=windup && t<SwordRelease(SPECTRAL_SWORDS-1)+.15f)
                frame=fmodf(t-windup,.205f)<.11f?2:1;
            if(skill==BONE_BEAM && t>=windup && t<windup+beamActive) frame=2;
            if(skill==CHAINS && boss->attackConnected && t<chainCut+.27f)
                frame=t<windup?1:t<windup+chainPull?2:1;
            if(skill==FLIGHT && t>=impact)
                frame=t<flightStop?2:t<flightImpact?1:t<flightImpact+.17f?2:3;
            if (skill==PROFANE_STEP) frame=t<windup*.55f?0:t<windup+.27f?1:t<1.15f?2:3;
            if ((skill==RUNES) && t>=impact && t<impact+.82f) frame=2;
            if(skill==FLIGHT && t>=windup && t<flightStop)
                offset=-72*sinf(PI*(t-windup)/(flightStop-windup));
            if(skill==PROFANE_STEP && t<windup) alpha=1-.84f*t/windup;
        } else if(boss->guardianPhase==GUARDIAN_PHASE_TRANSITION) {
            float glow=Clamp(t/1.22f,0,1);
            DrawRing(boss->position,38+glow*126,47+glow*126,0,360,36,Fade(VIOLET,1-glow));
            texture=g->necromancerSkills[form];
            row=3; frame=(int)Clamp(t*3.1f,0,3);
        }
        bool chainCutting=action && boss->bossAttack==CHAINS && boss->attackConnected &&
            t>=skills[CHAINS].windup+chainPull+.12f && t<chainCut+.42f;
        bool blade=action && (boss->bossAttack==BLADE || chainCutting);
        if(blade) {
            texture=g->necromancerBlade;
            row=facingRows[boss->facing];
            float hit=chainCutting?chainCut:bladeTimes[BladeStrike(t)];
            float age=t-hit;
            frame=age<-.22f?0:age<-.09f?1:age<0?2:age<.10f?3:age<.24f?4:5;
        }
        float tile=texture.width/(blade?6.0f:4.0f);
        float height=throne?texture.height:texture.height*.25f;
        float scale=(throne ? 208.0f : blade ? 275.0f : 240.0f)/256;
        float ground=blade?240:throne || texture.id==g->necromancerWalk[form].id?230:310;
        bool flip=action && !blade && boss->facing==2;
        if (!throne) DrawEllipse((int)boss->position.x,(int)boss->position.y,36,13,Fade(BLACK,.55f*alpha));
        DrawTexturePro(texture,(Rectangle){frame*tile,row*height,flip?-tile:tile,height},
                       (Rectangle){boss->position.x,boss->position.y+offset,tile*scale,height*scale},
                       (Vector2){tile*.5f*scale,ground*scale},0,Fade(WHITE,alpha));
    }
    if(foreground && boss->state==ENEMY_ATTACK &&
       (boss->bossAttack==BLADE || boss->bossAttack==FLIGHT)) {
        float hit=boss->bossAttack==FLIGHT?skills[FLIGHT].windup:
            bladeTimes[BladeStrike(boss->stateTime)];
        float age=(boss->stateTime-hit)/.24f;
        if(age>=0 && age<1) {
            Vector2 position=Vector2Add(boss->position,Vector2Scale(boss->attackDirection,59));
            position.y-=36;
            float heading=atan2f(boss->attackDirection.y,boss->attackDirection.x)*RAD2DEG;
            DrawCombatSprite(g->necromancerEffects,3,(int)(age*6),position,250,heading,WHITE);
        }
    }
    if(foreground && boss->state==ENEMY_ATTACK) DrawComboForeground(g,boss,player);
    if(foreground) for(int i=0;i<NECROMANCER_SHOTS;i++) {
        const NecromancerShot *shot=&g->necromancer.shots[i];
        if(shot->life<=0) continue;
        Vector2 center={shot->position.x,shot->position.y-20};
        Vector2 tail=Vector2Subtract(center,Vector2Scale(shot->velocity,.060f));
        Color tint=boss->necromancerForm==2?GOLD:VIOLET;
        float heading=atan2f(shot->velocity.y,shot->velocity.x)*RAD2DEG;
        if(shot->sword) {
            DrawLineEx(tail,center,4,Fade(VIOLET,.62f));
            DrawComboArt(g,ART_SWORD,center,(Vector2){72,31},(Vector2){36,15.5f},heading,WHITE);
            continue;
        }
        DrawLineEx(tail,center,6,Fade(tint,.44f));
        DrawCircleV(center,8,Fade(RAYWHITE,.78f));
        DrawCombatSprite(g->necromancerEffects,0,(int)((2.7f-shot->life)*18)%6,center,76,heading,WHITE);
    }
}
