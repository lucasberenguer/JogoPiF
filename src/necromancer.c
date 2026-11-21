#include "enemy.h"
#include "raymath.h"
#include <math.h>

enum {
    SEPULCHER, CIRCLES, STAFF, PROFANE_STEP, HALO, RUNES, BLADE, FLIGHT, WINGS, JUDGMENT
};

static const Vector2 sealCenter = {836, 480};
static const Vector2 runes[] = {{548,382}, {1126,382}, {836,677}};
static const struct {
    float windup, duration;
    int damage;
} skills[] = {
    [SEPULCHER] = {.68f, 1.38f, 14}, [CIRCLES] = {.78f, 1.92f, 19},
    [STAFF] = {.55f, 1.39f, 20}, [PROFANE_STEP] = {.65f, 1.44f, 16},
    [HALO] = {.70f, 1.75f, 15}, [RUNES] = {.86f, 2.36f, 21},
    [BLADE] = {.45f, 2.14f, 18}, [FLIGHT] = {.72f, 1.96f, 26},
    [WINGS] = {.62f, 1.55f, 20}, [JUDGMENT] = {.88f, 2.87f, 26}
};

bool LoadNecromancerSprites(EnemyGroup *group) {
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
    if (group->necromancerThrone.id) UnloadTexture(group->necromancerThrone);
    if (group->necromancerPortrait.id) UnloadTexture(group->necromancerPortrait);
    for (int i=0;i<NECROMANCER_FORMS;i++) {
        if (group->necromancerWalk[i].id) UnloadTexture(group->necromancerWalk[i]);
        if (group->necromancerSkills[i].id) UnloadTexture(group->necromancerSkills[i]);
    }
}

void StartNecromancer(EnemyGroup *group, float power) {
    int hp = (int)(4200 * fmaxf(1,power));
    group->list[0]=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_SPAWNING,.necromancerForm=1,
        .position={836,263},.speed=140,.power=power,.health=hp,.maxHealth=hp,
        .attackCooldown=.65f,.lastKatanaSwing=-1};
    group->necromancer=(NecromancerFight){.lastPlayer={836,845},.previousPlayer={836,845}};
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

static void Shot(EnemyGroup *g,Vector2 origin,Vector2 direction,float speed,int damage) {
    for(int i=0;i<NECROMANCER_SHOTS;i++) if (g->necromancer.shots[i].life<=0) {
        g->necromancer.shots[i]=(NecromancerShot){origin,Vector2Scale(direction,speed),2.7f,damage};
        return;
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

static void StartSkill(EnemyGroup *g,const Clearing *map,Enemy *boss,Vector2 player,int skill) {
    NecromancerFight *fight=&g->necromancer;
    boss->state=ENEMY_ATTACK;
    boss->stateTime=0;
    boss->bossAttack=skill;
    boss->attackConnected=false;
    Vector2 delta=Vector2Subtract(player,boss->position);
    boss->attackDirection=Vector2LengthSqr(delta)>1 ? Vector2Normalize(delta) : (Vector2){0,1};
    boss->facing=Facing(boss->attackDirection);
    if (skill==CIRCLES) {
        fight->marks[0]=fight->previousPlayer;
        fight->marks[1]=fight->lastPlayer;
        if (Vector2Distance(fight->marks[0],fight->marks[1])<100) {
            Vector2 offset={player.x<836?125:-125,0};
            if (!FindFreeMapPosition(map,Vector2Add(player,offset),&fight->marks[1]))
                fight->marks[1]=player;
        }
    } else if (skill==PROFANE_STEP) {
        Vector2 offset={player.x<836?210:-210,-135};
        if (!FindFreeMapPosition(map,Vector2Add(player,offset),&fight->marks[0]))
            fight->marks[0]=boss->position;
    } else if (skill==RUNES || skill==JUDGMENT) {
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
    if (skill==SEPULCHER && Crossed(before,t,windup)) {
        for(int i=-1;i<=1;i++) Shot(g,boss->position,Vector2Rotate(dir,i*.36f),365,damage);
    } else if (skill==CIRCLES) {
        for(int i=0;i<2;i++) if(Crossed(before,t,windup+i*.46f) &&
            Vector2Distance(player,fight->marks[i])<=93)
            Hurt(g,boss,dodging,health,damage);
    } else if (skill==STAFF && Crossed(before,t,windup)) {
        Sweep(g,map,boss,player,dodging,health,138,.06f,damage);
    } else if (skill==PROFANE_STEP) {
        if(Crossed(before,t,windup)) {
            boss->position=fight->marks[0];
            Vector2 aim=Vector2Subtract(player,boss->position);
            if (Vector2LengthSqr(aim)>1) boss->attackDirection=Vector2Normalize(aim);
            boss->facing=Facing(boss->attackDirection);
        }
        if(Crossed(before,t,windup+.27f))
            Shot(g,boss->position,boss->attackDirection,475,damage);
    } else if (skill==HALO) {
        for(int i=0;i<5;i++) if(i!=2 && Crossed(before,t,windup+i*.105f))
            Shot(g,boss->position,Vector2Rotate(dir,(i-2)*.37f),410,damage);
    } else if (skill==RUNES || skill==JUDGMENT) {
        int count=skill==RUNES?2:3;
        for(int i=0;i<count;i++) {
            float radius=190, age=t-windup-i*.41f;
            if (age<0 || age>.54f) continue;
            float distance=Vector2Distance(player,fight->marks[i]);
            float ring=radius*age/.54f;
            float oldRing=radius*fmaxf(0,age-dt)/.54f;
            if (distance>=oldRing-14 && distance<=ring+14)
                Hurt(g,boss,dodging,health,damage);
        }
    } else if (skill==BLADE) {
        if (Crossed(before,t,.45f) || Crossed(before,t,.73f))
            Sweep(g,map,boss,player,dodging,health,155,.08f,damage);
        if (Crossed(before,t,1.38f))
            Sweep(g,map,boss,player,dodging,health,181,-.10f,damage+8);
    } else if (skill==FLIGHT) {
        float active=fmaxf(0,fminf(t,1.35f)-fmaxf(before,windup));
        if(active>0) {
            Vector2 origin=boss->position;
            MoveEnemy(map,boss,Vector2Scale(dir,active*850));
            Vector2 travel=Vector2Subtract(boss->position,origin);
            float length=Vector2LengthSqr(travel);
            float along=length>0 ? Clamp(Vector2DotProduct(Vector2Subtract(player,origin),travel)/length,0,1) : 0;
            Vector2 nearest=Vector2Add(origin,Vector2Scale(travel,along));
            if(!boss->attackConnected && Vector2Distance(player,nearest)<67 && MapHasClearPath(map,nearest,player)) {
                boss->attackConnected=true;
                Hurt(g,boss,dodging,health,damage);
            }
        }
    } else if (skill==WINGS && Crossed(before,t,windup)) {
        Vector2 normal={-dir.y,dir.x};
        for(int side=-1;side<=1;side+=2)
            for(int i=-1;i<=1;i++)
                Shot(g,boss->position,Vector2Rotate(Vector2Scale(normal,(float)side),i*.31f),390,damage);
    }
    if(t>=skills[skill].duration) {
        boss->state=ENEMY_IDLE;
        boss->stateTime=0;
        boss->attackCooldown=boss->necromancerForm==1?.40f:.30f;
    }
}

void UpdateNecromancer(EnemyGroup *g,const Clearing *map,Vector2 player,float dt,bool dodging,int *health) {
    Enemy *boss=&g->list[0];
    NecromancerFight *fight=&g->necromancer;
    if(dt<=0 || boss->state==ENEMY_GONE || boss->state==ENEMY_FALLEN) return;
    g->playerHitCooldown=fmaxf(0,g->playerHitCooldown-dt);
    boss->stateTime+=dt;
    if(boss->guardianPhase==GUARDIAN_PHASE_TRANSITION || boss->state==ENEMY_DIE) {
        for(int i=0;i<NECROMANCER_SHOTS;i++) fight->shots[i].life=0;
        if(boss->guardianPhase==GUARDIAN_PHASE_TRANSITION && boss->stateTime>=1.22f) {
            boss->necromancerForm++;
            boss->guardianPhase=GUARDIAN_PHASE_ONE;
            boss->stateTime=0;
            boss->attackCooldown=.62f;
            fight->rotation=0;
        } else if(boss->state==ENEMY_DIE && boss->stateTime>=1.15f) {
            boss->state=ENEMY_FALLEN;
            boss->position=sealCenter;
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
        static const int order[3][4]={{SEPULCHER,CIRCLES,STAFF,STAFF},
            {PROFANE_STEP,HALO,RUNES,RUNES},{BLADE,FLIGHT,WINGS,JUDGMENT}};
        int span=form==3?4:3;
        int skill=order[form-1][fight->rotation%span];
        float desired=form==3?144:275;
        if(skill==STAFF) desired=115;
        if(boss->attackCooldown<=0 && (distance<=desired+25 || skill==PROFANE_STEP ||
            skill==FLIGHT || skill==CIRCLES || skill==RUNES || skill==JUDGMENT)) {
            fight->rotation++;
            StartSkill(g,map,boss,player,skill);
        } else {
            boss->state=distance>desired?ENEMY_WALK:ENEMY_IDLE;
            if (boss->state==ENEMY_WALK) {
                boss->walkTime+=dt;
                MoveEnemy(map,boss,Vector2Scale(delta,dt*boss->speed*(form==3?1.55f:1)/distance));
            }
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
    if(skill==CIRCLES || skill==RUNES || skill==JUDGMENT) {
        int count=skill==JUDGMENT?3:2;
        for(int i=0;i<count;i++) {
            Vector2 mark=fight->marks[i];
            float age=t-windup-i*(skill==CIRCLES?.46f:.41f);
            float range=skill==CIRCLES?93:190;
            if(age<0) {
                float glow=Clamp(1+age*.7f,.20f,.80f);
                DrawCircleV(mark,range,Fade(MAROON,.22f*glow));
                DrawCircleLines((int)mark.x,(int)mark.y,range,Fade(warning,glow));
                DrawPolyLines(mark,4,16+i*5,0,Fade(GOLD,glow));
            } else if (age<.54f) {
                float radius=skill==CIRCLES?range:fmaxf(4,range*age/.54f);
                DrawRing(mark,fmaxf(0,radius-9),radius,0,360,40,Fade(warning,1-age/.54f));
            }
        }
    } else if(skill==PROFANE_STEP && t<windup+.27f) {
        DrawEllipse((int)fight->marks[0].x,(int)fight->marks[0].y,39,18,Fade(PURPLE,.45f));
        DrawCircleLines((int)fight->marks[0].x,(int)fight->marks[0].y,37,Fade(VIOLET,.67f));
    } else if(skill==FLIGHT && t<windup) {
        Vector2 end=Vector2Add(boss->position,Vector2Scale(boss->attackDirection,535));
        DrawLineEx(boss->position,end,114,Fade(MAROON,.27f));
        DrawLineEx(boss->position,end,3,Fade(warning,.78f));
    } else if(skill==WINGS && t<windup) {
        for(int side=-1;side<=1;side+=2)
            DrawCircleSector(boss->position,240,heading+side*90-18,heading+side*90+18,18,Fade(warning,.36f));
    } else if(skill==STAFF || skill==BLADE) {
        float hit=skill==STAFF?windup:t<.62f?.45f:t<.98f?.73f:1.38f;
        float age=t-hit;
        float radius=skill==STAFF?138:hit==1.38f?181:155;
        if(age<0) DrawCircleSector(boss->position,radius,heading-85,heading+85,26,Fade(warning,.27f));
        else if(age<.22f) DrawRing(boss->position,radius*.69f,radius,heading-85,heading+85,
                                   26,Fade(warning,1-age/.22f));
    } else if((skill==SEPULCHER || skill==HALO) && t<windup) {
        DrawCircleV(boss->position,24+22*t/windup,Fade(skill==HALO?GOLD:warning,.28f));
        if(skill==HALO)
            for(int i=0;i<5;i++) {
                Vector2 shard=Vector2Add(boss->position,Vector2Scale(Vector2Rotate(boss->attackDirection,(i-2)*.37f),62));
                DrawPoly(shard,4,6,45,Fade(i==2?DARKGRAY:GOLD,.8f));
            }
    }
}

void DrawNecromancer(const EnemyGroup *g,Vector2 player,bool foreground) {
    const Enemy *boss=&g->list[0];
    if(!foreground) DrawFloor(g,boss);
    if(boss->state!=ENEMY_GONE && boss->state!=ENEMY_FALLEN &&
       (boss->position.y>=player.y)==foreground) {
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
            float impact=skill==BLADE && t>=.98f?1.38f:windup;
            frame=t<impact*.55f?0:t<impact?1:t<impact+.24f?2:3;
            if (skill==BLADE && t>=.65f && t<.98f) frame=t<.73f?1:2;
            if (skill==FLIGHT && t>=impact && t<1.35f) frame=2;
            if (skill==PROFANE_STEP) frame=t<windup*.55f?0:t<windup+.27f?1:t<1.15f?2:3;
            if ((skill==RUNES || skill==JUDGMENT) && t>=impact && t<impact+.82f) frame=2;
            if(skill==FLIGHT && t>=windup && t<1.35f)
                offset=-42*sinf(PI*(t-windup)/(1.35f-windup));
            if(skill==PROFANE_STEP && t<windup) alpha=1-.84f*t/windup;
        } else if(boss->guardianPhase==GUARDIAN_PHASE_TRANSITION) {
            float glow=Clamp(t/1.22f,0,1);
            DrawRing(boss->position,38+glow*126,47+glow*126,0,360,36,Fade(VIOLET,1-glow));
            texture=g->necromancerSkills[form];
            row=3; frame=(int)Clamp(t*3.1f,0,3);
        }
        if(boss->state==ENEMY_DIE) alpha=Clamp(1-t/1.15f,0,1);
        float tile=texture.width*.25f, height=throne?texture.height:texture.height*.25f;
        float scale=208.0f/256, ground=throne || texture.id==g->necromancerWalk[form].id?230:310;
        bool flip=action && boss->facing==2;
        if (!throne) DrawEllipse((int)boss->position.x,(int)boss->position.y,36,13,Fade(BLACK,.55f*alpha));
        DrawTexturePro(texture,(Rectangle){frame*tile,row*height,flip?-tile:tile,height},
                       (Rectangle){boss->position.x,boss->position.y+offset,tile*scale,height*scale},
                       (Vector2){tile*.5f*scale,ground*scale},0,Fade(WHITE,alpha));
    }
    if(foreground) for(int i=0;i<NECROMANCER_SHOTS;i++) {
        const NecromancerShot *shot=&g->necromancer.shots[i];
        if(shot->life<=0) continue;
        Vector2 center={shot->position.x,shot->position.y-20};
        Vector2 tail=Vector2Subtract(center,Vector2Scale(shot->velocity,.060f));
        Color tint=boss->necromancerForm==2?GOLD:(Color){220,50,119,255};
        DrawLineEx(tail,center,9,Fade(tint,.65f));
        DrawCircleV(center,12,Fade(tint,.7f));
        DrawPoly(center,4,7,45,RAYWHITE);
    }
}
