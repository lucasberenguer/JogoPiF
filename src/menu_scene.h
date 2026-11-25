#ifndef MENU_SCENE_H
#define MENU_SCENE_H

#include <stdbool.h>
#include "raylib.h"

typedef struct MenuScene MenuScene;

MenuScene *LoadMenuScene(void);
void UnloadMenuScene(MenuScene *scene);
void DrawMenuScene(const MenuScene *scene, Rectangle canvas, Rectangle crop,
                   float time, float intro, float departure, bool reduced);

#endif
