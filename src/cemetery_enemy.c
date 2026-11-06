#include "enemy.h"
#include "raymath.h"
#include <math.h>
#define ENEMY_PROJECTILES 32
#define ENEMY_MAX_HEALTH 60
#define ENEMY_SPEED 92.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12

static Vector2 CemeterySpawn(const Clearing *map,Vector2 player) {
    for (int n=0;n<800;n++) {
        Vector2 point={GetRandomValue((int)(map->background.width*.12f),(int)(map->background.width*.86f)),
            GetRandomValue((int)(map->background.height*.24f),(int)(map->background.height*.84f))};
        if (!IsMapActorBlocked(map,point) && Vector2Distance(point,player)>230) return point;
    }
    Vector2 fallback=map->spawn;
    FindFreeMapPosition(map,fallback,&fallback);
    return fallback;
}
void CemeteryStartWave(EnemyGroup *group,const Clearing *map,int wave,Vector2 player,float power) {
    ClearEnemyAttacks(group);
    group->cemetery=true;group->playerHitCooldown=0;
    int total=6+(wave-1)*3;
    if (total>ENEMY_COUNT) total=ENEMY_COUNT;
    int specials=GetRandomValue(1,1+(wave-1)/2);
    for (int i=0;i<ENEMY_COUNT;i++) {
        Enemy *enemy=&group->list[i];
        *enemy=(Enemy){.state=ENEMY_GONE};
        if (i>=total) continue;
        enemy->position=CemeterySpawn(map,player);
        enemy->cemeteryUnit=true;
        enemy->type=i<specials?ENEMY_SPECIAL:i%3==0?ENEMY_COMMON_RANGED:ENEMY_COMMON_MELEE;
        enemy->maxHealth=(int)((enemy->type==ENEMY_SPECIAL?110:60)*(1+(wave-1)*.12f));
        enemy->health=enemy->maxHealth;
        enemy->power=1+(wave-1)*.06f;
        enemy->speed=92*(1+(wave-1)*.025f);
        enemy->state=ENEMY_SPAWNING;enemy->stateTime=-i*.35f;
        enemy->lastKatanaSwing=-1;enemy->attackCooldown=.8f;
    }
    (void)power;
}
void CemeteryStartBoss(EnemyGroup *group,const Clearing *map,Vector2 player,float power) {
    ClearEnemyAttacks(group);group->cemetery=true;group->playerHitCooldown=0;
    for (int i=0;i<ENEMY_COUNT;i++) group->list[i]=(Enemy){.state=ENEMY_GONE};
    group->list[0]=(Enemy){.position=CemeterySpawn(map,player),.cemeteryUnit=true,
        .type=ENEMY_BOSS,.state=ENEMY_SPAWNING,.maxHealth=(int)(480*power),.health=(int)(480*power),
        .power=power,.bossAttack=2,.attackCooldown=1,.lastKatanaSwing=-1};
}
void CemeteryResetEnemies(EnemyGroup *group,const Clearing *map) {
    CemeteryStartWave(group,map,1,map->spawn,1);
}

void CemeteryDamageEnemy(Enemy *enemy, int damage) {
    if (!EnemyCanBeHit(enemy) || damage<=0) return;
    enemy->health -= damage;
    if (enemy->type!=ENEMY_BOSS) enemy->stateTime = 0.0f;
    if (enemy->health <= 0) {
        enemy->stateTime = 0;
        enemy->health = 0;
        enemy->state = ENEMY_DIE;
        if (!enemy->dropRolled) {
            enemy->dropRolled = true;
            enemy->pickupAvailable = EnemyShouldDrop(enemy->type, GetRandomValue(0, 9999));
        }
    } else if (enemy->type != ENEMY_BOSS) {
        enemy->state = ENEMY_HURT;
    }
}

static void MoveEnemy(const Clearing *clearing, Enemy *enemy, Vector2 delta) {
    MoveMapActor(clearing, &enemy->position, delta);
}

static void HitPlayer(EnemyGroup *g, bool dodging, int *health, int damage) {
    if (dodging || g->playerHitCooldown>0 || *health<=0) return;
    *health -= damage;
    if (*health<0) *health=0;
    g->playerHitCooldown=.65f;
}

static void SpawnProjectile(EnemyGroup *g, Vector2 p, Vector2 direction, int kind, float power) {
    for (int i=0;i<ENEMY_PROJECTILES;i++) if (!g->cemeteryProjectiles[i].active) {
        g->cemeteryProjectiles[i]=(CemeteryProjectile){true,p,
            Vector2Scale(direction,kind==1?310.0f:210.0f),kind==1?2.0f:4.0f,kind,(int)((kind==1?22:10)*power)};
        break;
    }
}

void CemeteryUpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float dt, bool playerDodging,
                   int *playerHealth, int playerMaxHealth) {
    group->playerHitCooldown = fmaxf(0.0f, group->playerHitCooldown-dt);
    for (int i=0;i<ENEMY_PROJECTILES;i++) {
        CemeteryProjectile *p=&group->cemeteryProjectiles[i];
        if (!p->active) continue;
        p->life-=dt;
        /* Sweep in small steps so fast projectiles cannot cross narrow obstacles. */
        Vector2 delta=Vector2Scale(p->velocity,dt);
        int steps=(int)(Vector2Length(delta)/4)+1;
        for (int s=0;s<steps && p->active;s++) {
            p->position=Vector2Add(p->position,Vector2Scale(delta,1.0f/steps));
            if (IsClearingBlocked(clearing,p->position) || p->life<=0) p->active=false;
            else if (Vector2Distance(p->position,player)<(p->kind==1?29:19)) {
                HitPlayer(group,playerDodging,playerHealth,p->damage);
                p->active=false;
            }
        }
    }
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE && enemy->pickupAvailable &&
            *playerHealth > 0 && *playerHealth < playerMaxHealth &&
            Vector2Distance(enemy->position, player) < 26.0f) {
            *playerHealth += 15;
            if (*playerHealth > playerMaxHealth) *playerHealth = playerMaxHealth;
            enemy->pickupAvailable = false;
        }
        if (enemy->state == ENEMY_GONE || enemy->state==ENEMY_FALLEN) continue;
        enemy->stateTime += dt;
        if (enemy->state==ENEMY_SPAWNING) {
            if (enemy->stateTime>=1) {enemy->state=ENEMY_IDLE;enemy->stateTime=0;}
            continue;
        }
        enemy->attackCooldown = fmaxf(0.0f, enemy->attackCooldown-dt);
        enemy->bibleHitCooldown = fmaxf(0.0f, enemy->bibleHitCooldown-dt);
        if (enemy->state == ENEMY_DIE) {
            if (enemy->type==ENEMY_BOSS) ClearEnemyAttacks(group);
            if (enemy->stateTime >= 0.85f)
                enemy->state=enemy->type==ENEMY_BOSS || enemy->type==ENEMY_SPECIAL?ENEMY_FALLEN:ENEMY_GONE;
            continue;
        }
        if (enemy->state == ENEMY_HURT) {
            if (enemy->stateTime >= 0.32f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (enemy->state == ENEMY_ATTACK) {
            if (enemy->type==ENEMY_SPECIAL) {
                if (enemy->stateTime>=.5f && enemy->stateTime<1) {
                    MoveEnemy(clearing,enemy,Vector2Scale(enemy->attackDirection,360*dt));
                    if (!enemy->attackConnected && Vector2Distance(enemy->position,player)<65 &&
                        MapHasClearPath(clearing,enemy->position,player)) {
                        HitPlayer(group,playerDodging,playerHealth,(int)(18*enemy->power));
                        enemy->attackConnected=true;
                    }
                }
                if (enemy->stateTime>=1.1f) {enemy->state=ENEMY_IDLE;enemy->stateTime=0;}
                continue;
            }
            if (group->cemetery && enemy->type==ENEMY_BOSS) {
                float t=enemy->stateTime;
                if (!enemy->attackConnected && t>=.8f) {
                    enemy->attackConnected=true;
                    if (enemy->bossAttack==1)
                        SpawnProjectile(group,enemy->position,enemy->attackDirection,1,enemy->power);
                }
                if (enemy->bossAttack==0 && t>=.8f && t<1.2f &&
                    Vector2Distance(player,enemy->attackTarget)<95)
                    HitPlayer(group,playerDodging,playerHealth,(int)(24*enemy->power));
                if (enemy->bossAttack==2 && t>=.8f && t<1.35f) {
                    MoveEnemy(clearing,enemy,Vector2Scale(enemy->attackDirection,430*dt));
                    if (Vector2Distance(player,enemy->position)<58 &&
                        MapHasClearPath(clearing,enemy->position,player))
                        HitPlayer(group,playerDodging,playerHealth,(int)(20*enemy->power));
                }
                if (t>=2.0f) {enemy->state=ENEMY_IDLE;enemy->stateTime=0;}
                continue;
            }
            if (group->cemetery && enemy->type==ENEMY_COMMON_RANGED) {
                if (!enemy->attackConnected && enemy->stateTime>=.55f) {
                    enemy->attackConnected=true;
                    SpawnProjectile(group,enemy->position,enemy->attackDirection,0,enemy->power);
                }
                if (enemy->stateTime>=.95f) {enemy->state=ENEMY_IDLE;enemy->stateTime=0;}
                continue;
            }
            if (!enemy->attackConnected && enemy->stateTime >= 0.32f) {
                enemy->attackConnected = true;
                if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0 &&
                    Vector2Distance(enemy->position, player) <= ENEMY_ATTACK_RANGE+8.0f &&
                    MapHasClearPath(clearing,enemy->position,player)) {
                    *playerHealth -= (int)(ENEMY_ATTACK_DAMAGE*enemy->power);
                    if (*playerHealth < 0) *playerHealth = 0;
                    group->playerHitCooldown = 0.65f;
                }
            }
            if (enemy->stateTime >= 0.62f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (*playerHealth <= 0) { enemy->state = ENEMY_IDLE; continue; }
        Vector2 toPlayer = Vector2Subtract(player, enemy->position);
        float distance = Vector2Length(toPlayer);
        if (enemy->type==ENEMY_SPECIAL && distance<=280 && enemy->attackCooldown<=0) {
            enemy->state=ENEMY_ATTACK;enemy->stateTime=0;enemy->attackConnected=false;
            enemy->attackDirection=Vector2Normalize(toPlayer);enemy->attackCooldown=3;
            continue;
        }
        if (group->cemetery && enemy->type==ENEMY_BOSS) {
            if (enemy->attackCooldown<=0) {
                enemy->bossAttack=(enemy->bossAttack+1)%3;
                enemy->state=ENEMY_ATTACK;enemy->stateTime=0;enemy->attackConnected=false;
                enemy->attackTarget=player;
                enemy->attackDirection=Vector2Normalize(toPlayer);
                enemy->facing=fabsf(toPlayer.x)>fabsf(toPlayer.y)?(toPlayer.x<0?2:3):(toPlayer.y<0?1:0);
                enemy->attackCooldown=3.2f;
            }
            continue;
        }
        if (group->cemetery && enemy->type==ENEMY_COMMON_RANGED) {
            enemy->facing=fabsf(toPlayer.x)>fabsf(toPlayer.y)?(toPlayer.x<0?2:3):(toPlayer.y<0?1:0);
            if (distance<480 && enemy->attackCooldown<=0) {
                enemy->state=ENEMY_ATTACK;enemy->stateTime=0;enemy->attackConnected=false;
                enemy->attackDirection=Vector2Normalize(toPlayer);
                enemy->attackCooldown=2.1f;
            } else if (distance>340 || distance<170) {
                enemy->state=ENEMY_WALK;enemy->walkTime+=dt;
                MoveEnemy(clearing,enemy,Vector2Scale(Vector2Normalize(toPlayer),
                    (distance<170?-1:1)*enemy->speed*(70.0f/92.0f)*dt));
            } else enemy->state=ENEMY_IDLE;
            continue;
        }
        if (distance > 0.001f) {
            enemy->facing = fabsf(toPlayer.x) > fabsf(toPlayer.y)*0.65f
                ? (toPlayer.x < 0 ? 2 : 3) : (toPlayer.y < 0 ? 1 : 0);
        }
        if (distance <= ENEMY_ATTACK_RANGE && enemy->attackCooldown <= 0.0f) {
            enemy->state = ENEMY_ATTACK;
            enemy->stateTime = 0.0f;
            enemy->attackConnected = false;
            enemy->attackCooldown = 1.35f;
        } else if (distance > ENEMY_ATTACK_RANGE*0.85f) {
            enemy->state = ENEMY_WALK;
            enemy->walkTime += dt;
            MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), enemy->speed*dt));
        } else {
            enemy->state = ENEMY_IDLE;
        }
    }
}

void CemeteryDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE && enemy->pickupAvailable) {
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 9.0f,
                        Fade(GOLD, 0.65f));
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 6.0f, RED);
        }
        if (enemy->state == ENEMY_GONE) continue;
        if (group->cemetery) {
            Texture2D tex=enemy->type==ENEMY_BOSS?group->boss[enemy->bossAttack]:
                enemy->type==ENEMY_COMMON_RANGED?group->mage:group->skeleton;
            int cols=enemy->type==ENEMY_BOSS?9:enemy->type==ENEMY_COMMON_RANGED?4:5;
            int rows=enemy->type==ENEMY_BOSS?4:enemy->type==ENEMY_COMMON_RANGED?4:5;
            int frame=enemy->state==ENEMY_ATTACK?(int)(enemy->stateTime/2.0f*cols):
                (int)(enemy->walkTime*7)%cols;
            if (frame>=cols) frame=cols-1;
            if (enemy->state==ENEMY_IDLE) frame=0;
            int row=enemy->type==ENEMY_BOSS?enemy->facing:
                enemy->state==ENEMY_DIE?rows-1:enemy->state==ENEMY_ATTACK?2:
                enemy->state==ENEMY_WALK?1:0;
            float cw=(float)tex.width/cols,ch=(float)tex.height/rows;
            float size=enemy->type==ENEMY_BOSS?150:80;
            Color tint=enemy->state==ENEMY_HURT?RED:enemy->state==ENEMY_DIE?
                Fade(WHITE,1-enemy->stateTime/.85f):enemy->type==ENEMY_SPECIAL?(Color){220,170,255,255}:WHITE;
            DrawTexturePro(tex,(Rectangle){frame*cw,row*ch,cw,ch},
                (Rectangle){enemy->position.x-size/2,enemy->position.y-size,size,size},
                (Vector2){0},0,tint);
            DrawRectangle((int)enemy->position.x-30,(int)(enemy->position.y-size)-8,60,5,DARKGRAY);
            DrawRectangle((int)enemy->position.x-30,(int)(enemy->position.y-size)-8,
                60*enemy->health/enemy->maxHealth,5,RED);
            if (enemy->type==ENEMY_BOSS && enemy->state==ENEMY_ATTACK) {
                Color warning=enemy->stateTime<.8f?Fade(VIOLET,.45f):VIOLET;
                if (enemy->bossAttack==0 && enemy->stateTime<1.2f)
                    DrawCircleLines((int)enemy->attackTarget.x,(int)enemy->attackTarget.y,95,warning);
                if (enemy->bossAttack==2 && enemy->stateTime<.8f)
                    DrawLineEx(enemy->position,Vector2Add(enemy->position,
                        Vector2Scale(enemy->attackDirection,240)),5,warning);
            }
            if (enemy->state==ENEMY_FALLEN) {
                DrawCircleLines((int)enemy->position.x,(int)enemy->position.y,26,VIOLET);
                DrawText("E: selar",(int)enemy->position.x-28,(int)enemy->position.y+8,14,GOLD);
            }
            continue;
        }
        int row = enemy->state == ENEMY_WALK ? 1 : enemy->state == ENEMY_ATTACK ? 2 :
                  enemy->state == ENEMY_HURT ? 3 : enemy->state == ENEMY_DIE ? 4 : 0;
        int frame = EnemyAnimationFrame(enemy);
        Texture2D tex = group->sprites;
        if (enemy->facing < 2 && enemy->state != ENEMY_DIE) {
            tex = enemy->facing == 1 ? group->backSprites : group->frontSprites;
            if (enemy->state == ENEMY_ATTACK) {
                row = 1; /* Sem arte de ataque frontal/traseiro: movimento seguro. */
                frame = (int)(enemy->stateTime*6.5f);
                if (frame > 3) frame = 3;
            } else if (enemy->state == ENEMY_HURT) {
                row = 0;
                frame = 0;
            }
        }
        Rectangle src = {frame*80.0f, row*80.0f,
                         enemy->facing==2 && tex.id==group->sprites.id ? -80.0f : 80.0f, 80.0f};
        Color tint = enemy->state == ENEMY_HURT ? (Color){255,160,160,255} : WHITE;
        DrawTexturePro(tex, src,
            (Rectangle){enemy->position.x-38, enemy->position.y-75, 76, 76},
            (Vector2){0},0,tint);
        if (EnemyCanBeHit(enemy) && enemy->health < ENEMY_MAX_HEALTH) {
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,48,5,DARKGRAY);
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,
                          48*enemy->health/ENEMY_MAX_HEALTH,5,RED);
        }
    }
}

static void DrawEffect(Texture2D tex, Vector2 position, int frame, float size) {
    float w=(float)tex.width/4,h=(float)tex.height/2;
    DrawTexturePro(tex,(Rectangle){(frame%4)*w,(frame/4)*h,w,h},
        (Rectangle){position.x-size/2,position.y-size*.75f,size,size*.75f},
        (Vector2){0},0,WHITE);
}

void DrawEnemyProjectiles(const EnemyGroup *group) {
    if (group->cemetery) {
        const Enemy *boss=&group->list[0];
        if (boss->state==ENEMY_ATTACK && boss->stateTime>=.8f) {
            int frame=(int)((boss->stateTime-.8f)/1.2f*8);
            if (frame>7) frame=7;
            if (boss->bossAttack==0)
                DrawEffect(group->effects[0],boss->attackTarget,frame,190);
            if (boss->bossAttack==2)
                DrawEffect(group->effects[2],boss->position,frame,140);
        }
    }
    for (int i=0;i<ENEMY_PROJECTILES;i++) {
        const CemeteryProjectile *p=&group->cemeteryProjectiles[i];
        if (!p->active) continue;
        if (p->kind==1) {
            DrawEffect(group->effects[1],p->position,(int)(GetTime()*16)%8,84);
        } else {
            DrawCircleV(p->position,14,Fade(GREEN,.3f));
            DrawCircleV(p->position,8,LIME);
            DrawCircleV(p->position,3,RAYWHITE);
        }
    }
}
