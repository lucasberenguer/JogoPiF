#include "enemy.h"
#include <assert.h>
#include <string.h>

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
    assert(EnemiesRemaining(&group)==6);
    map.cemetery=true;
    ResetEnemies(&group,&map);
    assert(EnemiesRemaining(&group)==6);
    assert(group.list[3].type==ENEMY_COMMON_RANGED);
    int health=100;
    CemeteryStartBoss(&group,&map,(Vector2){800,400},1);
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
    assert(health==76);
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==76); /* One hit while immunity is active. */
    group.playerHitCooldown=0;
    group.list[0].power=2;
    health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==52); /* Corruption power must affect the root attack. */
    CemeteryStartWave(&group,&map,1,target,1);
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    group.list[0]=(Enemy){.position={500,400},.cemeteryUnit=true,
        .type=ENEMY_COMMON_RANGED,.state=ENEMY_ATTACK,.stateTime=.54f,
        .power=2,.attackDirection={1,0},.health=60,.maxHealth=60};
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(group.cemeteryProjectiles[0].active && group.cemeteryProjectiles[0].damage==20);
    group.cemeteryProjectiles[0].position=target;
    group.playerHitCooldown=0;health=100;
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(health==80);
    for (int i=0;i<ENEMY_COUNT;i++) group.list[i]=(Enemy){.state=ENEMY_GONE};
    group.list[0]=(Enemy){.position={500,400},.cemeteryUnit=true,
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
    group.list[0]=(Enemy){.position={90,120},.cemeteryUnit=true,
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
    return 0;
}
