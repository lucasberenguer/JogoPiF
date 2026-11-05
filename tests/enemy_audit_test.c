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
    for (int i=4;i<ENEMY_COUNT;i++) assert(group.list[i].state==ENEMY_GONE);
    map.cemetery=true;
    ResetEnemies(&group,&map);
    assert(group.list[4].type==ENEMY_COMMON_RANGED);
    assert(group.list[6].type==ENEMY_BOSS && group.list[6].health==480);
    assert(!EnemyCanBeHit(&group.list[6]));
    int health=100;
    for (int i=0;i<6;i++) group.list[i].state=ENEMY_GONE;
    group.list[6].attackCooldown=0;
    Vector2 target={800,400};
    UpdateEnemies(&group,&map,target,.01f,false,&health,100);
    assert(group.list[6].state==ENEMY_ATTACK && group.list[6].bossAttack==0);
    float attackTime=group.list[6].stateTime;
    DamageEnemy(&group.list[6],25);
    assert(group.list[6].state==ENEMY_ATTACK && group.list[6].stateTime==attackTime);
    group.list[6].stateTime=.79f;
    UpdateEnemies(&group,&map,target,.02f,true,&health,100);
    assert(health==100); /* Dodge during active roots. */
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==76);
    UpdateEnemies(&group,&map,target,.02f,false,&health,100);
    assert(health==76); /* One hit while immunity is active. */
    map.obstacles[0]=(Rectangle){100,100,40,40};map.obstacleCount=1;
    assert(IsClearingBlocked(&map,(Vector2){105,105}));
    assert(!IsClearingBlocked(&map,(Vector2){200,200}));
    return 0;
}
