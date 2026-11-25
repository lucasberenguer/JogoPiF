#include "game_text.h"
#include "enemy.h"
#include "raymath.h"
#include <math.h>

#define COVEIRO_WINDUP .62f
#define COVEIRO_ROOT_END 1.02f
#define COVEIRO_CHARGE_FIRST_END .94f
#define COVEIRO_CHARGE_SECOND_START 1.27f
#define COVEIRO_CHARGE_SECOND_END 1.59f
#define COVEIRO_CHARGE_FIRST_PHASE_END 1.17f
#define COVEIRO_ATTACK_END 1.47f
#define COVEIRO_DOUBLE_CHARGE_END 1.82f
#define COVEIRO_ROOT_DAMAGE (GUARDIAN_ROOT_DAMAGE * 1.15f * COVEIRO_DAMAGE_BOOST)
#define COVEIRO_SHOVEL_DAMAGE (GUARDIAN_WAVE_DAMAGE * 1.15f * COVEIRO_DAMAGE_BOOST)
#define COVEIRO_CHARGE_DAMAGE (GUARDIAN_CURSE_DAMAGE * 1.15f * COVEIRO_DAMAGE_BOOST)
#define COVEIRO_MELEE_RANGE 122.0f
#define COVEIRO_MELEE_DAMAGE (GUARDIAN_MELEE_DAMAGE * 1.15f * COVEIRO_DAMAGE_BOOST)
#define COVEIRO_MELEE_COOLDOWN 1.65f
#define COVEIRO_MELEE_WINDUP .30f
#define COVEIRO_MELEE_DURATION .72f
#define COVEIRO_MOVEMENT_SCALE .92f
#define COVEIRO_CHARGE_SPEED (560.0f * COVEIRO_MOVEMENT_SCALE)
#define COVEIRO_ENRAGED_CHARGE_SPEED (680.0f * COVEIRO_MOVEMENT_SCALE)
#define COVEIRO_SHOVEL_RADIUS 43.0f
#define CEM_SPECIAL_RADIUS 76.0f
#define CEM_SPECIAL_DASH_START .27f
#define CEM_SPECIAL_DASH_SPEED 360.0f
#define CEM_SPECIAL_ATTACK_RANGE 150.0f
#define CEM_SPECIAL_STOP_DISTANCE 32.0f
#define CEM_SPECIAL_ATTACK_DURATION 1.04f

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
            .damage=(int)roundf(COVEIRO_SHOVEL_DAMAGE * boss->power)};
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
        if (boss->guardianMelee) {
            if (t >= COVEIRO_MELEE_WINDUP && !boss->attackConnected) {
                boss->attackConnected = true;
                Vector2 offset = Vector2Subtract(player, boss->position);
                float distance = Vector2Length(offset);
                if (distance <= COVEIRO_MELEE_RANGE &&
                    Vector2DotProduct(offset, boss->attackDirection) >= distance * .42f &&
                    MapHasClearPath(map, boss->position, player)) {
                    HitPlayer(group, dodging, health, (int)roundf(COVEIRO_MELEE_DAMAGE * boss->power));
                }
            }
            if (t >= COVEIRO_MELEE_WINDUP && t < COVEIRO_MELEE_WINDUP + .16f)
                MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, 95 * COVEIRO_MOVEMENT_SCALE * dt));
            if (t >= COVEIRO_MELEE_DURATION) {
                boss->state = ENEMY_IDLE;
                boss->stateTime = 0;
                boss->guardianMelee = false;
            }
            return;
        }
        if (t >= COVEIRO_WINDUP && !boss->attackConnected) {
            if (boss->bossAttack == COVEIRO_SHOVEL) {
                boss->attackConnected = true;
                ThrowShovel(group, boss, boss->attackDirection);
                if (phaseTwo) {
                    ThrowShovel(group, boss, Vector2Rotate(boss->attackDirection, .25f));
                    ThrowShovel(group, boss, Vector2Rotate(boss->attackDirection, -.25f));
                }
            } else if (boss->bossAttack == COVEIRO_ROOTS && t < COVEIRO_ROOT_END &&
                       Vector2Distance(player, boss->attackTarget) < (phaseTwo ? 115 : 95)) {
                boss->attackConnected = HitPlayer(group, dodging, health, (int)roundf(COVEIRO_ROOT_DAMAGE * boss->power));
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
        if(boss->bossAttack==COVEIRO_CHARGE && phaseTwo && t>=COVEIRO_CHARGE_FIRST_END && boss->attackBurst==0) {
            boss->attackBurst=1;boss->attackConnected=false;
            Vector2 offset=Vector2Subtract(player,boss->position);
            if(Vector2LengthSqr(offset)>.01f) boss->attackDirection=Vector2Normalize(offset);
            boss->facing=fabsf(offset.x)>fabsf(offset.y)?(offset.x<0?2:3):(offset.y<0?1:0);
        }
        bool chargeWindow=phaseTwo?((t>=COVEIRO_WINDUP && t<COVEIRO_CHARGE_FIRST_END)||
                                     (t>=COVEIRO_CHARGE_SECOND_START && t<COVEIRO_CHARGE_SECOND_END)):
                                    (t>=COVEIRO_WINDUP && t<COVEIRO_CHARGE_FIRST_PHASE_END);
        if (boss->bossAttack == COVEIRO_CHARGE && chargeWindow) {
            float speed = phaseTwo ? COVEIRO_ENRAGED_CHARGE_SPEED : COVEIRO_CHARGE_SPEED;
            MoveEnemy(map, boss, Vector2Scale(boss->attackDirection, speed * dt));
            if (!boss->attackConnected && Vector2Distance(player, boss->position) < 58 &&
                MapHasClearPath(map, boss->position, player))
                boss->attackConnected = HitPlayer(group, dodging, health, (int)roundf(COVEIRO_CHARGE_DAMAGE * boss->power));
        }
        /* Follow-through has real movement, while the initial targeting stays locked. */
        if(boss->bossAttack!=COVEIRO_CHARGE && t>=COVEIRO_WINDUP && t<COVEIRO_WINDUP+.22f)
            MoveEnemy(map,boss,Vector2Scale(boss->attackDirection,110*COVEIRO_MOVEMENT_SCALE*dt));
        if (t >= (phaseTwo && boss->bossAttack==COVEIRO_CHARGE?
                  COVEIRO_DOUBLE_CHARGE_END:COVEIRO_ATTACK_END)) { boss->state = ENEMY_IDLE; boss->stateTime = 0; }
        return;
    }
    if (*health <= 0) return;
    Vector2 offset = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(offset);
    int previousFacing = boss->facing;
    boss->facing = fabsf(offset.x) > fabsf(offset.y) ? (offset.x < 0 ? 2 : 3) : (offset.y < 0 ? 1 : 0);
    bool melee = distance <= COVEIRO_MELEE_RANGE && boss->guardianMeleeCooldown <= 0 &&
                 MapHasClearPath(map, boss->position, player);
    if (melee) {
        const Vector2 directions[] = {{0, 1}, {0, -1}, {-1, 0}, {1, 0}};
        boss->state = ENEMY_ATTACK;
        boss->stateTime = 0;
        boss->attackConnected = false;
        boss->guardianMelee = true;
        boss->guardianMeleeCooldown = phaseTwo ? 1.25f : COVEIRO_MELEE_COOLDOWN;
        boss->attackDirection = directions[boss->facing];
    } else if (distance <= 560 && boss->attackCooldown <= 0) {
        bool clearPath = MapHasClearPath(map,boss->position,player);
        int lastSkill = boss->bossAttack;
        int skill;
        if (boss->coveiroPhaseOpening)
            skill = COVEIRO_CHARGE;
        else if (distance > 285 && clearPath)
            skill = COVEIRO_CHARGE;
        else if (!clearPath)
            skill = COVEIRO_ROOTS;
        else if (distance > 175)
            skill = lastSkill == COVEIRO_ROOTS ? COVEIRO_SHOVEL : COVEIRO_ROOTS;
        else
            skill = lastSkill == COVEIRO_SHOVEL ? COVEIRO_ROOTS : COVEIRO_SHOVEL;
        boss->bossAttack = skill;
        boss->coveiroPhaseOpening = false;
        boss->state = ENEMY_ATTACK;
        boss->stateTime = 0;
        boss->attackConnected = false;
        boss->attackBurst=0;
        boss->attackTarget = player;
        boss->attackDirection = distance > .01f ? Vector2Normalize(offset) : (Vector2){0, 1};
        boss->attackCooldown = phaseTwo ? 1.75f : 2.65f;
    } else if (distance > 110) {
        Vector2 start = boss->position;
        float speed = boss->speed * COVEIRO_MOVEMENT_SCALE;
        MoveEnemy(map, boss, Vector2Scale(Vector2Normalize(offset), speed * (phaseTwo ? 1.5f : 1) * dt));
        Vector2 step = Vector2Subtract(boss->position, start);
        float traveled = Vector2Length(step);
        boss->state = traveled > .01f ? ENEMY_WALK : ENEMY_IDLE;
        if (boss->state == ENEMY_WALK) {
            boss->walkTime += traveled / speed;
            float dx = fabsf(step.x), dy = fabsf(step.y);
            // Evita trocar a vista a cada quadro quando o vetor fica perto de 45 graus.
            bool horizontal = previousFacing >= 2 ? dx > dy * .85f : dx > dy / .85f;
            boss->facing = horizontal ? (step.x < 0 ? 2 : 3) : (step.y < 0 ? 1 : 0);
        }
    } else boss->state = ENEMY_IDLE;
}

void CemeteryUpdateSpecial(EnemyGroup *group, const Clearing *map, Enemy *enemy, Vector2 player,
                           float dt, bool dodging, int *health) {
    if (enemy->state == ENEMY_ATTACK) {
        float t = enemy->stateTime;
        float dashTime = fminf(t, CEM_SPECIAL_IMPACT_TIME) - fmaxf(t - dt, CEM_SPECIAL_DASH_START);
        if (dashTime > 0.0f) {
            Vector2 remaining = Vector2Subtract(enemy->attackTarget, enemy->position);
            float distanceLeft = Vector2DotProduct(remaining, enemy->attackDirection);
            float step = Clamp(distanceLeft, 0.0f, CEM_SPECIAL_DASH_SPEED * dashTime);
            if (step > 0.0f)
                MoveEnemy(map, enemy, Vector2Scale(enemy->attackDirection, step));
        }
        if (enemy->stateTime >= CEM_SPECIAL_IMPACT_TIME && !enemy->attackConnected) {
            enemy->attackConnected = true;
            if (Vector2Distance(enemy->position, player) <= CEM_SPECIAL_RADIUS &&
                MapHasClearPath(map, enemy->position, player))
                HitPlayer(group, dodging, health, enemy->damageScale>0 ? (int)roundf(18 * enemy->power * enemy->damageScale) : (int)(18 * enemy->power));
        }
        if (enemy->stateTime >= CEM_SPECIAL_ATTACK_DURATION) {
            enemy->state = ENEMY_IDLE;
            enemy->stateTime = 0;
        }
        return;
    }
    if (*health <= 0) return;
    Vector2 offset = Vector2Subtract(player, enemy->position);
    float distance = Vector2Length(offset);
    if (distance > .01f)
        enemy->facing = fabsf(offset.x) > fabsf(offset.y) ? (offset.x < 0 ? 2 : 3) : (offset.y < 0 ? 1 : 0);
    if (distance <= CEM_SPECIAL_ATTACK_RANGE && enemy->attackCooldown <= 0 &&
        MapHasClearPath(map, enemy->position, player)) {
        enemy->attackDirection = Vector2Normalize(offset);
        float maxDash = CEM_SPECIAL_DASH_SPEED * (CEM_SPECIAL_IMPACT_TIME - CEM_SPECIAL_DASH_START);
        float dashDistance = Clamp(distance - CEM_SPECIAL_STOP_DISTANCE, 0.0f, maxDash);
        enemy->attackTarget = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, dashDistance));
        enemy->state = ENEMY_ATTACK;
        enemy->stateTime = 0;
        enemy->attackConnected = false;
        enemy->attackCooldown = 2.25f / fminf(enemy->power, 1.36f);
    } else if (distance > CEM_SPECIAL_RADIUS * .8f) {
        enemy->state = ENEMY_WALK;
        enemy->walkTime += dt;
        MoveEnemy(map, enemy, Vector2Scale(offset, .80f * enemy->speed * dt / distance));
    } else if (enemy->state != ENEMY_IDLE) {
        enemy->state = ENEMY_IDLE;
        enemy->stateTime = 0;
    }
}

CemeteryPose CemeteryAnimationPose(const Enemy *enemy) {
    CemeteryPose p;
    if (enemy->type == ENEMY_BOSS) {
        const int rows[] = {0, 3, 1, 2}; // frente, costas, esquerda, direita
        p = (CemeteryPose){CEM_BOSS_IDLE, 8, 4, rows[enemy->facing], 0, false};
        float t = enemy->stateTime;
        if (enemy->state == ENEMY_SPAWNING) {
            p.atlas = CEM_BOSS_EMERGE;
            p.frame = (int)Clamp(t * p.columns, 0, p.columns - 1);
        } else if (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN) {
            p.atlas = CEM_BOSS_FALL;
            p.frame = enemy->state == ENEMY_FALLEN ? p.columns - 1
                : (int)Clamp(t / .85f * p.columns, 0, p.columns - 1);
        } else if (enemy->state == ENEMY_WALK) {
            p.atlas = CEM_BOSS_WALK;
            p.frame = (int)(enemy->walkTime * 11) % p.columns;
        } else if (enemy->state == ENEMY_IDLE) {
            p.frame = (int)(t * 6) % p.columns;
        } else if (enemy->state == ENEMY_ATTACK) {
            p.atlas = enemy->guardianMelee ? CEM_BOSS_MELEE : CEM_BOSS_SLAM + enemy->bossAttack;
            if (enemy->guardianMelee) {
                p.frame = t < COVEIRO_MELEE_WINDUP ? (int)Clamp(t / COVEIRO_MELEE_WINDUP * 4, 0, 3)
                    : 4 + (int)Clamp((t - COVEIRO_MELEE_WINDUP) /
                                     (COVEIRO_MELEE_DURATION - COVEIRO_MELEE_WINDUP) * 4, 0, 3);
            } else if (enemy->bossAttack == COVEIRO_CHARGE) {
                bool phaseTwo = enemy->guardianPhase == GUARDIAN_PHASE_TWO;
                if (t < COVEIRO_WINDUP) p.frame = (int)Clamp(t / COVEIRO_WINDUP * 3, 0, 2);
                else if (phaseTwo && t >= COVEIRO_CHARGE_FIRST_END && t < COVEIRO_CHARGE_SECOND_START)
                    p.frame = t < COVEIRO_CHARGE_FIRST_END+.13f ? 6 : 2;
                else if (t < (phaseTwo ? COVEIRO_CHARGE_SECOND_END : COVEIRO_CHARGE_FIRST_PHASE_END)) {
                    float runTime = t - (phaseTwo && t >= COVEIRO_CHARGE_SECOND_START ?
                                         COVEIRO_CHARGE_SECOND_START : COVEIRO_WINDUP);
                    p.frame = 3 + (int)(runTime * 12) % 3;
                } else p.frame = t < (phaseTwo ? 1.72f : 1.34f) ? 6 : 7;
            } else {
                // A pá some da mão no mesmo instante em que o projétil nasce.
                int release = enemy->bossAttack == COVEIRO_SHOVEL ? 5 : 4;
                p.frame = t < COVEIRO_WINDUP ? (int)Clamp(t / COVEIRO_WINDUP * release, 0, release - 1)
                    : release + (int)Clamp((t - COVEIRO_WINDUP) / .85f * (8 - release), 0, 7 - release);
            }
        }
        return p;
    }
    const int rows[]={0,2,3,1}; // down, up, left, right
    if (enemy->type == ENEMY_SPECIAL) {
        p = (CemeteryPose){CEM_SPECIAL_IDLE,4,4,rows[enemy->facing],0,false};
        if (enemy->state == ENEMY_WALK) {
            p.atlas = CEM_SPECIAL_WALK;
            p.columns = 6;
            p.frame = (int)(enemy->walkTime * 8) % 6;
        } else if (enemy->state == ENEMY_ATTACK) {
            p.atlas = CEM_SPECIAL_ATTACK;
            p.columns = 6;
            float t = enemy->stateTime;
            if (t < CEM_SPECIAL_IMPACT_TIME) p.frame = (int)fminf(2, t / CEM_SPECIAL_IMPACT_TIME * 3);
            else if (t < .67f) p.frame = 3;
            else if (t < .84f) p.frame = 4;
            else p.frame = 5;
        } else if (enemy->state == ENEMY_IDLE) p.frame = (int)(enemy->stateTime * 4) % 4;
        return p;
    }
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

/* The original eight poses still define attack timing; only the in-between row changes. */
static int CoveiroMotionSample(const Enemy *enemy) {
    float t = enemy->stateTime;
    float position = 0;
    if (enemy->state == ENEMY_WALK) position = enemy->walkTime * 11;
    else if (enemy->state == ENEMY_IDLE) position = t * 6;
    else if (enemy->state == ENEMY_SPAWNING) position = t * 8;
    else if (enemy->state == ENEMY_DIE) position = t / .85f * 8;
    else if (enemy->state == ENEMY_ATTACK) {
        if (enemy->guardianMelee) {
            position = t < COVEIRO_MELEE_WINDUP ? t / COVEIRO_MELEE_WINDUP * 4 :
                4 + (t - COVEIRO_MELEE_WINDUP) / (COVEIRO_MELEE_DURATION - COVEIRO_MELEE_WINDUP) * 4;
        } else if (enemy->bossAttack == COVEIRO_CHARGE) {
            bool second = enemy->guardianPhase == GUARDIAN_PHASE_TWO;
            if (t < COVEIRO_WINDUP) position = t / COVEIRO_WINDUP * 3;
            else if (second && t >= COVEIRO_CHARGE_FIRST_END && t < COVEIRO_CHARGE_SECOND_START) position = 0;
            else if (t < (second ? COVEIRO_CHARGE_SECOND_END : COVEIRO_CHARGE_FIRST_PHASE_END)) {
                float runTime = t - (second && t >= COVEIRO_CHARGE_SECOND_START ?
                                     COVEIRO_CHARGE_SECOND_START : COVEIRO_WINDUP);
                position = runTime * 12;
            } else position = 0;
        } else {
            int release = enemy->bossAttack == COVEIRO_SHOVEL ? 5 : 4;
            position = t < COVEIRO_WINDUP ? t / COVEIRO_WINDUP * release :
                release + (t - COVEIRO_WINDUP) / .85f * (8 - release);
        }
    }
    float fraction = position - floorf(position);
    return (int)Clamp(fraction * 3, 0, 2);
}

void CemeteryDrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE) {
            if (enemy->pickupAvailable) DrawHealthPickup(enemy->position);
            continue;
        }
        if (enemy->state == ENEMY_QUEUED) continue;
        if (enemy->state == ENEMY_SPAWNING && enemy->type != ENEMY_BOSS) {
            if (enemy->stateTime >= 0)
                DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                24 + 10 * sinf(enemy->stateTime*8), GOLD);
            continue;
        }

        bool boss = enemy->type == ENEMY_BOSS;
        bool special = enemy->type == ENEMY_SPECIAL;
        bool fallen = enemy->state == ENEMY_FALLEN;
        float collapse = boss ? 0.0f : fallen ? 1.0f :
            ((boss || special) && enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / .85f, 0.0f, 1.0f) : 0.0f);
        CemeteryPose pose=CemeteryAnimationPose(enemy);
        const Texture2D atlases[]={group->coveiroWalk,group->boss[0],group->boss[1],group->boss[2],group->coveiroMelee,
            group->cemeteryZombie[0],group->cemeteryZombie[1],group->cemeteryZombie[2],
            group->cemeteryLantern[0],group->cemeteryLantern[1],group->cemeteryLantern[2],
            group->cemeterySpecial[0],group->cemeterySpecial[1],group->cemeterySpecial[2],
            group->coveiroIdle,group->coveiroEmerge,group->coveiroFall};
        Texture2D tex=atlases[pose.atlas];
        int frame=pose.frame,row=pose.row;
        if (boss) {
            row += CoveiroMotionSample(enemy) * pose.rows;
            pose.rows *= 3;
        }
        float cw=(float)tex.width/pose.columns,ch=(float)tex.height/pose.rows;
        float baseSize=boss?288*(210.0f/256.0f):special?128:96;
        float height=baseSize*(1.0f-.22f*collapse);
        float width=baseSize*cw/ch;
        float bob=0;
        float lean=0;
        if(boss && !fallen && enemy->state!=ENEMY_DIE) {
            if (enemy->state == ENEMY_IDLE) bob=sinf(enemy->stateTime*5)*1.2f;
        } else if ((boss || special) && collapse > 0.0f) {
            lean = (enemy->facing == 2 ? -1.0f : 1.0f) * 82.0f * collapse;
        }
        Color tint = enemy->state == ENEMY_HURT ? RED : WHITE;
        if (!boss && !special && enemy->state==ENEMY_DIE) tint=Fade(tint,fmaxf(0,1-enemy->stateTime/.45f));
        if ((boss || special) && (fallen || enemy->state == ENEMY_DIE))
            tint = Fade((Color){215, 190, 225, 255}, .78f);
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
        if (special && enemy->state == ENEMY_ATTACK && enemy->stateTime < CEM_SPECIAL_IMPACT_TIME) {
            Color warning = Fade(VIOLET, .25f + .45f * enemy->stateTime / CEM_SPECIAL_IMPACT_TIME);
            DrawLineEx(enemy->position, enemy->attackTarget, 3, warning);
            DrawCircleLines((int)enemy->attackTarget.x, (int)enemy->attackTarget.y, CEM_SPECIAL_RADIUS, warning);
        }
        DrawTexturePro(tex, (Rectangle){frame*cw, row*ch, pose.flip?-cw:cw, ch},
            (Rectangle){enemy->position.x, enemy->position.y+bob, width, height},
            (Vector2){width/2,boss?height*(272.0f/288.0f):height*.914f}, lean, tint);

        if (fallen) DrawGameText("E: selar", (int)enemy->position.x-30, (int)enemy->position.y+10, 16, GOLD);
        if (!boss && enemy->health < enemy->maxHealth)
            DrawEnemyHealthBar(enemy, (int)(enemy->position.y-height)-8);
        if (boss && enemy->state == ENEMY_ATTACK && !enemy->guardianMelee) {
            Color warning = enemy->stateTime < COVEIRO_WINDUP ? Fade(VIOLET, .45f) : VIOLET;
            bool phaseTwo = enemy->guardianPhase == GUARDIAN_PHASE_TWO;
            if(enemy->bossAttack==COVEIRO_SHOVEL && enemy->stateTime<COVEIRO_WINDUP) {
                for(int n=phaseTwo?-1:0;n<=(phaseTwo?1:0);n++) {
                    Vector2 direction=Vector2Rotate(enemy->attackDirection,n*.25f);
                    DrawLineEx(enemy->position,Vector2Add(enemy->position,Vector2Scale(direction,180)),3,warning);
                }
            }
            if (enemy->bossAttack == COVEIRO_ROOTS && enemy->stateTime < COVEIRO_ROOT_END)
                DrawCircleLines((int)enemy->attackTarget.x, (int)enemy->attackTarget.y, phaseTwo ? 115 : 95, warning);
            if (enemy->bossAttack == COVEIRO_CHARGE && (enemy->stateTime < COVEIRO_WINDUP ||
                (phaseTwo && enemy->stateTime>=COVEIRO_CHARGE_FIRST_END &&
                 enemy->stateTime<COVEIRO_CHARGE_SECOND_START)))
                DrawLineEx(enemy->position, Vector2Add(enemy->position,
                    Vector2Scale(enemy->attackDirection,
                                 (phaseTwo ? COVEIRO_ENRAGED_CHARGE_SPEED : COVEIRO_CHARGE_SPEED) * .55f)), 5, warning);
        }
    }
}

static void DrawEffect(Texture2D tex, Vector2 position, int frame, int columns, float size) {
    float w = (float)tex.width/columns, h = (float)tex.height/2;
    DrawTexturePro(tex, (Rectangle){(frame%columns)*w,(frame/columns)*h,w,h},
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
        else DrawEffect(group->effects[0],root->position,(int)fminf((root->age-.8f)*20,15),8,124);
    }
    if (boss->type == ENEMY_BOSS && boss->state == ENEMY_ATTACK && !boss->guardianMelee && boss->stateTime >= COVEIRO_WINDUP) {
        int frame = (int)fminf((boss->stateTime-COVEIRO_WINDUP)/1.2f*8, 7);
        if (boss->bossAttack == COVEIRO_ROOTS)
            DrawEffect(group->effects[0], boss->attackTarget, (int)Clamp((boss->stateTime-COVEIRO_WINDUP)/.85f*16,0,15), 8,
                       boss->guardianPhase == GUARDIAN_PHASE_TWO ? 230 : 190);
        if (boss->bossAttack == COVEIRO_CHARGE)
            DrawEffect(group->effects[2], boss->position, frame, 4, 140);
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
