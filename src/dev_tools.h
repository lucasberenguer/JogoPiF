#ifndef DEV_TOOLS_H
#define DEV_TOOLS_H

#include "encounter.h"

typedef enum DevAction {
    DEV_NONE, DEV_STAGE, DEV_BOSS_ONE, DEV_BOSS_TWO, DEV_OPENING, DEV_GUARDIAN,
    DEV_SEAL, DEV_HEAL, DEV_CLEAR_STAGE, DEV_MENU, DEV_CLOSE
} DevAction;

typedef struct DevTools {
    int map, wave, weapon, weaponLevel, selected;
    bool open, invincible, collision;
    char status[160];
} DevTools;

DevAction UpdateDevTools(DevTools *tools, bool inPractice);
void DrawDevTools(const DevTools *tools, bool inPractice);
bool StartDevEncounter(const DevTools *tools, DevAction action, Encounter *encounter,
                       EnemyGroup *enemies, Clearing *map, Vector2 *player, int *health);

#endif
