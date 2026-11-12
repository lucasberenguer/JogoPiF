#include "enemy.h"
#include <assert.h>
#include <math.h>
#include <string.h>

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

int main(void) {
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
    assert(EnemiesRemaining(&group)==7);
    CheckGuardianAttacks(&map, &group);
    CheckGuardianMelee(&map, &group);
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
    assert(meleeTargetHealth==88 && melee->attackConnected);
    group.playerHitCooldown=0;
    UpdateEnemies(&group,&map,meleeTarget,.30f,false,&meleeTargetHealth,100);
    assert(meleeTargetHealth==88 && melee->state==ENEMY_IDLE);
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
    assert(EnemiesRemaining(&group)==7);
    assert(group.list[3].type==ENEMY_COMMON_RANGED);
    int health=100;
    StartArenaBoss(&group,&map,(Vector2){800,400},1);
    group.list[0].position=(Vector2){800,600};
    group.list[0].state=ENEMY_IDLE;
    group.list[0].attackCooldown=0;
    Vector2 target={800,400};
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(group.list[0].state==ENEMY_ATTACK && group.list[0].bossAttack==0);
    float attackTime=group.list[0].stateTime;
    DamageEnemy(&group.list[0],25);
    assert(group.list[0].state==ENEMY_ATTACK && group.list[0].stateTime==attackTime);
    group.list[0].stateTime=.79f;
    UpdateEnemies(&group,&map,target,.02f,true,&health,100);
    assert(health==100); /* Dodge during active roots. */
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==56);
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==56); /* One hit while immunity is active. */
    group.playerHitCooldown=0;
    group.list[0].power=2;
    group.list[0].attackConnected=false;
    health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==12); /* Corruption power must affect the root attack. */
    Enemy *coveiro = &group.list[0];
    DamageEnemy(coveiro, coveiro->health);
    assert(coveiro->guardianPhase == GUARDIAN_PHASE_TRANSITION && coveiro->health == coveiro->maxHealth/2);
    UpdateEnemies(&group, &map, target, 1.0f, true, &health, 100);
    UpdateEnemies(&group, &map, target, .25f, true, &health, 100);
    assert(coveiro->guardianPhase == GUARDIAN_PHASE_TWO && coveiro->bossAttack == COVEIRO_CHARGE);
    coveiro->bossAttack = COVEIRO_SHOVEL;
    coveiro->stateTime = .79f;
    UpdateEnemies(&group, &map, target, .02f, true, &health, 100);
    assert(group.cemeteryProjectiles[2].active && !group.cemeteryProjectiles[3].active);
    assert(group.cemeteryProjectiles[0].damage == 76); /* The projectile keeps corruption power. */
    assert(fabsf(group.cemeteryProjectiles[0].velocity.x) +
           fabsf(group.cemeteryProjectiles[0].velocity.y) >= 739);
    ClearEnemyAttacks(&group);
    coveiro->state=ENEMY_IDLE;coveiro->attackCooldown=10;
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
    assert(group.projectiles[0].remaining>0 && group.projectiles[0].damage==20);
    group.projectiles[0].position=(Vector2){target.x,target.y-35};
    group.playerHitCooldown=0;health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==80);
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
    poseEnemy.facing=2;assert(CemeteryAnimationPose(&poseEnemy).flip);
    poseEnemy.facing=3;assert(!CemeteryAnimationPose(&poseEnemy).flip);
    poseEnemy.type=ENEMY_COMMON_RANGED;
    const int mageRows[]={0,1,3,2};
    for(int face=0;face<4;face++) {poseEnemy.facing=face;assert(CemeteryAnimationPose(&poseEnemy).row==mageRows[face]);}
    poseEnemy.type=ENEMY_BOSS;poseEnemy.state=ENEMY_ATTACK;
    for(int attack=0;attack<3;attack++) for(int face=0;face<4;face++) for(int f=0;f<220;f++) {
        poseEnemy.bossAttack=attack;poseEnemy.facing=face;poseEnemy.stateTime=f*.01f;
        CemeteryPose pose=CemeteryAnimationPose(&poseEnemy);
        assert(pose.frame>=0 && pose.frame<pose.columns && pose.row>=0 && pose.row<4);
        assert(pose.flip==(face==2));
    }
    Clearing arena=map;arena.cemetery=false;
    SetRandomSeed(321);StartEnemyWave(&group,&arena,1,target,1);
    int normalHealth[ENEMY_COUNT];
    for(int i=0;i<ENEMY_COUNT;i++) normalHealth[i]=group.list[i].maxHealth;
    SetRandomSeed(321);StartEnemyWave(&group,&map,1,target,1);
    for(int i=0;i<ENEMY_COUNT;i++) if(normalHealth[i]) assert(group.list[i].maxHealth==(int)ceilf(normalHealth[i]*1.10f));
    StartArenaBoss(&group,&map,target,1);assert(group.list[0].maxHealth==2420);
    for(int scenario=0;scenario<3;scenario++) {
        for(int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
        ClearEnemyAttacks(&group);group.cemetery=true;group.playerHitCooldown=scenario==2?.5f:0;
        group.projectiles[0]=(EnemyProjectile){{target.x-20,target.y-35},{1,0},1,20};
        health=100;UpdateEnemies(&group,&map,target,.05f,scenario==1,&health,100);
        assert(health==(scenario==0?80:100));
        assert(group.projectiles[0].remaining==0);
        assert(group.impacts[0].remaining>0);
    }
    return 0;
}
