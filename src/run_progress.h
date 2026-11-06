#ifndef RUN_PROGRESS_H
#define RUN_PROGRESS_H

#define CLEARING_WAVES 4
#define CLEARING_BOSS_STAGE 5
#define CLEARING_COMPLETE_STAGE 6

typedef struct RunProgress {
    int map; /* 0: clareira, 1: cemiterio */
    int weapon;
    int stage; /* 1..4: waves, 5: boss, 6: clearing sealed */
    int health, maxHealth;
    float damage, speed, bossPower;
    int weaponLevel[3];
    int strongWeapon;
} RunProgress;

static inline RunProgress NewRunProgress(int weapon) {
    return (RunProgress){.weapon=weapon, .stage=1, .health=100, .maxHealth=100,
        .damage=1, .speed=240, .bossPower=1, .strongWeapon=-1};
}
#endif
