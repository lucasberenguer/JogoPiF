#include "enemy.h"
#include "run_progress.h"
#include "raymath.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static void CheckCoveiroWalk(void) {
    Clearing map = {.cemetery=true, .background={.width=1600,.height=1200},
                    .playableBounds={0,0,1600,1200}};
    EnemyGroup group = {0};
    Vector2 start = {800,600};
    const Vector2 directions[] = {{0,1},{0,-1},{-1,0},{1,0},
                                  {-1,-1},{1,-1},{-1,1},{1,1}};
    const int faces[] = {0,1,2,3,2,3,2,3};
    for (int i=0; i<8; i++) {
        Vector2 axis = Vector2Normalize(directions[i]);
        Vector2 target = Vector2Add(start,Vector2Scale(axis,400));
        Enemy boss = {.type=ENEMY_BOSS,.state=ENEMY_WALK,.position=start,.speed=160,
                      .facing=faces[i],.attackCooldown=10,.guardianMeleeCooldown=10};
        int health = 100;
        for (int frame=0; frame<30; frame++) {
            CemeteryUpdateBoss(&group,&map,&boss,target,1.0f/60,false,&health);
            Vector2 traveled = Vector2Subtract(boss.position,start);
            assert(boss.state==ENEMY_WALK && boss.facing==faces[i]);
            assert(Vector2DotProduct(traveled,axis)>0);
            assert(fabsf(traveled.x*axis.y-traveled.y*axis.x)<.02f);
        }
    }
}

static void CheckCoveiroChoices(void) {
    Clearing map = {.cemetery=true,.background={.width=1600,.height=1200},
                    .playableBounds={0,0,1600,1200}};
    EnemyGroup group={0};
    Enemy *boss=&group.list[0];
    Vector2 player={800,600};
    int health=100;
    const struct {float distance; int previous, expected;} cases[] = {
        {350,COVEIRO_ROOTS,COVEIRO_CHARGE},
        {220,COVEIRO_SHOVEL,COVEIRO_ROOTS},
        {220,COVEIRO_ROOTS,COVEIRO_CHARGE},
        {220,COVEIRO_CHARGE,COVEIRO_SHOVEL},
        {145,COVEIRO_ROOTS,COVEIRO_SHOVEL},
    };
    for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        *boss=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_IDLE,
            .position={player.x-cases[i].distance,player.y},.speed=160,
            .health=100,.maxHealth=100,.power=1,.bossAttack=cases[i].previous};
        CemeteryUpdateBoss(&group,&map,boss,player,.01f,false,&health);
        assert(boss->state==ENEMY_ATTACK && boss->bossAttack==cases[i].expected);
    }
    map.obstacles[0]=(Rectangle){610,570,40,60};
    map.obstacleCount=1;
    *boss=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_IDLE,.position={450,600},
        .speed=160,.health=100,.maxHealth=100,.power=1,.bossAttack=COVEIRO_SHOVEL};
    CemeteryUpdateBoss(&group,&map,boss,player,.01f,false,&health);
    assert(boss->state==ENEMY_ATTACK && boss->bossAttack==COVEIRO_ROOTS);
    map.obstacleCount=0;
    *boss=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_IDLE,
        .position={player.x-220,player.y},.speed=160,.health=100,.maxHealth=100,
        .power=1,.guardianPhase=GUARDIAN_PHASE_TWO,.bossAttack=COVEIRO_ROOTS};
    CemeteryUpdateBoss(&group,&map,boss,player,.01f,false,&health);
    assert(boss->state==ENEMY_ATTACK && boss->bossAttack==COVEIRO_CHARGE);

    memset(&group,0,sizeof(group));
    boss=&group.list[0];
    *boss=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_ATTACK,
        .position={player.x-220,player.y},.health=100,.maxHealth=100,
        .power=1,.guardianPhase=GUARDIAN_PHASE_TWO,.bossAttack=COVEIRO_SHOVEL,
        .stateTime=.63f,.attackDirection={1,0},.attackTarget=player};
    CemeteryUpdateBoss(&group,&map,boss,player,.02f,false,&health);
    assert(group.cemeteryProjectiles[0].active && group.cemeteryProjectiles[1].active &&
           group.cemeteryProjectiles[2].active);
    assert(group.rootAttacks[0].active && group.rootAttacks[1].active);
    assert(fabsf(group.rootAttacks[0].position.y-player.y)==125 &&
           fabsf(group.rootAttacks[1].position.y-player.y)==125);
    assert(group.rootAttacks[0].position.y != group.rootAttacks[1].position.y);

    // An opener can link once, with a fresh target and its own windup.
    for (int scenario = 0; scenario < 3; scenario++) {
        memset(&group, 0, sizeof(group));
        bool shovelCombo = scenario == 1;
        *boss = (Enemy){.type=ENEMY_BOSS, .state=ENEMY_IDLE, .position={580,600},
            .health=100, .maxHealth=100, .power=1,
            .guardianPhase=shovelCombo ? GUARDIAN_PHASE_TWO : GUARDIAN_PHASE_ONE,
            .bossAttack=shovelCombo ? COVEIRO_CHARGE : COVEIRO_SHOVEL};
        CemeteryUpdateBoss(&group, &map, boss, player, .01f, false, &health);
        assert(boss->bossAttack == (shovelCombo ? COVEIRO_SHOVEL : COVEIRO_ROOTS));
        Vector2 moved = {800,720};
        if (scenario == 2) {
            map.obstacles[0] = (Rectangle){650,620,60,100};
            map.obstacleCount = 1;
        }
        boss->stateTime = 1.19f;
        CemeteryUpdateBoss(&group, &map, boss, moved, .01f, false, &health);
        if (scenario == 2) {
            assert(boss->bossAttack == COVEIRO_ROOTS && boss->stateTime > 1);
            map.obstacleCount = 0;
        } else {
            assert(boss->bossAttack == (shovelCombo ? COVEIRO_ROOTS : COVEIRO_CHARGE));
            assert(boss->stateTime == 0 && boss->attackTarget.y == moved.y);
        }
        boss->stateTime = 1.5f;
        CemeteryUpdateBoss(&group, &map, boss, moved, .01f, false, &health);
        assert(boss->state == ENEMY_IDLE && boss->coveiroCombo == COVEIRO_COMBO_RECOVERY);
        CemeteryUpdateBoss(&group, &map, boss, boss->position, .01f, false, &health);
        assert(boss->state == ENEMY_IDLE);
    }
}

static void CheckBossPrediction(void) {
    const struct { float distance; int previous; } cases[] = {
        {200, GUARDIAN_CURSE}, {220, COVEIRO_SHOVEL},
        {220, 0}, {500, BONE_RANGED}, {170, IGRIS_MELEE},
    };
    for (int i=0; i<5; i++) {
        Clearing map = {.cemetery=i==1, .village=i==2, .boneland=i==3, .templarField=i==4,
                        .background={.width=1600,.height=1200}, .playableBounds={0,0,1600,1200}};
        EnemyGroup group={0};
        Vector2 player={400+cases[i].distance,500};
        int health=100;
        StartArenaBoss(&group,&map,player,1);
        Enemy *boss=&group.list[0];
        boss->position=(Vector2){400,500};
        boss->state=ENEMY_IDLE;
        boss->attackCooldown=.06f;
        boss->bossAttack=cases[i].previous;
        UpdateEnemies(&group,&map,(Vector2){player.x,player.y-10},.05f,false,&health,100);
        boss->position=(Vector2){400,500};
        UpdateEnemies(&group,&map,player,.05f,false,&health,100);
        assert(boss->state==ENEMY_ATTACK && boss->attackDirection.y>.08f);
        if (i==0) assert(group.rootAttacks[0].active && group.rootAttacks[0].position.y>player.y+30);
        if (i==4) {
            for (int frame=0; frame<8; frame++) {
                player.y+=10;
                UpdateEnemies(&group,&map,player,.05f,false,&health,100);
            }
            assert(boss->attackTarget.y>player.y+20);
        }
        Vector2 locked=boss->attackDirection;
        player.y-=10;
        UpdateEnemies(&group,&map,player,.05f,false,&health,100);
        assert(Vector2Distance(locked,boss->attackDirection)<.001f && health==100);
    }
    Clearing map={.background={.width=1600,.height=1200}, .playableBounds={0,0,1600,1200},
                  .obstacles={{590,520,20,25}}, .obstacleCount=1};
    Vector2 player={600,500};
    Vector2 target=PredictBossTarget(&map,(Vector2){400,500},player,(Vector2){0,250},.24f);
    assert(Vector2Distance(target,player)<.001f);
}

static void CheckBonelandWaves(void) {
    Clearing map = {.boneland = true, .background = {.width = 1672, .height = 941},
                    .playableBounds = {0, 0, 1672, 941}, .spawn = {835, 840}};
    EnemyGroup group = {0};
    const int totals[] = {36, 38};
    const int rangedCounts[] = {7, 10};
    for (int wave = 1; wave <= BONELAND_WAVES; wave++) {
        StartEnemyWave(&group, &map, wave, map.spawn, 1);
        int ghouls = 0, cultists = 0, shells = 0;
        for (int i = 0; i < ENEMY_COUNT; i++) {
            const Enemy *enemy = &group.list[i];
            if (enemy->state == ENEMY_GONE) continue;
            assert(enemy->state == (i<22?ENEMY_SPAWNING:ENEMY_QUEUED));
            if(i<22) assert(!IsMapActorBlocked(&map, enemy->position));
            if (i > 0) assert(enemy->stateTime < group.list[i - 1].stateTime);
            if (enemy->type == ENEMY_COMMON_MELEE) {
                ghouls++;
                assert(enemy->maxHealth == 40);
            } else if (enemy->type == ENEMY_SPECIAL) {
                shells++;
                assert(enemy->maxHealth == 145);
            } else {
                cultists++;
                assert(enemy->type == ENEMY_COMMON_RANGED && enemy->maxHealth == 36);
            }
        }
        assert(EnemiesRemaining(&group) == totals[wave - 1]);
        assert(ghouls + cultists + shells == totals[wave - 1] && cultists == rangedCounts[wave - 1]);
        assert(shells == wave);
    }
    group.list[0].state=ENEMY_GONE;
    int reserveHealth=100;
    UpdateEnemies(&group,&map,map.spawn,.01f,true,&reserveHealth,100);
    assert(group.list[22].state==ENEMY_SPAWNING && group.list[23].state==ENEMY_QUEUED);
    for (int i = 0; i < ENEMY_COUNT; i++) group.list[i].state = ENEMY_GONE;
    Enemy *shell = &group.list[0];
    *shell = (Enemy){.type=ENEMY_SPECIAL,.state=ENEMY_IDLE,.position={500,400},.power=1,.health=310};
    int playerHealth = 100;
    Vector2 nearby = {605,400};
    UpdateEnemies(&group,&map,nearby,.01f,false,&playerHealth,100);
    assert(shell->state == ENEMY_ATTACK && playerHealth == 100);
    UpdateEnemies(&group,&map,nearby,.68f,false,&playerHealth,100);
    assert(playerHealth == 100);
    UpdateEnemies(&group,&map,nearby,.03f,false,&playerHealth,100);
    assert(playerHealth == 66);
    group.playerHitCooldown=0;
    UpdateEnemies(&group,&map,nearby,.1f,false,&playerHealth,100);
    assert(playerHealth == 66);
    Enemy *cultist = &group.list[0];
    *cultist = (Enemy){.type = ENEMY_COMMON_RANGED, .state = ENEMY_IDLE,
                      .position = {340, 400}, .power = 1, .health = 42};
    Vector2 player = {590, 400};
    int health = 100;
    map.obstacles[0] = (Rectangle){445, 370, 30, 60};
    map.obstacleCount = 1;
    UpdateEnemies(&group, &map, player, .01f, false, &health, 100);
    assert(cultist->state != ENEMY_ATTACK);
    map.obstacleCount = 0;
    UpdateEnemies(&group, &map, player, .01f, false, &health, 100);
    assert(cultist->state == ENEMY_ATTACK);
    player.y += 120;
    for (int i = 0; i < 11; i++) UpdateEnemies(&group, &map, player, .05f, false, &health, 100);
    assert(group.projectiles[0].remaining == 0);
    UpdateEnemies(&group, &map, player, .05f, false, &health, 100);
    assert(group.projectiles[0].remaining > 0);
    assert(group.projectiles[0].direction.x > .99f && fabsf(group.projectiles[0].direction.y) < .01f);
    assert(health == 100);
}

static void CheckBoneLordAttacks(void) {
    Clearing map = {.boneland = true, .background = {.width = 1200, .height = 900},
                    .playableBounds = {0, 0, 1200, 900}, .spawn = {600, 750}};
    EnemyGroup group = {0};
    Vector2 origin = {400, 400};
    const struct { int attack, previous, damage; float distance; bool evade; } scenarios[] = {
        {BONE_CLAW, BONE_BLAST, 22, 85, false},
        {BONE_RANGED, BONE_RAIN, 25, 500, false},
        {BONE_RAIN, BONE_RANGED, 28, 500, false},
        {BONE_RAIN, BONE_RANGED, 0, 500, true},
        {BONE_DASH, BONE_RAIN, 32, 270, false},
        {BONE_DASH, BONE_RAIN, 0, 270, true},
        {BONE_BLAST, BONE_DASH, 38, 200, false},
        {BONE_BLAST, BONE_DASH, 0, 200, true},
    };
    for (unsigned i = 0; i < sizeof(scenarios)/sizeof(scenarios[0]); i++) {
        int attack = scenarios[i].attack;
        Vector2 player = {origin.x + scenarios[i].distance, origin.y};
        int health = 100;
        StartArenaBoss(&group, &map, player, 1);
        Enemy *boss = &group.list[0];
        boss->position = origin;
        boss->state = ENEMY_IDLE;
        boss->attackCooldown = 0;
        boss->bossAttack = scenarios[i].previous;
        UpdateEnemies(&group, &map, player, .01f, false, &health, 100);
        assert(boss->state == ENEMY_ATTACK && boss->bossAttack == attack);
        bool projectile = false;
        for (int frame = 0; frame < 38; frame++) {
            if (attack == BONE_RAIN && scenarios[i].evade && frame == 10) player.y += 130;
            UpdateEnemies(&group, &map, player, .05f, scenarios[i].evade && attack != BONE_RAIN, &health, 100);
            assert(!IsMapActorBlocked(&map, boss->position));
            if (frame == 13 && attack == BONE_DASH) {
                assert(fabsf(boss->position.y - origin.y) > 60);
                assert(health == 100);
            }
            if (group.projectiles[0].remaining > 0 && group.projectiles[0].boneShot) projectile = true;
            if (boss->state != ENEMY_ATTACK && (attack != BONE_RANGED || !group.projectiles[0].remaining)) break;
        }
        assert(health == 100 - scenarios[i].damage);
        if (attack == BONE_RANGED) assert(projectile);
    }
    Vector2 player = {origin.x + 330, origin.y};
    int health = 100;
    StartArenaBoss(&group, &map, player, 1);
    Enemy *boss = &group.list[0];
    boss->position = origin;
    boss->state = ENEMY_IDLE;
    boss->bossAttack = BONE_DASH;
    boss->attackCooldown = 0;
    UpdateEnemies(&group, &map, player, .05f, false, &health, 100);
    assert(boss->state == ENEMY_WALK && boss->bossAttack == BONE_DASH);
    for (int i = 0; i < 30 && boss->state != ENEMY_ATTACK; i++)
        UpdateEnemies(&group, &map, player, .05f, false, &health, 100);
    assert(boss->state == ENEMY_ATTACK && boss->bossAttack == BONE_BLAST);
}

static void CheckGuardianAttacks(Clearing *map, EnemyGroup *group) {
    Vector2 player = {480, 400};
    int health = 100;
    StartArenaBoss(group, map, player, 1.0f);
    Enemy *boss = &group->list[0];
    boss->position = (Vector2){340, 400};
    boss->state = ENEMY_IDLE;
    boss->attackCooldown = 0;
    UpdateEnemies(group, map, player, .01f, false, &health, 100);
    assert(group->rootAttacks[0].active && group->rootAttacks[0].attack == GUARDIAN_ROOTS);

    // Finish the first cast and force the cooldown to zero to exercise the alternation.
    for (int i = 0; i < 33; i++) UpdateEnemies(group, map, player, .05f, true, &health, 100);
    boss->position = (Vector2){340, 400};
    boss->attackCooldown = 0;
    map->obstacles[0] = (Rectangle){600, 370, 30, 60};
    map->obstacleCount = 1;
    UpdateEnemies(group, map, player, .01f, false, &health, 100);
    assert(group->rootAttacks[0].attack == GUARDIAN_ROOT_WAVE);
    assert(group->rootAttacks[1].attack == GUARDIAN_ROOT_WAVE);
    assert(group->rootAttacks[1].age < group->rootAttacks[0].age);
    assert(!group->rootAttacks[2].active); /* The rock cuts off the rest of the line. */

    for (int i = 0; i < 16; i++) UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(health == 100); /* Warning and early emergence. */
    UpdateEnemies(group, map, player, .05f, true, &health, 100);
    assert(health == 100);
    UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(health == 64);
    UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(health == 64);
    for (int i = 0; i < 16; i++) UpdateEnemies(group, map, player, .05f, true, &health, 100);
    boss->position = (Vector2){340, 400};
    boss->attackCooldown = 0;
    health = 100;
    UpdateEnemies(group, map, player, .01f, false, &health, 100);
    EnemyRoot *curse = &group->rootAttacks[0];
    assert(curse->active && curse->attack == GUARDIAN_CURSE);
    assert(curse->position.x == player.x && curse->position.y == player.y);
    for (int i = 0; i < 21; i++) UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(health == 100);
    UpdateEnemies(group, map, (Vector2){player.x, player.y + 120}, .05f, false, &health, 100);
    assert(health == 100 && curse->position.y == player.y);
    UpdateEnemies(group, map, player, .05f, true, &health, 100);
    assert(health == 100);
    UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(health == 58 && curse->hit);
    UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(health == 58);
    DamageEnemy(boss, boss->health);
    UpdateEnemies(group, map, player, .05f, false, &health, 100);
    assert(!group->rootAttacks[0].active && !group->rootAttacks[1].active);
    map->obstacleCount = 0;
}

static void CheckGuardianMelee(Clearing *map, EnemyGroup *group) {
    const Vector2 origin = {400, 400};
    const Vector2 player = {500, 400};
    // Hit, move behind, dodge, leave the range, or interrupt with a phase change.
    for (int scenario = 0; scenario < 5; scenario++) {
        int health = 100;
        StartArenaBoss(group, map, player, 1.0f);
        Enemy *boss = &group->list[0];
        boss->position = origin;
        boss->state = ENEMY_IDLE;
        UpdateEnemies(group, map, player, .01f, false, &health, 100);
        assert(boss->state == ENEMY_ATTACK && boss->guardianMelee && boss->facing == 3);
        assert(boss->bossAttack == GUARDIAN_ATTACK_COUNT - 1);
        UpdateEnemies(group, map, player, .35f, false, &health, 100);
        assert(health == 100 && EnemyAnimationFrame(boss) == 3);
        assert(boss->position.x > origin.x && boss->position.x <= origin.x+16);
        Vector2 impactPosition = player;
        if (scenario == 1) impactPosition.x = origin.x - 100;
        if (scenario == 3) impactPosition.x = origin.x + 135;
        if (scenario == 4) DamageEnemy(boss, boss->health);
        UpdateEnemies(group, map, impactPosition, .07f, scenario == 2, &health, 100);
        assert(health == (scenario == 0 ? 74 : 100));
        UpdateEnemies(group, map, player, .2f, false, &health, 100);
        assert(health == (scenario == 0 ? 74 : 100));
    }
}

static void CheckGuardianClosePressure(Clearing *map, EnemyGroup *group) {
    const Vector2 origin = {400,400}, player = {527,400};
    int health = 100;
    StartArenaBoss(group,map,player,1);
    Enemy *boss = &group->list[0];
    boss->position = origin;
    boss->state = ENEMY_IDLE;
    boss->attackCooldown = 0;
    UpdateEnemies(group,map,player,.16f,false,&health,100);
    assert(boss->state == ENEMY_WALK && boss->position.x > origin.x);
    UpdateEnemies(group,map,player,.01f,false,&health,100);
    assert(boss->state == ENEMY_ATTACK && boss->guardianMelee);
    UpdateEnemies(group,map,player,.41f,false,&health,100);
    assert(health == 74);
    UpdateEnemies(group,map,player,.40f,false,&health,100);
    Vector2 feet = boss->position;
    UpdateEnemies(group,map,player,.10f,false,&health,100);
    assert(boss->state == ENEMY_IDLE && Vector2Distance(feet,boss->position) < .01f);
    UpdateEnemies(group,map,player,.26f,false,&health,100);
    assert(boss->state == ENEMY_ATTACK && boss->guardianMelee);
    UpdateEnemies(group,map,player,.41f,false,&health,100);
    assert(health == 48);
}

static void CheckGuardianDodge(Clearing *map, EnemyGroup *group) {
    Vector2 player={500,400};
    int health=100;
    StartArenaBoss(group,map,player,1);
    Enemy *boss=&group->list[0];
    boss->position=(Vector2){340,400};
    boss->state=ENEMY_IDLE;
    boss->attackCooldown=1;
    boss->guardianMeleeCooldown=1;
    UpdateEnemies(group,map,player,.01f,false,&health,100);
    assert(boss->guardianDodgeTime>0 && boss->guardianDodgeCooldown>3);
    UpdateEnemies(group,map,player,.05f,false,&health,100);
    assert(fabsf(boss->position.y-400)>1 && health==100);
    boss->attackCooldown=0;
    UpdateEnemies(group,map,player,.01f,false,&health,100);
    assert(boss->state==ENEMY_ATTACK && boss->guardianDodgeTime==0);
}

static void CheckTemplarBalance(void) {
    Clearing map = {.templarField = true, .background = {.width = 1672, .height = 941},
                    .playableBounds = {0, 0, 1672, 941}, .spawn = {835, 840}};
    EnemyGroup group = {0};
    const int health[] = {
        [TEMPLAR_SHIELD] = 218, [TEMPLAR_EXECUTIONER] = 280,
        [TEMPLAR_CROSSBOW] = 156, [TEMPLAR_STANDARD] = 187,
    };
    for (int wave = 1; wave <= 2; wave++) {
        StartEnemyWave(&group, &map, wave, map.spawn, 1);
        int count = 0;
        for (int i = 0; i < ENEMY_COUNT; i++) {
            const Enemy *enemy = &group.list[i];
            if (enemy->state == ENEMY_GONE) continue;
            assert(enemy->templarRole >= TEMPLAR_SHIELD && enemy->templarRole < TEMPLAR_STANDARD);
            assert(enemy->health == health[enemy->templarRole]);
            assert(enemy->maxHealth == enemy->health);
            count++;
        }
        assert(count == 5);
    }

    const TemplarRole roles[] = {TEMPLAR_SHIELD, TEMPLAR_EXECUTIONER, TEMPLAR_CROSSBOW};
    const float settle[] = {.74f, 1.26f, 1.00f};
    for (int i=0; i<3; i++) {
        StartEnemyWave(&group, &map, 1, map.spawn, 1);
        for (int j=1; j<ENEMY_COUNT; j++) group.list[j].state=ENEMY_GONE;
        Enemy *enemy=&group.list[0];
        enemy->templarRole=roles[i];
        enemy->type=i==2 ? ENEMY_COMMON_RANGED : i==1 ? ENEMY_SPECIAL : ENEMY_COMMON_MELEE;
        enemy->state=ENEMY_ATTACK;
        enemy->stateTime=settle[i]-.02f;
        enemy->attackConnected=true;
        int hp=100;
        UpdateEnemies(&group,&map,map.spawn,.03f,true,&hp,100);
        assert(enemy->state==ENEMY_IDLE);
    }

    StartEnemyWave(&group, &map, 2, map.spawn, 1);
    for (int i = 1; i < ENEMY_COUNT; i++) group.list[i].state = ENEMY_GONE;
    Enemy *special = &group.list[0];
    for (int scenario = 0; scenario < 3; scenario++) {
        special->position = (Vector2){600,500};
        special->state = ENEMY_IDLE;
        special->attackCooldown = 0;
        group.playerHitCooldown = 0;
        Vector2 target = {830,500};
        int hp = 100;
        map.obstacleCount = 0;
        UpdateEnemies(&group,&map,target,.01f,false,&hp,100);
        assert(special->state == ENEMY_ATTACK);
        UpdateEnemies(&group,&map,target,.48f,false,&hp,100);
        assert(special->position.x == 600 && hp == 100);
        if (scenario == 2) {
            map.obstacles[0] = (Rectangle){680,460,20,80};
            map.obstacleCount = 1;
        }
        UpdateEnemies(&group,&map,target,.29f,scenario == 1,&hp,100);
        assert(scenario == 2 ? special->position.x < 680 : special->position.x > 750);
        assert(hp == (scenario == 0 ? 73 : 100));
    }
    map.obstacleCount = 0;

    StartArenaBoss(&group, &map, map.spawn, 1);
    Enemy *marshal = &group.list[0];
    assert(marshal->templarRole == TEMPLAR_MARSHAL && marshal->maxHealth == 3000);
    marshal->position = (Vector2){835, 500};
    marshal->health = marshal->maxHealth / 2;
    marshal->guardianPhase = GUARDIAN_PHASE_TWO;
    marshal->state = ENEMY_IDLE;
    marshal->attackCooldown = 0;
    Vector2 player = {955, 500};
    const int skills[] = {IGRIS_ECLIPSE, IGRIS_SPEARS, IGRIS_VORTEX, IGRIS_ERUPTION, IGRIS_MELEE};
    const int previous[] = {IGRIS_MELEE, IGRIS_ECLIPSE, IGRIS_ECLIPSE, IGRIS_SPEARS, IGRIS_ECLIPSE};
    const float choiceDistance[] = {120,450,160,300,80};
    const int damage[] = {31,28,35,36,22};
    const float impact[] = {.96f,.59f,1.00f,1.12f,.42f};
    for (int i = 0; i < IGRIS_SKILL_COUNT; i++) {
        marshal->state = ENEMY_IDLE;
        marshal->attackCooldown = 0;
        marshal->attackBurst = i == 0 ? 0 : 1;
        marshal->bossAttack = previous[i];
        marshal->position = (Vector2){player.x-choiceDistance[i],player.y};
        group.playerHitCooldown = 0;
        int playerHealth = 100;
        UpdateEnemies(&group,&map,player,.01f,false,&playerHealth,100);
        assert(marshal->bossAttack == skills[i] && playerHealth == 100);
        marshal->position = (Vector2){835,500};
        marshal->attackTarget = player;
        marshal->attackDirection = (Vector2){1,0};
        marshal->stateTime = 0;
        for (int j = 0; j < 24; j++) {
            UpdateEnemies(&group,&map,player,.05f,false,&playerHealth,100);
            if ((j+1)*.05f < impact[i]-.001f) assert(playerHealth==100);
            if (skills[i] == IGRIS_SPEARS && marshal->attackConnected && playerHealth == 100) {
                assert(group.projectiles[0].umbraSpear && group.projectiles[1].umbraSpear &&
                       group.projectiles[2].umbraSpear);
            }
        }
        assert(playerHealth == 100-damage[i]);
        if (skills[i] == IGRIS_ECLIPSE) assert(marshal->position.x > 870);
        if (skills[i] == IGRIS_ECLIPSE || skills[i] == IGRIS_VORTEX || skills[i] == IGRIS_ERUPTION) {
            for (int tick=0; tick<40 && marshal->state==ENEMY_ATTACK; tick++)
                UpdateEnemies(&group,&map,player,.025f,false,&playerHealth,100);
            assert(marshal->state==ENEMY_IDLE && fabsf(marshal->attackCooldown-.60f)<.001f);
            UpdateEnemies(&group,&map,player,.49f,false,&playerHealth,100);
            assert(marshal->state!=ENEMY_ATTACK);
        }
        ClearEnemyAttacks(&group);
    }
    marshal->position = (Vector2){835,500};
    marshal->state = ENEMY_IDLE;
    marshal->attackCooldown = 0;
    marshal->attackBurst = 0;
    marshal->bossAttack = IGRIS_MELEE;
    int aimingHealth = 100;
    UpdateEnemies(&group,&map,player,.01f,false,&aimingHealth,100);
    assert(marshal->bossAttack == IGRIS_ECLIPSE);
    Vector2 running = player;
    for (int i = 0; i < 8; i++) {
        running.y += 5;
        UpdateEnemies(&group,&map,running,.05f,false,&aimingHealth,100);
    }
    Vector2 locked = marshal->attackTarget;
    assert(locked.y > running.y+15);
    running.y -= 10;
    UpdateEnemies(&group,&map,running,.05f,false,&aimingHealth,100);
    assert(Vector2Distance(marshal->attackTarget,locked)<.01f);
    marshal->state = ENEMY_IDLE;
    marshal->attackCooldown = 0;
    marshal->attackBurst = 1;
    marshal->bossAttack = IGRIS_ECLIPSE;
    marshal->position = (Vector2){player.x-200,player.y};
    int choiceHealth = 100;
    UpdateEnemies(&group,&map,player,.01f,false,&choiceHealth,100);
    assert(marshal->bossAttack == IGRIS_ERUPTION); /* Evita repetir o cone na mesma faixa. */
    marshal->position = (Vector2){835,500};
    marshal->igrisFinalForm = true;
    marshal->state = ENEMY_IDLE;
    marshal->attackCooldown = 0;
    marshal->attackBurst = 1;
    group.playerHitCooldown = 0;
    int playerHealth = 100;
    UpdateEnemies(&group,&map,player,.01f,false,&playerHealth,100);
    assert(marshal->state == ENEMY_ATTACK && marshal->facing == 3);
    Vector2 origin = marshal->position;
    UpdateEnemies(&group,&map,player,.73f,false,&playerHealth,100);
    assert(marshal->position.x > origin.x+150 && fabsf(marshal->position.y-origin.y)<.1f);
    assert(playerHealth == 65);
    Vector2 pivot = marshal->position;
    UpdateEnemies(&group,&map,player,.08f,false,&playerHealth,100);
    assert(Vector2Distance(pivot,marshal->position)<.1f);
    UpdateEnemies(&group,&map,player,.36f,false,&playerHealth,100);
    assert(fabsf(marshal->position.x-origin.x)<.1f && marshal->position.y>origin.y+100);
    pivot = marshal->position;
    UpdateEnemies(&group,&map,player,.09f,false,&playerHealth,100);
    assert(Vector2Distance(pivot,marshal->position)<.1f && marshal->facing == 3);
    UpdateEnemies(&group,&map,player,.27f,false,&playerHealth,100);
    assert(marshal->position.x>origin.x+150 && fabsf(marshal->position.y-pivot.y)<.1f);
    UpdateEnemies(&group,&map,player,.40f,false,&playerHealth,100);
    assert(marshal->state == ENEMY_IDLE && playerHealth == 65);
    assert(fabsf(marshal->attackCooldown - .14f) < .001f);
    for (int dodge = 0; dodge < 2; dodge++) {
        marshal->position = origin;
        marshal->attackCooldown = 0;
        marshal->attackBurst = dodge ? 2 : 0;
        group.playerHitCooldown = 0;
        Vector2 victim = {origin.x+250,origin.y};
        playerHealth = 100;
        UpdateEnemies(&group,&map,victim,.01f,false,&playerHealth,100);
        UpdateEnemies(&group,&map,victim,.57f,dodge,&playerHealth,100);
        assert(IgrisHoldingPlayer(&group) == !dodge);
        assert(playerHealth == (dodge ? 100 : 74));
        UpdateEnemies(&group,&map,victim,.20f,false,&playerHealth,100);
        ApplyIgrisGrab(&group,&map,&victim,.20f);
        assert(dodge ? victim.x == origin.x+250 : victim.x < origin.x+200);
        UpdateEnemies(&group,&map,victim,.6f,false,&playerHealth,100);
        assert(!IgrisHoldingPlayer(&group));
    }
    marshal->position = origin;
    marshal->attackCooldown = 0;
    marshal->attackBurst = 3;
    group.playerHitCooldown = 0;
    playerHealth = 100;
    UpdateEnemies(&group,&map,player,.01f,false,&playerHealth,100);
    assert(marshal->state == ENEMY_ATTACK);
    for (int i = 0; i < 3; i++) {
        assert(!IsMapActorBlocked(&map,marshal->igrisEchoFrom[i]));
        for (int j = 0; j < i; j++)
            assert(Vector2Distance(marshal->igrisEchoFrom[i],marshal->igrisEchoFrom[j]) >= 94);
    }
    UpdateEnemies(&group,&map,player,.80f,false,&playerHealth,100);
    assert(playerHealth == 100);
    UpdateEnemies(&group,&map,player,.39f,false,&playerHealth,100);
    assert(playerHealth == 69 && marshal->igrisEchoHits == 1u);
    UpdateEnemies(&group,&map,player,.65f,false,&playerHealth,100);
    assert(playerHealth == 38 && marshal->igrisEchoHits == 3u);
    UpdateEnemies(&group,&map,player,.60f,true,&playerHealth,100);
    assert(playerHealth == 38 && marshal->igrisEchoHits == 3u);
    UpdateEnemies(&group,&map,player,.5f,false,&playerHealth,100);
    assert(marshal->state == ENEMY_IDLE);
    marshal->position = origin;
    marshal->attackCooldown = 0;
    marshal->attackBurst = 3;
    UpdateEnemies(&group,&map,player,.01f,true,&playerHealth,100);
    UpdateEnemies(&group,&map,player,.50f,true,&playerHealth,100);
    Vector2 firstAim = marshal->igrisEchoTo[0];
    Vector2 secondAim = marshal->igrisEchoTo[1];
    Vector2 moved = {player.x,player.y+100};
    UpdateEnemies(&group,&map,moved,.52f,true,&playerHealth,100);
    assert(Vector2Distance(firstAim,marshal->igrisEchoTo[0]) < .01f);
    assert(Vector2Distance(secondAim,marshal->igrisEchoTo[1]) > 20);
    secondAim = marshal->igrisEchoTo[1];
    UpdateEnemies(&group,&map,player,.20f,true,&playerHealth,100);
    assert(Vector2Distance(secondAim,marshal->igrisEchoTo[1]) < .01f);
}

static int CountNecromancerShots(const EnemyGroup *group) {
    int count=0;
    for(int i=0;i<NECROMANCER_SHOTS;i++)
        count+=group->necromancer.shots[i].life>0;
    return count;
}

static void CheckNecromancerPressure(void) {
    Clearing map={.necromancerArena=true,.background={.width=1672,.height=941},
                  .playableBounds={0,0,1672,941}};
    EnemyGroup group={0};
    Vector2 player={836,680};
    int health=100;
    StartNecromancer(&group,1);
    Enemy *boss=&group.list[0];
    boss->state=ENEMY_IDLE;
    boss->attackCooldown=0;
    UpdateNecromancer(&group,&map,player,.01f,false,&health);
    UpdateNecromancer(&group,&map,player,.48f,false,&health);
    assert(CountNecromancerShots(&group)==5);

    StartNecromancer(&group,1);
    boss->state=ENEMY_IDLE;
    boss->attackCooldown=0;
    group.necromancer.rotation=2;
    UpdateNecromancer(&group,&map,player,.01f,false,&health);
    UpdateNecromancer(&group,&map,player,.69f,false,&health);
    Enemy *wraith=&group.list[1];
    assert(wraith->necromancerWraith && wraith->health==230 && wraith->maxHealth==230);
    assert(CountNecromancerShots(&group)==0);
    UpdateNecromancer(&group,&map,player,.99f,true,&health);
    assert(CountNecromancerShots(&group)==0);
    UpdateNecromancer(&group,&map,player,.02f,true,&health);
    assert(CountNecromancerShots(&group)==2);
    DamageEnemy(wraith,100);
    assert(wraith->health==130 && EnemyCanBeHit(wraith));
    DamageEnemy(wraith,130);
    assert(wraith->health==0 && !EnemyCanBeHit(wraith));
    DamageNecromancer(boss,boss->maxHealth);
    UpdateNecromancer(&group,&map,player,.01f,false,&health);
    assert(group.list[1].state==ENEMY_GONE && group.list[2].state==ENEMY_GONE);

    StartNecromancer(&group,1);
    boss->state=ENEMY_IDLE;
    boss->necromancerForm=3;
    boss->position=(Vector2){836,550};
    boss->attackCooldown=0;
    group.necromancer.automaticClock=.5f;
    UpdateNecromancer(&group,&map,player,.13f,true,&health);
    for(int i=0;i<5;i++) UpdateNecromancer(&group,&map,player,.08f,true,&health);
    assert(boss->state==ENEMY_ATTACK);
    assert(group.necromancer.automaticVolley==1);
    assert(CountNecromancerShots(&group)==7);
}

static void StartNecromancerAuditSkill(EnemyGroup *group,int form,int rotation) {
    StartNecromancer(group,1);
    Enemy *boss=&group->list[0];
    boss->state=ENEMY_IDLE;
    boss->attackCooldown=0;
    boss->necromancerForm=form;
    boss->position=(Vector2){836,420};
    group->necromancer.rotation=rotation;
}

static void CheckNecromancerCombos(void) {
    Clearing map={.necromancerArena=true,.background={.width=1672,.height=941},
                  .playableBounds={0,0,1672,941}};
    EnemyGroup group={0};
    Vector2 target={836,670};
    int health=100;
    StartNecromancerAuditSkill(&group,1,1);
    UpdateNecromancer(&group,&map,target,.01f,false,&health);
    UpdateNecromancer(&group,&map,target,.45f,false,&health);
    assert(health==100);
    UpdateNecromancer(&group,&map,target,.02f,false,&health);
    assert(health==66);

    health=100;
    StartNecromancerAuditSkill(&group,1,3);
    UpdateNecromancer(&group,&map,target,.01f,false,&health);
    UpdateNecromancer(&group,&map,target,.65f,false,&health);
    assert(CountNecromancerShots(&group)==0);
    UpdateNecromancer(&group,&map,target,.04f,false,&health);
    assert(group.necromancer.shots[0].life>0 && group.necromancer.shots[0].sword);

    StartNecromancerAuditSkill(&group,2,4);
    UpdateNecromancer(&group,&map,target,.01f,false,&health);
    UpdateNecromancer(&group,&map,target,1.03f,false,&health);
    Vector2 player;
    const Vector2 escapeSteps[]={{155,0},{-155,0},{0,155},{0,-155},
        {120,120},{120,-120},{-120,120},{-120,-120}};
    for(int i=0;i<8;i++) {
        player=target;
        MoveNecromancerPlayer(&group,&map,&player,escapeSteps[i]);
        assert(hypotf(player.x-target.x,player.y-target.y)<116);
    }
    player=(Vector2){target.x,target.y+109};
    MoveNecromancerPlayer(&group,&map,&player,(Vector2){0,100});
    assert(player.y<target.y+116);
    UpdateNecromancer(&group,&map,target,.53f,false,&health);
    assert(health==18);
    UpdateNecromancer(&group,&map,target,.70f,true,&health);
    player=target;
    MoveNecromancerPlayer(&group,&map,&player,(Vector2){155,0});
    assert(player.x>target.x+145);

    health=100;
    StartNecromancerAuditSkill(&group,2,3);
    group.list[1]=(Enemy){.necromancerWraith=true,.health=230,.maxHealth=230,
                         .state=ENEMY_IDLE,.position=target,.attackCooldown=.1f};
    UpdateNecromancer(&group,&map,target,.01f,false,&health);
    UpdateNecromancer(&group,&map,target,.85f,false,&health);
    assert(CountNecromancerShots(&group)==0);
    DamageEnemy(&group.list[1],230);
    UpdateNecromancer(&group,&map,target,.34f,false,&health);
    assert(health==100);

    StartNecromancerAuditSkill(&group,3,2);
    player=target;
    UpdateNecromancer(&group,&map,player,.01f,false,&health);
    for(int i=0;i<65;i++) {
        UpdateNecromancer(&group,&map,player,.016f,false,&health);
        ApplyNecromancerPull(&group,&map,&player);
    }
    assert(player.y<target.y-120 && health==100 && group.necromancer.automaticVolley==0);
    player.x+=250;
    UpdateNecromancer(&group,&map,player,.8f,false,&health);
    assert(health==100);

    StartNecromancerAuditSkill(&group,3,2);
    UpdateNecromancer(&group,&map,target,.01f,false,&health);
    UpdateNecromancer(&group,&map,target,.83f,true,&health);
    player=target;
    ApplyNecromancerPull(&group,&map,&player);
    assert(player.y==target.y && health==100);

    StartNecromancerAuditSkill(&group,3,1);
    UpdateNecromancer(&group,&map,target,.01f,false,&health);
    Vector2 flanking={986,670};
    UpdateNecromancer(&group,&map,flanking,1.07f,true,&health);
    Vector2 landing=group.list[0].position;
    assert(landing.y>900 && health==100);
    Vector2 nearLanding={landing.x+120,landing.y};
    UpdateNecromancer(&group,&map,nearLanding,.31f,false,&health);
    assert(health==22);

    DamageNecromancer(&group.list[0],group.list[0].maxHealth);
    UpdateNecromancer(&group,&map,target,.85f,false,&health);
    assert(group.list[0].state==ENEMY_DIE && !EnemyCanBeHit(&group.list[0]));
    UpdateNecromancer(&group,&map,target,.32f,false,&health);
    assert(group.list[0].state==ENEMY_FALLEN);
    assert(group.list[0].position.x==landing.x && group.list[0].position.y==landing.y);

}

int main(void) {
    CheckCoveiroWalk();
    CheckCoveiroChoices();
    CheckNecromancerPressure();
    CheckNecromancerCombos();
    CheckTemplarBalance();
    CheckBossPrediction();
    const int expected[] = {20, 25, 45, 65, 100};
    for (int type=0; type<5; type++) {
        assert(EnemyOptionalDropChance((EnemyType)type) == expected[type]);
        int successes = 0;
        for (int roll=0; roll<10000; roll++)
            successes += EnemyShouldDrop((EnemyType)type, roll);
        assert(successes == expected[type]*100);
    }
    assert(!EnemyShouldDrop((EnemyType)99, 0));
    assert(!EnemyShouldDrop(ENEMY_BOSS, -1));
    assert(!EnemyShouldDrop(ENEMY_BOSS, 10000));

    int drops = 0;
    for (int i=0; i<10000; i++) {
        Enemy enemy;
        memset(&enemy, 0, sizeof(enemy));
        enemy.type = ENEMY_COMMON_MELEE;
        enemy.health = 1;
        DamageEnemy(&enemy, 1);
        assert(enemy.state == ENEMY_DIE && enemy.dropRolled);
        bool firstDrop = enemy.pickupAvailable;
        DamageEnemy(&enemy, 1);
        assert(enemy.pickupAvailable == firstDrop);
        drops += firstDrop;
    }
    assert(drops > 1700 && drops < 2300);

    Enemy enemy = {0};
    enemy.state = ENEMY_DIE;
    enemy.stateTime = 0.85f;
    assert(EnemyAnimationFrame(&enemy) == 4);
    enemy.state = ENEMY_ATTACK;
    enemy.stateTime = 0.32f;
    assert(EnemyAnimationFrame(&enemy) == 2);

    Clearing map={0};
    map.background.width=1670; map.background.height=942;
    map.playableBounds=(Rectangle){0,0,1670,942};
    EnemyGroup group={0};
    ResetEnemies(&group,&map);
    assert(EnemiesRemaining(&group)==26); /* Clareira, onda 1 */
    CheckGuardianAttacks(&map, &group);
    CheckGuardianMelee(&map, &group);
    CheckGuardianClosePressure(&map, &group);
    CheckGuardianDodge(&map, &group);
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    Enemy *melee = &group.list[0];
    *melee = (Enemy){.type=ENEMY_COMMON_MELEE,.state=ENEMY_IDLE,
        .position={400,400},.health=45,.maxHealth=45,.power=1};
    Vector2 meleeTarget = {460,400};
    int meleeTargetHealth = 100;
    UpdateEnemies(&group,&map,meleeTarget,.01f,false,&meleeTargetHealth,100);
    assert(melee->state==ENEMY_ATTACK && melee->facing==3);
    UpdateEnemies(&group,&map,meleeTarget,.30f,false,&meleeTargetHealth,100);
    assert(meleeTargetHealth==100 && !melee->attackConnected);
    UpdateEnemies(&group,&map,meleeTarget,.02f,false,&meleeTargetHealth,100);
    assert(meleeTargetHealth==86 && melee->attackConnected);
    group.playerHitCooldown=0;
    UpdateEnemies(&group,&map,meleeTarget,.30f,false,&meleeTargetHealth,100);
    assert(meleeTargetHealth==86 && melee->state==ENEMY_IDLE);
    const Vector2 archerTargets[] = {{400,600},{400,200},{200,400},{600,400}};
    for (int direction=0;direction<4;direction++) {
        ClearEnemyAttacks(&group);
        Enemy *archer = &group.list[0];
        *archer = (Enemy){.type=ENEMY_COMMON_RANGED,.state=ENEMY_IDLE,
            .position={400,400},.health=45,.maxHealth=45,.power=1};
        int archerTargetHealth = 100;
        Vector2 archerTarget = archerTargets[direction];
        UpdateEnemies(&group,&map,archerTarget,.01f,false,&archerTargetHealth,100);
        assert(archer->state==ENEMY_ATTACK && archer->facing==direction);
        UpdateEnemies(&group,&map,archerTarget,.31f,false,&archerTargetHealth,100);
        assert(!archer->attackConnected && group.projectiles[0].remaining==0);
        UpdateEnemies(&group,&map,archerTarget,.02f,false,&archerTargetHealth,100);
        assert(archer->attackConnected && group.projectiles[0].remaining>0);
        Vector2 shot = group.projectiles[0].direction;
        assert((direction==0 ? shot.y : direction==1 ? -shot.y : direction==2 ? -shot.x : shot.x)>.99f);
        UpdateEnemies(&group,&map,archerTarget,.05f,false,&archerTargetHealth,100);
        assert(group.projectiles[1].remaining==0);
    }
    map.cemetery=true;
    ResetEnemies(&group,&map);
    assert(EnemiesRemaining(&group)==32);
    assert(group.list[3].type==ENEMY_COMMON_RANGED);
    int health=100;
    StartArenaBoss(&group,&map,(Vector2){800,400},1);
    group.list[0].position=(Vector2){800,600};
    group.list[0].state=ENEMY_IDLE;
    group.list[0].attackCooldown=0;
    group.list[0].bossAttack=COVEIRO_SHOVEL;
    Vector2 target={800,400};
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(group.list[0].state==ENEMY_ATTACK && group.list[0].bossAttack==0);
    float attackTime=group.list[0].stateTime;
    DamageEnemy(&group.list[0],25);
    assert(group.list[0].state==ENEMY_ATTACK && group.list[0].stateTime==attackTime);
    group.list[0].stateTime=.59f;
    UpdateEnemies(&group,&map,target,.02f,true,&health,100);
    assert(health==100 && !group.list[0].attackConnected); /* Warning still active. */
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==62);
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==62); /* One hit while immunity is active. */
    group.playerHitCooldown=0;
    group.list[0].power=2;
    group.list[0].attackConnected=false;
    health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==24); /* Corruption power must affect the root attack. */
    Enemy *coveiro = &group.list[0];
    DamageEnemy(coveiro, coveiro->health);
    assert(coveiro->guardianPhase == GUARDIAN_PHASE_TRANSITION && coveiro->health == coveiro->maxHealth/2);
    UpdateEnemies(&group, &map, target, 1.0f, true, &health, 100);
    UpdateEnemies(&group, &map, target, .25f, true, &health, 100);
    assert(coveiro->guardianPhase == GUARDIAN_PHASE_TWO && coveiro->bossAttack == COVEIRO_CHARGE);
    coveiro->bossAttack = COVEIRO_SHOVEL;
    coveiro->stateTime = .60f;
    UpdateEnemies(&group, &map, target, .03f, true, &health, 100);
    assert(group.cemeteryProjectiles[2].active && !group.cemeteryProjectiles[3].active);
    assert(group.cemeteryProjectiles[0].damage == 91); /* The projectile keeps corruption power. */
    assert(fabsf(group.cemeteryProjectiles[0].velocity.x) +
           fabsf(group.cemeteryProjectiles[0].velocity.y) >= 739);
    ClearEnemyAttacks(&group);
    coveiro->state=ENEMY_IDLE;coveiro->attackCooldown=10;
    coveiro->position=(Vector2){700,400};
    coveiro->guardianMeleeCooldown=0;
    coveiro->power=1;
    group.playerHitCooldown=0;health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(coveiro->state==ENEMY_ATTACK && coveiro->guardianMelee && coveiro->facing==3);
    assert(CemeteryAnimationPose(coveiro).atlas==CEM_BOSS_MELEE && CemeteryAnimationPose(coveiro).frame==0);
    UpdateEnemies(&group,&map,target,.28f,false,&health,100);
    assert(health==100 && !coveiro->attackConnected);
    UpdateEnemies(&group,&map,target,.03f,false,&health,100);
    assert(health==67 && coveiro->attackConnected && CemeteryAnimationPose(coveiro).frame==4);
    UpdateEnemies(&group,&map,target,.45f,false,&health,100);
    assert(coveiro->state==ENEMY_IDLE && !coveiro->guardianMelee && coveiro->guardianMeleeCooldown>0);
    coveiro->position=(Vector2){800,600};
    group.playerHitCooldown=0;health=100;
    group.cemeteryProjectiles[0]=(CemeteryProjectile){.active=true,
        .position={target.x-40,target.y+35},.velocity={620,0},.life=2,.damage=38};
    UpdateEnemies(&group,&map,target,.05f,false,&health,100);
    assert(health==62 && group.cemeteryProjectiles[0].hitPlayer);
    group.playerHitCooldown=0;
    group.cemeteryProjectiles[0].position=target;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==62); /* Enlarged blade still hits only once per leg. */
    DamageEnemy(coveiro, coveiro->health);
    UpdateEnemies(&group, &map, target, .9f, true, &health, 100);
    assert(coveiro->state == ENEMY_FALLEN && !group.cemeteryProjectiles[0].active);
    StartEnemyWave(&group,&map,1,target,1);
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    group.list[0]=(Enemy){.position={500,400},
        .type=ENEMY_COMMON_RANGED,.state=ENEMY_ATTACK,.stateTime=.54f,
        .power=2,.attackDirection={1,0},.health=60,.maxHealth=60};
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(group.projectiles[0].remaining>0 && group.projectiles[0].damage==24);
    group.projectiles[0].position=(Vector2){target.x,target.y-35};
    group.playerHitCooldown=0;health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==76);
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    group.list[0]=(Enemy){.position={500,400},
        .type=ENEMY_COMMON_MELEE,.state=ENEMY_IDLE,.speed=20,.power=1,
        .health=60,.maxHealth=60,.attackCooldown=1};
    UpdateEnemies(&group,&map,target,.1f,false,&health,100);
    assert(group.list[0].position.x>501.9f && group.list[0].position.x<502.1f);
    map.obstacles[0]=(Rectangle){100,100,40,40};map.obstacleCount=1;
    assert(IsClearingBlocked(&map,(Vector2){105,105}));
    assert(!IsClearingBlocked(&map,(Vector2){200,200}));
    assert(!MapHasClearPath(&map,(Vector2){90,120},(Vector2){150,120}));
    assert(MapHasClearPath(&map,(Vector2){90,170},(Vector2){150,170}));
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    group.list[0]=(Enemy){.position={90,120},
        .type=ENEMY_COMMON_MELEE,.state=ENEMY_ATTACK,.stateTime=.31f,
        .power=1,.health=60,.maxHealth=60};
    group.playerHitCooldown=0;health=100;
    UpdateEnemies(&group,&map,(Vector2){150,120},.02f,false,&health,100);
    assert(health==100); /* Within melee range, but separated by stone. */
    Vector2 embedded={110,110};
    MoveMapActor(&map,&embedded,(Vector2){0,0});
    assert(!IsMapActorBlocked(&map,embedded));
    Vector2 edge={8,200};
    for (int i=0;i<20;i++) MoveMapActor(&map,&edge,(Vector2){-5,3});
    assert(!IsMapActorBlocked(&map,edge));
    Vector2 obstacleEdge={90,110};
    for (int i=0;i<20;i++) MoveMapActor(&map,&obstacleEdge,(Vector2){3,0});
    assert(!IsMapActorBlocked(&map,obstacleEdge));
    assert(obstacleEdge.x>90 || obstacleEdge.y!=110);
    Clearing outlined={0};
    outlined.background.width=1672;outlined.background.height=941;
    ConfigureClearingBoundary(&outlined);
    assert(outlined.spawn.x==836 && outlined.spawn.y==470.5f);
    assert(!IsMapActorBlocked(&outlined,outlined.spawn));
    assert(IsClearingBlocked(&outlined,(Vector2){10,470}));
    assert(IsClearingBlocked(&outlined,(Vector2){345,202}));
    assert(!IsMapActorBlocked(&outlined,(Vector2){790,921}));
    assert(IsClearingBlocked(&outlined,(Vector2){1340.0f/1413*1672,250.0f/804*941}));
    Vector2 walking=outlined.spawn;
    for (int i=0;i<440;i++) MoveMapActor(&outlined,&walking,(Vector2){0,1});
    assert(walking.y>900 && !IsMapActorBlocked(&outlined,walking));
    Clearing grave={0};grave.cemetery=true;
    grave.background.width=1670;grave.background.height=942;
    ConfigureCemeteryBoundary(&grave);
    assert(!IsMapActorBlocked(&grave,(Vector2){835,210}));
    assert(!IsMapActorBlocked(&grave,(Vector2){835,500}));
    assert(IsClearingBlocked(&grave,(Vector2){1450,260}));
    assert(IsClearingBlocked(&grave,(Vector2){300,560}));
    assert(!IsMapActorBlocked(&grave,(Vector2){1560,490}));
    Enemy poseEnemy={.type=ENEMY_COMMON_MELEE,.state=ENEMY_WALK,.walkTime=.25f};
    const int zombieRows[]={0,2,3,1};
    for (int facing=0;facing<4;facing++) {
        poseEnemy.facing=facing;
        poseEnemy.state=ENEMY_WALK;
        CemeteryPose walk=CemeteryAnimationPose(&poseEnemy);
        assert(walk.atlas==CEM_ZOMBIE_WALK && walk.row==zombieRows[facing] && !walk.flip);
        poseEnemy.state=ENEMY_IDLE;
        assert(CemeteryAnimationPose(&poseEnemy).atlas==CEM_ZOMBIE_IDLE);
        poseEnemy.state=ENEMY_ATTACK;
        poseEnemy.stateTime=CEM_ZOMBIE_HIT_TIME;
        CemeteryPose strike=CemeteryAnimationPose(&poseEnemy);
        assert(strike.atlas==CEM_ZOMBIE_ATTACK && strike.frame==4 && strike.row==zombieRows[facing]);
    }
    poseEnemy.type=ENEMY_COMMON_RANGED;
    for (int facing=0;facing<4;facing++) {
        poseEnemy.facing=facing;
        poseEnemy.state=ENEMY_IDLE;
        CemeteryPose idle=CemeteryAnimationPose(&poseEnemy);
        assert(idle.atlas==CEM_LANTERN_IDLE && idle.columns==4 && idle.row==zombieRows[facing] && !idle.flip);
        poseEnemy.state=ENEMY_WALK;
        CemeteryPose walk=CemeteryAnimationPose(&poseEnemy);
        assert(walk.atlas==CEM_LANTERN_WALK && walk.columns==6 && walk.row==zombieRows[facing]);
        poseEnemy.state=ENEMY_ATTACK;
        poseEnemy.stateTime=CEM_LANTERN_RELEASE_TIME-.01f;
        assert(CemeteryAnimationPose(&poseEnemy).frame==3);
        poseEnemy.stateTime=CEM_LANTERN_RELEASE_TIME;
        CemeteryPose release=CemeteryAnimationPose(&poseEnemy);
        assert(release.atlas==CEM_LANTERN_ATTACK && release.frame==4 && release.row==zombieRows[facing]);
        poseEnemy.stateTime=.9f;
        assert(CemeteryAnimationPose(&poseEnemy).frame==5);
    }
    poseEnemy.type=ENEMY_SPECIAL;
    for (int facing=0;facing<4;facing++) {
        poseEnemy.facing=facing;
        poseEnemy.state=ENEMY_IDLE;
        CemeteryPose idle=CemeteryAnimationPose(&poseEnemy);
        assert(idle.atlas==CEM_SPECIAL_IDLE && idle.columns==4 && idle.row==zombieRows[facing] && !idle.flip);
        poseEnemy.state=ENEMY_WALK;
        CemeteryPose walk=CemeteryAnimationPose(&poseEnemy);
        assert(walk.atlas==CEM_SPECIAL_WALK && walk.columns==6 && walk.row==zombieRows[facing] && !walk.flip);
        poseEnemy.state=ENEMY_ATTACK;
        poseEnemy.stateTime=CEM_SPECIAL_IMPACT_TIME;
        CemeteryPose impact=CemeteryAnimationPose(&poseEnemy);
        assert(impact.atlas==CEM_SPECIAL_ATTACK && impact.frame==3 && impact.row==zombieRows[facing] && !impact.flip);
    }
    poseEnemy.type=ENEMY_BOSS;poseEnemy.state=ENEMY_ATTACK;
    for(int attack=0;attack<3;attack++) for(int face=0;face<4;face++) for(int f=0;f<220;f++) {
        poseEnemy.bossAttack=attack;poseEnemy.facing=face;poseEnemy.stateTime=f*.01f;
        CemeteryPose pose=CemeteryAnimationPose(&poseEnemy);
        assert(pose.frame>=0 && pose.frame<pose.columns && pose.row>=0 && pose.row<4);
        assert(!pose.flip && pose.row==((int[]){0,3,1,2})[face] && pose.columns==8);
    }
    const EnemyState coveiroStates[] = {ENEMY_SPAWNING, ENEMY_IDLE, ENEMY_WALK,
                                        ENEMY_HURT, ENEMY_DIE, ENEMY_FALLEN};
    const int coveiroAtlases[] = {CEM_BOSS_EMERGE, CEM_BOSS_IDLE, CEM_BOSS_WALK,
                                 CEM_BOSS_IDLE, CEM_BOSS_FALL, CEM_BOSS_FALL};
    for (int i=0; i<6; i++) for (int face=0; face<4; face++) {
        poseEnemy.state=coveiroStates[i];
        poseEnemy.facing=face;
        poseEnemy.stateTime=.5f;
        poseEnemy.walkTime=.25f;
        CemeteryPose pose=CemeteryAnimationPose(&poseEnemy);
        assert(pose.atlas==coveiroAtlases[i] && pose.columns==8 && pose.rows==4);
        assert(pose.row==((int[]){0,3,1,2})[face] && !pose.flip);
        assert(pose.frame>=0 && pose.frame<pose.columns);
        if (poseEnemy.state==ENEMY_FALLEN) assert(pose.frame==7);
    }
    Clearing arena=map;arena.cemetery=false;
    /* Clareira: 2 ondas de 14/20, vida +10% e dano +15% sobre a base. */
    for (int wave=1;wave<=CLEARING_WAVES;wave++) {
        StartEnemyWave(&group,&arena,wave,target,1);
        assert(EnemiesRemaining(&group)==26+(wave-1)*4);
        for (int i=0;i<ENEMY_COUNT;i++) {
            const Enemy *enemy=&group.list[i];
            if (enemy->state==ENEMY_GONE) continue;
            int base=(int)((enemy->type==ENEMY_SPECIAL ? 100 : 45)*(1.0f+(wave-1)*.12f)*1.0f);
            assert(enemy->maxHealth==((int)ceilf(ceilf(base*1.10f)*.90f)+1)/2);
            assert(fabsf(enemy->damageScale-1.15f)<.001f);
            assert(fabsf(enemy->power-(1.0f+(wave-1)*.09f))<.001f);
        }
    }
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    group.list[0]=(Enemy){.position={500,400},.type=ENEMY_COMMON_RANGED,.state=ENEMY_ATTACK,
        .stateTime=.54f,.power=1,.damageScale=1.15f,.attackDirection={1,0},.health=60,.maxHealth=60};
    UpdateEnemies(&group,&arena,target,.02f,false,&health,100);
    assert(group.projectiles[0].remaining>0 && group.projectiles[0].damage==14); /* 12 x 1,15 = 13,8 */
    ClearEnemyAttacks(&group);
    /* Cemitério: mesma base por classe da Clareira, mais 90 PV e 30% de dano. */
    for (int wave=1;wave<=CEMETERY_WAVES;wave++) {
        StartEnemyWave(&group,&map,wave,target,1);
        int melee=0, ranged=0, specials=0;
        for (int i=0;i<ENEMY_COUNT;i++) {
            if (group.list[i].state==ENEMY_GONE) continue;
            const Enemy *enemy=&group.list[i];
            int base=(int)((enemy->type==ENEMY_SPECIAL ? 100 : 45)*(1.0f+(wave-1)*.12f));
            assert(enemy->maxHealth==((int)ceilf((ceilf(base*1.10f)+90)*.90f)+1)/2);
            assert(fabsf(enemy->damageScale-1.15f*1.30f)<.001f);
            assert(fabsf(enemy->power-(1.0f+(wave-1)*.09f))<.001f);
            if (group.list[i].type==ENEMY_COMMON_MELEE) melee++;
            if (group.list[i].type==ENEMY_COMMON_RANGED) ranged++;
            if (group.list[i].type==ENEMY_SPECIAL) specials++;
        }
        assert(melee>0 && ranged>0 && specials==2);
        assert(melee+ranged+specials==((int[]){32,36})[wave-1]);
    }
    for(int cemetery=0;cemetery<2;cemetery++) {
        const Clearing *waveMap=cemetery?&map:&arena;
        int limit=cemetery?20:18;
        int total=cemetery?32:26;
        StartEnemyWave(&group,waveMap,1,target,1);
        UpdateEnemies(&group,waveMap,target,.01f,true,&health,100);
        assert(group.list[limit-1].state==ENEMY_SPAWNING);
        assert(group.list[limit].state==ENEMY_QUEUED);
        group.list[0].state=ENEMY_GONE;
        UpdateEnemies(&group,waveMap,target,.01f,true,&health,100);
        assert(group.list[limit].state==ENEMY_SPAWNING);
        assert(group.list[limit+1].state==ENEMY_QUEUED);
        for(int i=0;i<limit+1;i++) group.list[i].state=ENEMY_GONE;
        UpdateEnemies(&group,waveMap,target,.01f,true,&health,100);
        int nearbyReserves=0;
        for(int i=limit+1;i<total;i++) {
            assert(group.list[i].state==ENEMY_SPAWNING);
            float distance=Vector2Distance(group.list[i].position,target);
            if(distance>=239.9f && distance<=380.1f) nearbyReserves++;
        }
        assert(nearbyReserves>=(cemetery?9:6));
        assert(EnemiesRemaining(&group)==total-limit-1);
    }
    StartArenaBoss(&group,&map,target,1);assert(group.list[0].maxHealth==2600);
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    ClearEnemyAttacks(&group);
    Enemy *special=&group.list[0];
    for (int scenario=0;scenario<3;scenario++) {
        *special=(Enemy){.position={target.x-145,target.y},.type=ENEMY_SPECIAL,.state=ENEMY_IDLE,
            .health=110,.maxHealth=110,.power=1};
        group.playerHitCooldown=0;
        health=100;
        UpdateEnemies(&group,&map,target,.01f,false,&health,100);
        assert(special->state==ENEMY_ATTACK && special->facing==3);
        UpdateEnemies(&group,&map,target,.20f,false,&health,100);
        assert(special->position.x==target.x-145);
        UpdateEnemies(&group,&map,target,CEM_SPECIAL_IMPACT_TIME-.21f,false,&health,100);
        assert(health==100 && !special->attackConnected && CemeteryAnimationPose(special).frame==2);
        assert(special->position.x>target.x-76);
        Vector2 impactTarget=scenario==2?(Vector2){target.x+100,target.y}:target;
        UpdateEnemies(&group,&map,impactTarget,.02f,scenario==1,&health,100);
        assert(health==(scenario==0?82:100) && special->attackConnected);
        assert(CemeteryAnimationPose(special).frame==3);
        group.playerHitCooldown=0;
        UpdateEnemies(&group,&map,target,.10f,false,&health,100);
        assert(health==(scenario==0?82:100));
    }
    DamageEnemy(special,special->health);
    UpdateEnemies(&group,&map,target,.9f,false,&health,100);
    assert(special->state==ENEMY_FALLEN && EnemiesRemaining(&group)==1);
    *special=(Enemy){.state=ENEMY_GONE};
    Enemy *lantern=&group.list[0];
    *lantern=(Enemy){.position={target.x-120,target.y},.type=ENEMY_COMMON_RANGED,
        .state=ENEMY_ATTACK,.health=50,.maxHealth=50,.power=1,.facing=3,
        .attackDirection={1,0},.stateTime=CEM_LANTERN_RELEASE_TIME-.025f};
    health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(group.projectiles[0].remaining==0 && !lantern->attackConnected);
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(group.projectiles[0].remaining>0 && lantern->attackConnected);
    assert(CemeteryAnimationPose(lantern).frame==4);
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(group.projectiles[1].remaining==0);
    for(int scenario=0;scenario<3;scenario++) {
        for(int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
        ClearEnemyAttacks(&group);group.cemetery=true;group.playerHitCooldown=scenario==2?.5f:0;
        group.projectiles[0]=(EnemyProjectile){{target.x-20,target.y-35},{1,0},1,20,false};
        health=100;UpdateEnemies(&group,&map,target,.05f,scenario==1,&health,100);
        assert(health==(scenario==0?80:100));
        assert(group.projectiles[0].remaining==0);
        assert(group.impacts[0].remaining>0);
    }
    CheckBonelandWaves();
    CheckBoneLordAttacks();
    return 0;
}
