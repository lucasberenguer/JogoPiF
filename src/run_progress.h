#ifndef RUN_PROGRESS_H
#define RUN_PROGRESS_H

#include <stdbool.h>

#define CLEARING_MAP 0
#define CEMETERY_MAP 1
#define VILLAGE_MAP 2
#define CLEARING_WAVES 3 /* Clareira do Selo; o chefe vem depois da última onda. */
#define CEMETERY_WAVES 4 /* Cemitério das Raízes. */
#define BONELAND_WAVES 3
#define CHAPEL_MAP 3
#define INVERTED_CHAPEL_MAP 5
#define BONELAND_MAP 6
#define TEMPLAR_FIELD_MAP 7
#define NECROMANCER_MAP 8
#define TEMPLAR_FIELD_WAVES 2
#define CLEARING_BOSS_STAGE 5
#define CLEARING_COMPLETE_STAGE 6
#define MAX_WEAPON_LEVEL 5
#define WEAPON_LEVEL_DAMAGE_BONUS 0.2f
#define EPIC_DAMAGE_STACK_BONUS 0.5f
#define BIBLE_L1_DAMAGE_BONUS 0.15f
#define GUN_L2_DAMAGE_BONUS 0.25f

/* Ondas normais de cada mapa antes do chefe (estágio CLEARING_BOSS_STAGE).
   Mapas sem ondas numeradas mantêm o valor do Cemitério, como antes da separação. */
static inline int CampaignWaveCount(int map) {
    if (map == CLEARING_MAP) return CLEARING_WAVES;
    if (map == BONELAND_MAP) return BONELAND_WAVES;
    if (map == TEMPLAR_FIELD_MAP) return TEMPLAR_FIELD_WAVES;
    return CEMETERY_WAVES;
}

typedef enum GuardianAftermathState {
    GUARDIAN_UNPURIFIED, GUARDIAN_LIBERATION_PENDING, GUARDIAN_ALTAR_READY,
    GUARDIAN_PASSAGE_PENDING, GUARDIAN_PASSAGE_COMPLETE
} GuardianAftermathState;

typedef enum VillageAftermath { VILLAGE_DUEL, VILLAGE_SCENE_PENDING, VILLAGE_SCENE_SEEN } VillageAftermath;

typedef struct RunProgress {
    int map;
    int weapon;
    int initialWeapon;
    int stage; /* 1..4: waves, 5: boss, 6: clearing sealed */
    int health, maxHealth;
    float damage, speed, bossPower;
    int weaponLevel[3];
    int strongWeapon; /* Legado: mantido apenas para leitura e migração de saves antigos. */
    int epicDamageStacks;
    bool guardianIntroSeen;
    bool angelSceneCompleted;
    int angelBlessingUses;
    int finalSealFailures;
    VillageAftermath villageAftermath;
    GuardianAftermathState guardianAftermath;
} RunProgress;

static inline void SetRunWeaponLevel(RunProgress *p, int level) {
    for (int i = 0; i < 3; i++) p->weaponLevel[i] = level;
}

static inline int WeaponBaseDamage(int weapon) {
    static const int baseDamages[3] = {5, 12, 17};
    return (weapon >= 0 && weapon < 3) ? baseDamages[weapon] : 0;
}

static inline RunProgress NewRunProgress(int weapon) {
    return (RunProgress){.weapon=weapon, .initialWeapon=weapon, .stage=1, .health=100, .maxHealth=100,
        .damage=1, .speed=204, .bossPower=1, .strongWeapon=-1, .epicDamageStacks=0};
}
#endif
