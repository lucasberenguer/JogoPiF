#ifndef WEAPON_SELECT_H
#define WEAPON_SELECT_H
#include "raylib.h"

typedef enum Weapon { WEAPON_GUN, WEAPON_BIBLE, WEAPON_KATANA, WEAPON_COUNT } Weapon;
typedef struct WeaponSelect {
    Texture2D frame;
    Texture2D cards[WEAPON_COUNT];
    int selected;
} WeaponSelect;
bool InitWeaponSelect(WeaponSelect *screen);
void UnloadWeaponSelect(WeaponSelect *screen);
int UpdateWeaponSelect(WeaponSelect *screen); /* -1: aguardando, -2: voltar */
void DrawWeaponSelect(const WeaponSelect *screen);
const char *WeaponName(Weapon weapon);
#endif
