#include "game_text.h"
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
    CemeteryPose p;
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
    const int rows[]={0,2,3,1}; // down, up, left, right
    if (enemy->type==ENEMY_COMMON_RANGED) {
        p=(CemeteryPose){CEM_LANTERN_IDLE,4,4,rows[enemy->facing],0,false};
        if (enemy->state==ENEMY_WALK) {
            p.atlas=CEM_LANTERN_WALK;
            p.columns=6;
            p.frame=(int)(enemy->walkTime*9)%6;
        } else if (enemy->state==ENEMY_ATTACK) {
            p.atlas=CEM_LANTERN_ATTACK;
            p.columns=6;
            float t=enemy->stateTime;
            if (t<CEM_LANTERN_RELEASE_TIME) p.frame=(int)fminf(3, t*4/CEM_LANTERN_RELEASE_TIME);
            else p.frame=t<.73f?4:5;
        } else if (enemy->state==ENEMY_IDLE) p.frame=(int)(enemy->stateTime*5)%4;
        return p;
    }
    p=(CemeteryPose){CEM_ZOMBIE_IDLE,4,4,rows[enemy->facing],0,false};
    if (enemy->state==ENEMY_WALK) {
        p.atlas=CEM_ZOMBIE_WALK;
        p.frame=(int)(enemy->walkTime*9)%4;
    } else if (enemy->state==ENEMY_ATTACK) {
        p.atlas=CEM_ZOMBIE_ATTACK;
        p.columns=6;
        p.frame=(int)fminf(5,enemy->stateTime*4/CEM_ZOMBIE_HIT_TIME);
    } else if (enemy->state==ENEMY_IDLE) p.frame=(int)(enemy->stateTime*5)%4;
    return p;
}

void CemeteryDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE) {
            if (enemy->pickupAvailable) DrawHealthPickup(enemy->position);
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            if (enemy->stateTime >= 0)
                DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                24 + 10 * sinf(enemy->stateTime*8), GOLD);
            continue;
        }

        bool boss = enemy->type == ENEMY_BOSS;
        bool fallen = enemy->state == ENEMY_FALLEN;
        float collapse = fallen ? 1.0f :
            (boss && enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / .85f, 0.0f, 1.0f) : 0.0f);
        CemeteryPose pose=CemeteryAnimationPose(enemy);
        const Texture2D atlases[]={group->coveiroWalk,group->boss[0],group->boss[1],group->boss[2],
            group->cemeteryZombie[0],group->cemeteryZombie[1],group->cemeteryZombie[2],
            group->cemeteryLantern[0],group->cemeteryLantern[1],group->cemeteryLantern[2]};
        Texture2D tex=atlases[pose.atlas];
        int frame=pose.frame,row=pose.row;
        float cw=(float)tex.width/pose.columns,ch=(float)tex.height/pose.rows;
        float height=(boss?190:96)*(1.0f-.22f*collapse);
        float width=(boss?190:96)*cw/ch;
        float bob=0;
        float lean=0;
        if(boss && !fallen && enemy->state!=ENEMY_DIE) {
            if(enemy->state==ENEMY_WALK) {bob=-fabsf(sinf(enemy->walkTime*14))*3;lean=sinf(enemy->walkTime*14)*1.5f;}
            else if(enemy->state==ENEMY_ATTACK) {
                float t=enemy->stateTime;
                float motion=t<.8f?-sinf(t/.8f*PI/2):sinf(fminf((t-.8f)/.85f,1)*PI);
                lean=motion*(pose.flip?-5:5);
                bob=-fabsf(sinf(t*9))*2;
            } else bob=sinf(enemy->stateTime*5)*1.2f;
        } else if (boss && collapse > 0.0f) {
            lean = (enemy->facing == 2 ? -1.0f : 1.0f) * 82.0f * collapse;
        }
        Color tint = enemy->state == ENEMY_HURT ? RED : WHITE;
        if (!boss && enemy->state==ENEMY_DIE) tint=Fade(tint,fmaxf(0,1-enemy->stateTime/.45f));
        if (boss && (fallen || enemy->state == ENEMY_DIE)) tint = Fade((Color){215, 190, 225, 255}, .78f);
        if (boss && enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
            float radius = 50 + 120 * enemy->stateTime;
            DrawCircleV(enemy->position, radius, Fade(VIOLET, .30f * (1 - enemy->stateTime)));
            DrawCircleLines((int)enemy->position.x, (int)enemy->position.y, radius, VIOLET);
        } else if (boss && enemy->guardianPhase == GUARDIAN_PHASE_TWO && enemy->health > 0) {
            DrawCircleV(enemy->position, 43, Fade(VIOLET, .26f));
        }
        if (boss && fallen) DrawBossGroundSeal(group->sealMarker, enemy->position);
        if (boss && collapse > 0.0f)
            DrawEllipse((int)enemy->position.x, (int)enemy->position.y - 8, 52.0f, 18.0f, Fade(BLACK, .38f));
        DrawTexturePro(tex, (Rectangle){frame*cw, row*ch, pose.flip?-cw:cw, ch},
            (Rectangle){enemy->position.x, enemy->position.y+bob, width, height},
            (Vector2){width/2,height*(boss?.94f:.914f)}, lean, tint);

        if (fallen) DrawGameText("E: selar", (int)enemy->position.x-30, (int)enemy->position.y+10, 16, GOLD);
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
