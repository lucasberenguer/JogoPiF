#ifndef MENU_H
#define MENU_H
#include "raylib.h"
#include "run_progress.h"
#include "menu_video.h"
typedef enum MenuAction { MENU_NONE, MENU_START, MENU_CONTINUE, MENU_QUIT, MENU_TEST_CEMETERY } MenuAction;
typedef enum PauseAction { PAUSE_NONE, PAUSE_RESUME, PAUSE_MAIN_MENU } PauseAction;
typedef enum PausePage { PAUSE_OPTIONS, PAUSE_SETTINGS, PAUSE_CONTROLS } PausePage;
typedef struct PauseMenu {
    Texture2D background, frame, labels;
    Shader shader;
    int timeLocation, layerLocation;
    float time, buttonFocus[4];
    PausePage page;
    int selected;
} PauseMenu;
typedef struct Menu {
    Texture2D background;
    Texture2D moon, seal, necromancer, fog, fogTexture;
    Texture2D hero[10];
    Texture2D buttons[4];
    PauseMenu pause;
    MenuVideo *video;
    Texture2D videoFrame;
    double videoInterval, videoElapsed;
    float effectTime;
    float buttonFocus[4];
    int page;
    int selected;
    bool hasSave;
} Menu;
bool InitMenu(Menu *menu);
void UnloadMenu(Menu *menu);
MenuAction UpdateMenu(Menu *menu);
void DrawMenu(const Menu *menu);
PauseAction UpdatePauseMenu(PauseMenu *pause);
void DrawPauseMenu(const Menu *menu);
bool SaveMenuCheckpoint(int weapon);
int LoadMenuCheckpointWeapon(void);
bool SaveClearingCheckpoint(const RunProgress *progress);
bool LoadClearingCheckpoint(RunProgress *progress);
#endif
