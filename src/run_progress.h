#ifndef RUN_PROGRESS_H
#define RUN_PROGRESS_H

typedef struct RunProgress {
    int weapon;
    int stage; /* 1..5: waves, 6: boss, 7: clearing sealed */
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
