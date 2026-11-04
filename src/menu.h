#ifndef MENU_H
#define MENU_H
#include "raylib.h"
typedef enum MenuAction { MENU_NONE, MENU_START, MENU_CONTINUE, MENU_QUIT } MenuAction;
typedef struct Menu {
    Texture2D background;
    Texture2D buttons[4];
    int page;
    int selected;
    bool hasSave;
} Menu;
bool InitMenu(Menu *menu);
void UnloadMenu(Menu *menu);
MenuAction UpdateMenu(Menu *menu);
void DrawMenu(const Menu *menu);
bool SaveMenuCheckpoint(int weapon);
int LoadMenuCheckpointWeapon(void);
#endif
