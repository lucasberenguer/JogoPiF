#ifndef MENU_H
#define MENU_H
#include "raylib.h"
#include "run_progress.h"
#include "game_audio.h"
#include "menu_scene.h"
#include "video_settings.h"
typedef enum MenuAction { MENU_NONE, MENU_START, MENU_CONTINUE, MENU_TEST_VILLA, MENU_TEST_CHAPEL, MENU_TEST_INVERTED_CHAPEL, MENU_QUIT } MenuAction;
typedef enum PauseAction { PAUSE_NONE, PAUSE_RESUME, PAUSE_MAIN_MENU } PauseAction;
typedef enum PausePage { PAUSE_OPTIONS, PAUSE_SETTINGS, PAUSE_CONTROLS } PausePage;
typedef struct PauseMenu {
    Texture2D background, frame, labels;
    Shader shader;
    int timeLocation, layerLocation, fogLocation;
    float time, buttonFocus[4];
    PausePage page;
    int selected;
} PauseMenu;
typedef struct Menu {
    Texture2D background, mainButtons;
    Texture2D buttonGlow;
    MenuScene *scene;
    float sceneTime, introTime, departureTime;
    bool starting;
    Texture2D fogTexture;
    Texture2D buttons[4];
    Texture2D creditPhotos[4];
    PauseMenu pause;
    GameAudio audio;
    int volumeFocus, volumeDrag;
    int settingsTab, videoFocus; /* aba 0: Áudio, 1: Vídeo */
    bool volumeChanged;
    int page;
    int selected;
    bool hasSave;
} Menu;
bool InitMenu(Menu *menu);
void UnloadMenu(Menu *menu);
MenuAction UpdateMenu(Menu *menu);
bool MenuControlsVisible(const Menu *menu);
void DrawMenu(const Menu *menu);
PauseAction UpdatePauseMenu(Menu *menu);
/* Tela cheia sem borda (janela do tamanho do monitor). Permite Alt+Tab em todas as plataformas. */
void ToggleGameFullscreen(void);
bool IsGameFullscreen(void);
void DrawPauseMenu(const Menu *menu);
bool SaveMenuCheckpoint(int weapon);
int LoadMenuCheckpointWeapon(void);
bool SaveClearingCheckpoint(const RunProgress *progress);
bool LoadClearingCheckpoint(RunProgress *progress);
#endif
