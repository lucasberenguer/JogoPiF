#include "enemy.h"
#include "raymath.h"
#include <math.h>

#define COVEIRO_WINDUP .8f
#define COVEIRO_ROOT_DAMAGE 44
#define COVEIRO_SHOVEL_DAMAGE 38
#define COVEIRO_CHARGE_DAMAGE 42
#define COVEIRO_MOVEMENT_SCALE .92f
#define COVEIRO_CHARGE_SPEED (560.0f * COVEIRO_MOVEMENT_SCALE)
#define COVEIRO_ENRAGED_CHARGE_SPEED (680.0f * COVEIRO_MOVEMENT_SCALE)
#define COVEIRO_SHOVEL_RADIUS 43.0f

static bool HitPlayer(EnemyGroup *group, bool dodging, int *health, int damage) {
    if (dodging || group->playerHitCooldown > 0 || *health <= 0) return false;
    *health = (int)fmaxf(0, *health - damage);
    group->playerHitCooldown = .65f;
    return true;
}

static void ThrowShovel(EnemyGroup *group, const Enemy *boss, Vector2 direction) {
    for (int i = 0; i < CEMETERY_PROJECTILE_COUNT; i++) {
        CemeteryProjectile *shot = &group->cemeteryProjectiles[i];
        if (shot->active) continue;
        float speed = boss->guardianPhase == GUARDIAN_PHASE_TWO ? 740 : 620;
        *shot = (CemeteryProjectile){.active=true, .position=boss->position,
            .velocity=Vector2Scale(direction, speed), .life=2,
            .damage=(int)(COVEIRO_SHOVEL_DAMAGE * boss->power)};
        return;
    }
}

void CemeteryUpdateProjectiles(EnemyGroup *group, const Clearing *map, Vector2 player,
                               float dt, bool dodging, int *health) {
    for (int i = 0; i < CEMETERY_PROJECTILE_COUNT; i++) {
        CemeteryProjectile *shot = &group->cemeteryProjectiles[i];
        if (!shot->active) continue;
        shot->life -= dt;
        const Enemy *boss=&group->list[0];
        if (boss->type!=ENEMY_BOSS || boss->health<=0 || shot->life<=0) {shot->active=false;continue;}
        if (!shot->returning && shot->life<1.15f) {shot->returning=true;shot->hitPlayer=false;}
        if (shot->returning) {
            Vector2 back=Vector2Subtract(boss->position,shot->position);
            if (Vector2Length(back)<26) {shot->active=false;continue;}
            shot->velocity=Vector2Scale(Vector2Normalize(back),boss->guardianPhase==GUARDIAN_PHASE_TWO?760:660);
        }
        Vector2 delta = Vector2Scale(shot->velocity, dt);
        int steps = (int)(Vector2Length(delta)/4)+1;
        for (int s = 0; s < steps && shot->active; s++) {
            Vector2 next=Vector2Add(shot->position,Vector2Scale(delta,1.0f/steps));
            if (IsClearingBlocked(map,next)) {
                if (shot->returning) shot->active=false;
                else {shot->returning=true;shot->hitPlayer=false;}
                break;
            }
            shot->position=next;
            if (!shot->hitPlayer && Vector2Distance(next,player)<COVEIRO_SHOVEL_RADIUS &&
                HitPlayer(group,dodging,health,shot->damage)) shot->hitPlayer=true;
        }
    }
}

void CemeteryUpdateBoss(EnemyGroup *group, const Clearing *map, Enemy *boss, Vector2 player,
                        float dt, bool dodging, int *health) {
    bool phaseTwo = boss->guardianPhase == GUARDIAN_PHASE_TWO;
    if (boss->state == ENEMY_ATTACK) {
        float t = boss->stateTime;
        if (t >= COVEIRO_WINDUP && !boss->attackConnected) {
            if (boss->bossAttack == COVEIRO_SHOVEL) {
                boss->attackConnected = true;
                ThrowShovel(group, boss, boss->attackDirection);
                if (phaseTwo) {
                    ThrowShovel(group, boss, Vector2Rotate(boss->attackDirection, .25f));
                    ThrowShovel(group, boss, Vector2Rotate(boss->attackDirection, -.25f));
                }
            } else if (boss->bossAttack == COVEIRO_ROOTS && t < 1.2f &&
                       Vector2Distance(player, boss->attackTarget) < (phaseTwo ? 115 : 95)) {
                boss->attackConnected = HitPlayer(group, dodging, health, (int)(COVEIRO_ROOT_DAMAGE * boss->power));
            }
        }
        if (boss->bossAttack==COVEIRO_ROOTS && phaseTwo && t>=COVEIRO_WINDUP && boss->attackBurst==0) {
            boss->attackBurst=1;
            Vector2 side=Vector2Scale(Vector2Rotate(boss->attackDirection,PI/2),120);
            for(int n=-1;n<=1;n+=2) {
                Vector2 target=Vector2Add(boss->attackTarget,Vector2Scale(side,n));
                if(IsClearingBlocked(map,target)) continue;
                for(int i=0;i<ENEMY_ROOT_COUNT;i++) if(!group->rootAttacks[i].active) {
                    group->rootAttacks[i]=(EnemyRoot){.position=target,.active=true,.attack=GUARDIAN_ROOTS};break;
                }
            }
        }
        if(boss->bossAttack==COVEIRO_CHARGE && phaseTwo && t>=1.12f && boss->attackBurst==0) {
            boss->attackBurst=1;boss->attackConnected=false;
            Vector2 offset=Vector2Subtract(player,boss->position);
            if(Vector2LengthSqr(offset)>.01f) boss->attackDirection=Vector2Normalize(offset);
            boss->facing=fabsf(offset.x)>fabsf(offset.y)?(offset.x<0?2:3):(offset.y<0?1:0);
        }
        bool chargeWindow=phaseTwo?((t>=.8f && t<1.12f)||(t>=1.45f && t<1.77f)):(t>=.8f && t<1.35f);
        if (boss->bossAttack == COVEIRO_CHARGE && chargeWindow) {
            float speed = phaseTwo ? COVEIRO_ENRAGED_CHARGE_SPEED : COVEIRO_CHARGE_SPEED;
            MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, speed * dt));
            if (!boss->attackConnected && Vector2Distance(player, boss->position) < 58 &&
                MapHasClearPath(map, boss->position, player))
                boss->attackConnected = HitPlayer(group, dodging, health, (int)(COVEIRO_CHARGE_DAMAGE * boss->power));
        }
        /* Follow-through has real movement, while the initial targeting stays locked. */
        if(boss->bossAttack!=COVEIRO_CHARGE && t>=.8f && t<1.02f)
            MoveEnemy(map,boss,Vector2Scale(boss->attackDirection,110*COVEIRO_MOVEMENT_SCALE*dt));
        if (t >= (phaseTwo && boss->bossAttack==COVEIRO_CHARGE?2.0f:1.65f)) { boss->state = ENEMY_IDLE; boss->stateTime = 0; }
        return;
    }
    if (*health <= 0) return;
    Vector2 offset = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(offset);
    boss->facing = fabsf(offset.x) > fabsf(offset.y) ? (offset.x < 0 ? 2 : 3) : (offset.y < 0 ? 1 : 0);
    if (distance <= 560 && boss->attackCooldown <= 0) {
        boss->bossAttack = (boss->bossAttack + 1) % COVEIRO_ATTACK_COUNT;
        boss->state = ENEMY_ATTACK;
        boss->stateTime = 0;
        boss->attackConnected = false;
        boss->attackBurst=0;
        boss->attackTarget = player;
        boss->attackDirection = distance > .01f ? Vector2Normalize(offset) : (Vector2){0, 1};
        boss->attackCooldown = phaseTwo ? 1.75f : 2.65f;
    } else if (distance > 150) {
        boss->state = ENEMY_WALK;
        boss->walkTime+=dt;
        MoveEnemy(map, boss, Vector2Scale(Vector2Normalize(offset), boss->speed * COVEIRO_MOVEMENT_SCALE * (phaseTwo ? 1.5f : 1) * dt));
    } else boss->state = ENEMY_IDLE;
}

CemeteryPose CemeteryAnimationPose(const Enemy *enemy) {
    CemeteryPose p={CEM_SKELETON_SIDE,5,5,0,0,false};
    bool dead=enemy->state==ENEMY_DIE || enemy->state==ENEMY_FALLEN;
    if(enemy->type==ENEMY_BOSS) {
        p=(CemeteryPose){CEM_BOSS_WALK,12,4,enemy->facing,0,false};
        /* Lateral assets are authored facing right; mirror them explicitly. */
        if(enemy->facing==2) {p.row=3;p.flip=true;}
        if(enemy->state==ENEMY_WALK) p.frame=(int)(enemy->walkTime*18)%12;
        else if(enemy->state==ENEMY_IDLE) p.frame=(int)(enemy->stateTime*5)%2;
        if(enemy->state==ENEMY_ATTACK) {
            p.atlas=CEM_BOSS_SLAM+enemy->bossAttack;
            p.columns=6;
            float t=enemy->stateTime;
            if(enemy->bossAttack==COVEIRO_SHOVEL) {
                p.columns=12;
                p.frame=t<.8f?(int)(t/.8f*6):6+(int)fminf((t-.8f)/.85f*6,5);
            }
            else if(t<.8f) p.frame=(int)(t/.8f*3);
            else if(enemy->bossAttack==COVEIRO_CHARGE) {
                if(enemy->guardianPhase==GUARDIAN_PHASE_TWO && t>=1.12f && t<1.45f) p.frame=1;
                else if(t<(enemy->guardianPhase==GUARDIAN_PHASE_TWO?1.77f:1.35f)) p.frame=2+(int)(t*12)%2;
                else p.frame=t<1.9f?4:5;
            } else p.frame=t<1.02f?3:t<1.5f?4:5;
        }
        return p;
    }
    if(enemy->type==ENEMY_COMMON_RANGED) {
        const int directions[]={0,1,3,2};
        p=(CemeteryPose){CEM_MAGE,4,4,directions[enemy->facing],0,false};
        if(enemy->state==ENEMY_WALK) p.frame=(int)(enemy->walkTime*9)%2;
        if(enemy->state==ENEMY_ATTACK) p.frame=enemy->stateTime<.25f?0:enemy->stateTime<.55f?1:enemy->stateTime<.75f?2:3;
        return p;
    }
    if(!dead && enemy->facing<2) {
        p=(CemeteryPose){enemy->facing==1?CEM_SKELETON_BACK:CEM_SKELETON_FRONT,4,2,
            enemy->state==ENEMY_WALK || enemy->state==ENEMY_ATTACK?1:0,0,false};
        p.frame=(int)((enemy->state==ENEMY_WALK?enemy->walkTime:enemy->stateTime)*10)%4;
    } else {
        p.flip=enemy->facing==2;
        p.row=dead?4:enemy->state==ENEMY_ATTACK?2:enemy->state==ENEMY_HURT?3:enemy->state==ENEMY_WALK?1:0;
        p.frame=dead?(enemy->state==ENEMY_FALLEN?4:(int)fminf(enemy->stateTime*6,4)):
            enemy->state==ENEMY_ATTACK?(int)fminf(enemy->stateTime*9,4):(int)(enemy->walkTime*10)%5;
    }
    return p;
}

void CemeteryDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE) {
            if (enemy->pickupAvailable) {
                Vector2 drop = {enemy->position.x, enemy->position.y - 9};
                DrawCircleV(drop, 9, Fade(GOLD, .65f));
                DrawCircleV(drop, 6, RED);
            }
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            if (enemy->stateTime >= 0)
                DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                24 + 10 * sinf(enemy->stateTime*8), GOLD);
            continue;
        }

        bool boss = enemy->type == ENEMY_BOSS;
        bool ranged = enemy->type == ENEMY_COMMON_RANGED;
        bool fallen = enemy->state == ENEMY_FALLEN;
        CemeteryPose pose=CemeteryAnimationPose(enemy);
        const Texture2D atlases[]={group->skeleton,group->skeletonFront,group->skeletonBack,
            group->mage,group->coveiroWalk,group->boss[0],group->boss[1],group->boss[2]};
        Texture2D tex=atlases[pose.atlas];
        int frame=pose.frame,row=pose.row;
        float cw=(float)tex.width/pose.columns,ch=(float)tex.height/pose.rows;
        float height=boss?190:80;
        float width=height*cw/ch;
        float bob=ranged && enemy->state==ENEMY_WALK?sinf(enemy->walkTime*12)*1.5f:0;
        float lean=0;
        if(boss && !fallen && enemy->state!=ENEMY_DIE) {
            if(enemy->state==ENEMY_WALK) {bob=-fabsf(sinf(enemy->walkTime*14))*3;lean=sinf(enemy->walkTime*14)*1.5f;}
            else if(enemy->state==ENEMY_ATTACK) {
                float t=enemy->stateTime;
                float motion=t<.8f?-sinf(t/.8f*PI/2):sinf(fminf((t-.8f)/.85f,1)*PI);
                lean=motion*(pose.flip?-5:5);
                bob=-fabsf(sinf(t*9))*2;
            } else bob=sinf(enemy->stateTime*5)*1.2f;
        }
        Color tint = enemy->state == ENEMY_HURT ? RED : enemy->type == ENEMY_SPECIAL ? VIOLET : WHITE;
        if (boss && (fallen || enemy->state == ENEMY_DIE)) tint = Fade(tint, .45f);
        if (boss && enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
            float radius = 50 + 120 * enemy->stateTime;
            DrawCircleV(enemy->position, radius, Fade(VIOLET, .30f * (1 - enemy->stateTime)));
            DrawCircleLines((int)enemy->position.x, (int)enemy->position.y, radius, VIOLET);
        } else if (boss && enemy->guardianPhase == GUARDIAN_PHASE_TWO && enemy->health > 0) {
            DrawCircleV(enemy->position, 43, Fade(VIOLET, .26f));
        }
        DrawTexturePro(tex, (Rectangle){frame*cw, row*ch, pose.flip?-cw:cw, ch},
            (Rectangle){enemy->position.x, enemy->position.y+bob, width, height},
            (Vector2){width/2,height*.94f}, lean, tint);

        if (fallen) DrawText("E: selar", (int)enemy->position.x-30, (int)enemy->position.y+10, 16, GOLD);
        if (enemy->type == ENEMY_SPECIAL && enemy->state == ENEMY_ATTACK && enemy->stateTime < .5f)
            DrawCircleLines((int)enemy->position.x, (int)enemy->position.y, 55, ORANGE);
        if (!boss && EnemyCanBeHit(enemy) && enemy->health < enemy->maxHealth) {
            int x = (int)enemy->position.x-24, y = (int)(enemy->position.y-height)-8;
            DrawRectangle(x, y, 48, 5, DARKGRAY);
            DrawRectangle(x, y, 48*enemy->health/enemy->maxHealth, 5, RED);
        }
        if (boss && enemy->state == ENEMY_ATTACK) {
            Color warning = enemy->stateTime < COVEIRO_WINDUP ? Fade(VIOLET, .45f) : VIOLET;
            bool phaseTwo = enemy->guardianPhase == GUARDIAN_PHASE_TWO;
            if(enemy->bossAttack==COVEIRO_SHOVEL && enemy->stateTime<COVEIRO_WINDUP) {
                for(int n=phaseTwo?-1:0;n<=(phaseTwo?1:0);n++) {
                    Vector2 direction=Vector2Rotate(enemy->attackDirection,n*.25f);
                    DrawLineEx(enemy->position,Vector2Add(enemy->position,Vector2Scale(direction,180)),3,warning);
                }
            }
            if (enemy->bossAttack == COVEIRO_ROOTS && enemy->stateTime < 1.2f)
                DrawCircleLines((int)enemy->attackTarget.x, (int)enemy->attackTarget.y, phaseTwo ? 115 : 95, warning);
            if (enemy->bossAttack == COVEIRO_CHARGE && (enemy->stateTime < COVEIRO_WINDUP ||
                (phaseTwo && enemy->stateTime>=1.12f && enemy->stateTime<1.45f)))
                DrawLineEx(enemy->position, Vector2Add(enemy->position,
                    Vector2Scale(enemy->attackDirection,
                                 (phaseTwo ? COVEIRO_ENRAGED_CHARGE_SPEED : COVEIRO_CHARGE_SPEED) * .55f)), 5, warning);
        }
    }
}

static void DrawEffect(Texture2D tex, Vector2 position, int frame, float size) {
    float w = (float)tex.width/4, h = (float)tex.height/2;
    DrawTexturePro(tex, (Rectangle){(frame%4)*w,(frame/4)*h,w,h},
        (Rectangle){position.x-size/2,position.y-size*.75f,size,size*.75f},
        (Vector2){0}, 0, WHITE);
}

void DrawEnemyProjectiles(const EnemyGroup *group) {
    if (!group->cemetery) return;
    const Enemy *boss = &group->list[0];
    for(int i=0;i<ENEMY_ROOT_COUNT;i++) {
        const EnemyRoot *root=&group->rootAttacks[i];
        if(!root->active) continue;
        if(root->age<.8f) DrawCircleLines((int)root->position.x,(int)root->position.y,62,GREEN);
        else DrawEffect(group->effects[0],root->position,(int)fminf((root->age-.8f)*10,7),124);
    }
    if (boss->type == ENEMY_BOSS && boss->state == ENEMY_ATTACK && boss->stateTime >= COVEIRO_WINDUP) {
        int frame = (int)fminf((boss->stateTime-COVEIRO_WINDUP)/1.2f*8, 7);
        if (boss->bossAttack == COVEIRO_ROOTS)
            DrawEffect(group->effects[0], boss->attackTarget, frame, boss->guardianPhase == GUARDIAN_PHASE_TWO ? 230 : 190);
        if (boss->bossAttack == COVEIRO_CHARGE)
            DrawEffect(group->effects[2], boss->position, frame, 140);
    }
    for (int i = 0; i < CEMETERY_PROJECTILE_COUNT; i++) {
        const CemeteryProjectile *shot = &group->cemeteryProjectiles[i];
        if (shot->active) {
            Texture2D tex=group->effects[1];
            float cw=tex.width/8.0f,ch=tex.height/2.0f;
            float age=2-shot->life;
            int frame=(int)(age*24)%8;
            float angle=atan2f(shot->velocity.y,shot->velocity.x)*RAD2DEG;
            DrawEllipse((int)shot->position.x,(int)shot->position.y+5,36,10,Fade(BLACK,.3f));
            DrawTexturePro(tex,(Rectangle){frame*cw,0,cw,ch},
                (Rectangle){shot->position.x,shot->position.y-18,142,142},(Vector2){71,71},angle,WHITE);
            Vector2 dust=Vector2Subtract(shot->position,Vector2Scale(Vector2Normalize(shot->velocity),45));
            DrawTexturePro(tex,(Rectangle){frame*cw,ch,cw,ch},
                (Rectangle){dust.x,dust.y,110,75},(Vector2){55,37.5f},angle,Fade(WHITE,.65f));
        }
    }
}
