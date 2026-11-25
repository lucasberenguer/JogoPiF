#include "game_input.h"
#include "game_text.h"
#include "menu.h"
#include "speedrun.h"
#include <stdio.h>
#include <math.h>
#include "rlgl.h"
#include <string.h>

enum { PAUSE_SCENE = 0, PAUSE_PLATE = 2, PAUSE_ICON = 3, PAUSE_FRAME = 4, MENU_BUTTON = 5 };

static const Color buttonText = {255, 226, 181, 255};

enum MainMenuButton { MAIN_START, MAIN_CONTINUE, MAIN_SETTINGS, MAIN_CREDITS, MAIN_QUIT, MAIN_BUTTON_COUNT };
static const float menuIntroDuration = 3.8f;
static const float menuDepartureDuration = 0.68f;

static const Rectangle mainButtonSprites[MAIN_BUTTON_COUNT] = {
    {409, 48, 720, 186},
    {409, 243, 720, 166},
    {409, 416, 720, 165},
    {409, 590, 720, 167},
    {409, 764, 720, 166}
};

static const char *SavePath(void) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/partida_pif.sav", GetApplicationDirectory());
    return path;
}

static Rectangle MenuButtonBounds(int i, float centerX, float centerY, float w) {
    float h = w * 0.30f;
    float gap = fmaxf(8.0f, h * 0.04f);
    float top = centerY - (4*h + 3*gap)/2;
    return (Rectangle){centerX - w/2, top + i*(h+gap), w, h};
}

static Rectangle MainMenuBackdropSource(Texture2D background) {
    // Frame the illustration around the title's sword while keeping its aspect ratio.
    float scale = 1635.0f / 1672.0f;
    float height = background.height * scale;
    return (Rectangle){0, (background.height - height) / 2, background.width * scale, height};
}

static Rectangle MainMenuCanvas(const Menu *m) {
    float width = (float)GetScreenWidth();
    float height = (float)GetScreenHeight();
    float scale = fminf(width / m->background.width, height / m->background.height);
    float artWidth = m->background.width * scale;
    float artHeight = m->background.height * scale;
    return (Rectangle){(width - artWidth) / 2, (height - artHeight) / 2, artWidth, artHeight};
}

static Rectangle MainButtonBounds(const Menu *m, int index) {
    Rectangle canvas = MainMenuCanvas(m);
    float top = canvas.y + canvas.height * 0.315f;
    float bottom = fminf(canvas.y + canvas.height * 0.96f, GetScreenHeight() - 70.0f);
    float rowHeight = fmaxf(1.0f, (bottom - top) / MAIN_BUTTON_COUNT);
    float width = fminf(canvas.width * 0.37f, 4.15f * rowHeight);
    return (Rectangle){(GetScreenWidth() - width) / 2, top + index * rowHeight, width, rowHeight};
}

static Rectangle TestButtonBounds(void) {
    return (Rectangle){GetScreenWidth() - 174.0f, GetScreenHeight() - 62.0f, 146.0f, 40.0f};
}

static Rectangle TestChapelButtonBounds(void) {
    return (Rectangle){GetScreenWidth() - 332.0f, GetScreenHeight() - 62.0f, 146.0f, 40.0f};
}

static Rectangle TestInvertedChapelButtonBounds(void) {
    return (Rectangle){GetScreenWidth() - 536.0f, GetScreenHeight() - 62.0f, 190.0f, 40.0f};
}

static void UpdateButtonFocus(float focus[4], int selected, float dt) {
    float blend = 1.0f - expf(-18.0f*dt);
    for (int i = 0; i < 4; i++) focus[i] += ((i == selected ? 1.0f : 0.0f) - focus[i])*blend;
}

static Color MenuButtonTint(Rectangle bounds, float focus, bool disabled) {
    if (disabled) return Fade(WHITE, .48f);
    float idleTint = 217.0f + 38.0f*focus;
    if (focus > .5f && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), bounds)) idleTint *= .89f;
    return (Color){(unsigned char)idleTint, (unsigned char)idleTint, (unsigned char)idleTint, 255};
}

static void DrawMenuPanel(Rectangle bounds) {
    DrawRectangleRounded(bounds, .08f, 12, (Color){31, 5, 20, 248});
    DrawRectangleRoundedLinesEx(bounds, .08f, 12, 2, (Color){163, 93, 46, 255});
}

static float SmoothRange(float start, float end, float value) {
    float t = fminf(1.0f, fmaxf(0.0f, (value-start)/(end-start)));
    return t*t*(3.0f - 2.0f*t);
}

static Texture2D LoadMainButtons(void) {
    Image atlas = LoadImage("assets/menu_principal_botoes.png");
    if (!atlas.data) return (Texture2D){0};
    for (int i = 0; i < MAIN_BUTTON_COUNT; i++) {
        Rectangle sprite = mainButtonSprites[i];
        if (sprite.x + sprite.width > atlas.width || sprite.y + sprite.height > atlas.height) {
            UnloadImage(atlas);
            return (Texture2D){0};
        }
    }

    Color *pixels = LoadImageColors(atlas);
    Texture2D texture = {0};
    if (pixels) {
        // Fade the soft shadows at each crop boundary so they blend into the artwork.
        for (int i = 0; i < MAIN_BUTTON_COUNT; i++) {
            Rectangle sprite = mainButtonSprites[i];
            for (int y = 0; y < (int)sprite.height; y++) {
                float vertical = SmoothRange(0, 18, fminf(y, sprite.height - 1 - y));
                Color *row = pixels + ((int)sprite.y + y) * atlas.width + (int)sprite.x;
                for (int x = 0; x < (int)sprite.width; x++) {
                    float horizontal = SmoothRange(0, 24, fminf(x, sprite.width - 1 - x));
                    float brightness = fmaxf(row[x].r, fmaxf(row[x].g, row[x].b));
                    float opacity = 0.42f + 0.58f * SmoothRange(6, 105, brightness);
                    row[x].a = (unsigned char)(row[x].a * vertical * horizontal * opacity);
                }
            }
        }
        Image prepared = {pixels, atlas.width, atlas.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        texture = LoadTextureFromImage(prepared);
        UnloadImageColors(pixels);
    }
    UnloadImage(atlas);
    return texture;
}

static Texture2D LoadPauseFrame(void) {
    Image source = LoadImage("assets/pausa_moldura.png");
    if (!source.data) return (Texture2D){0};
    Color *pixels = LoadImageColors(source);
    Texture2D texture = {0};
    if (pixels) {
        // Remova a cor do quadriculado antes de interpolar. O shader recebe RGB pré-multiplicado.
        const float matte = 218.0f;
        for (int i = 0; i < source.width*source.height; i++) {
            Color pixel = pixels[i];
            int low = (int)fminf(pixel.r, fminf(pixel.g, pixel.b));
            int high = (int)fmaxf(pixel.r, fmaxf(pixel.g, pixel.b));
            float alpha = fmaxf(1.0f - SmoothRange(70, 142, low), SmoothRange(19, 62, high-low));
            float background = matte*(1.0f-alpha);
            float limit = 255.0f*alpha;
            pixels[i] = (Color){
                (unsigned char)fminf(limit, fmaxf(0, pixel.r-background)),
                (unsigned char)fminf(limit, fmaxf(0, pixel.g-background)),
                (unsigned char)fminf(limit, fmaxf(0, pixel.b-background)),
                (unsigned char)roundf(limit)
            };
        }
        Image prepared = {pixels, source.width, source.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        texture = LoadTextureFromImage(prepared);
        UnloadImageColors(pixels);
    }
    UnloadImage(source);
    return texture;
}

bool SaveMenuCheckpoint(int weapon) {
    if (weapon < 0 || weapon > 2) return false;
    RunProgress progress=NewRunProgress(weapon);
    return SaveClearingCheckpoint(&progress);
}

bool SaveClearingCheckpoint(const RunProgress *p) {
    char data[320];
    int level = (p->weapon >= 0 && p->weapon < 3) ? p->weaponLevel[p->weapon] : p->weaponLevel[0];
    int initWeapon = (p->initialWeapon >= 0 && p->initialWeapon < 3) ? p->initialWeapon : p->weapon;
    snprintf(data,sizeof(data),"PIF-RUN-8 %d %d %d %d %.6f %.6f %.6f %d %d %d %d %d %d %d %d\n",
        p->weapon,p->stage,p->health,p->maxHealth,p->damage,p->speed,p->bossPower,
        level,level,level,p->strongWeapon,p->map,p->guardianIntroSeen,
        p->guardianAftermath,p->epicDamageStacks);
    if (initWeapon >= 0 && initWeapon < 3) {
        size_t used=strlen(data);
        snprintf(data+used,sizeof(data)-used,"initial_weapon=%d\n",initWeapon);
    }
    if (p->villageAftermath!=VILLAGE_DUEL) {
        size_t used=strlen(data);
        snprintf(data+used,sizeof(data)-used,"village_aftermath=%d\n",p->villageAftermath);
    }
    if (p->igrisAftermathPending) {
        size_t used=strlen(data);
        snprintf(data+used,sizeof(data)-used,"igris_aftermath=1\n");
    }
    if (p->angelSceneCompleted) {
        size_t used=strlen(data);
        snprintf(data+used,sizeof(data)-used,"angel_blessing=%d\n",p->angelBlessingUses);
    }
    if (p->finalSealFailures > 0) {
        size_t used=strlen(data);
        snprintf(data+used,sizeof(data)-used,"final_seal_failures=%d\n",p->finalSealFailures);
    }
    {
        size_t used=strlen(data);
        snprintf(data+used,sizeof(data)-used,"run_seconds=%.2f\nrun_eligible=%d\n",
                 SpeedrunTime(),SpeedrunEligible()?1:0);
    }
    return SaveFileText(SavePath(), data);
}

bool LoadClearingCheckpoint(RunProgress *p) {
    if (!FileExists(SavePath())) return false;
    char *data = LoadFileText(SavePath());
    if (!data) return false;
    RunProgress loaded={0};
    int seen=0, aftermath=0;
    bool run8Format=!strncmp(data,"PIF-RUN-8 ",10);
    bool aftermathFormat=run8Format || !strncmp(data,"PIF-RUN-7 ",10);
    int fields=0;
    if (run8Format) {
        fields=sscanf(data,"PIF-RUN-8 %d %d %d %d %f %f %f %d %d %d %d %d %d %d %d",
            &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
            &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
            &loaded.weaponLevel[2],&loaded.strongWeapon,&loaded.map,&seen,&aftermath,
            &loaded.epicDamageStacks);
        if (fields!=15) { UnloadFileText(data); return false; }
        fields=13;
    } else if (aftermathFormat) {
        fields=sscanf(data,"PIF-RUN-7 %d %d %d %d %f %f %f %d %d %d %d %d %d %d",
            &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
            &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
            &loaded.weaponLevel[2],&loaded.strongWeapon,&loaded.map,&seen,&aftermath);
        if (fields!=14) { UnloadFileText(data); return false; }
        fields=13;
    } else fields=sscanf(data,"PIF-RUN-6 %d %d %d %d %f %f %f %d %d %d %d %d %d",
        &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
        &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
        &loaded.weaponLevel[2],&loaded.strongWeapon,&loaded.map,&seen);
    bool currentFormat=fields==13;
    if (currentFormat) fields=11;
    else fields=sscanf(data,"PIF-RUN-5 %d %d %d %d %f %f %f %d %d %d %d %d",
        &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
        &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
        &loaded.weaponLevel[2],&loaded.strongWeapon,&loaded.map);
    if (!currentFormat && fields==12) fields=11;
    else if (!currentFormat) fields=sscanf(data,"PIF-CLEARING-4 %d %d %d %d %f %f %f %d %d %d %d",
        &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
        &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
        &loaded.weaponLevel[2],&loaded.strongWeapon);
    if (fields!=11) {
        fields=sscanf(data,"PIF-CLEARING-3 %d %d %d %d %f %f %f %d %d %d %d",
            &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
            &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
            &loaded.weaponLevel[2],&loaded.strongWeapon);
        if (fields==11 && loaded.stage>=8 && loaded.stage<=14) {
            loaded.map=1;
            loaded.stage=loaded.stage==14 ? CLEARING_COMPLETE_STAGE :
                         loaded.stage>=12 ? CLEARING_BOSS_STAGE : loaded.stage-7;
        } else if (fields==11 && loaded.stage>=5 && loaded.stage<=7) {
            loaded.stage=loaded.stage==7?CLEARING_COMPLETE_STAGE:CLEARING_BOSS_STAGE;
        }
    }
    int weapon = -1;
    if (fields!=11 && sscanf(data,"PIF-CLEARING-2 %d",&weapon)==1 && weapon>=0 && weapon<3)
        loaded=NewRunProgress(weapon);
    else if (fields!=11) loaded.weapon=-1;
    loaded.guardianIntroSeen=currentFormat ? seen==1 : loaded.map==1 || loaded.stage>=CLEARING_COMPLETE_STAGE;
    loaded.guardianAftermath=aftermathFormat ? (GuardianAftermathState)aftermath :
        loaded.map>0 ? GUARDIAN_PASSAGE_COMPLETE :
        loaded.stage==CLEARING_COMPLETE_STAGE ? GUARDIAN_ALTAR_READY : GUARDIAN_UNPURIFIED;
    const char *village=strstr(data,"\nvillage_aftermath=");
    int aftermathVillage=0;
    bool villageValid=!village || (sscanf(village,"\nvillage_aftermath=%d",&aftermathVillage)==1 &&
        aftermathVillage>=VILLAGE_DUEL && aftermathVillage<=VILLAGE_SCENE_SEEN);
    loaded.villageAftermath=(VillageAftermath)aftermathVillage;
    const char *igris=strstr(data,"\nigris_aftermath=");
    int igrisPending=0;
    bool igrisValid=!igris || (sscanf(igris+1,"igris_aftermath=%d",&igrisPending)==1 &&
        igrisPending==1 && loaded.map==TEMPLAR_FIELD_MAP && loaded.stage==CLEARING_BOSS_STAGE);
    loaded.igrisAftermathPending=igrisPending==1;
    const char *angel=strstr(data,"\nangel_blessing=");
    bool angelValid=!angel || (sscanf(angel+1,"angel_blessing=%d",&loaded.angelBlessingUses)==1 &&
        loaded.angelBlessingUses>=0 && loaded.angelBlessingUses<=3);
    loaded.angelSceneCompleted=angel!=NULL;
    if (angelValid && loaded.angelBlessingUses>1) loaded.angelBlessingUses=1;
    const char *finalSeal=strstr(data,"\nfinal_seal_failures=");
    bool finalSealValid=!finalSeal || (sscanf(finalSeal+1,"final_seal_failures=%d",&loaded.finalSealFailures)==1 &&
        loaded.map==NECROMANCER_MAP && loaded.finalSealFailures>0 && loaded.finalSealFailures<3);
    const char *initW=strstr(data,"\ninitial_weapon=");
    int parsedInitWeapon=loaded.weapon;
    bool initWeaponValid=!initW || (sscanf(initW+1,"initial_weapon=%d",&parsedInitWeapon)==1 &&
        parsedInitWeapon>=0 && parsedInitWeapon<3);
    loaded.initialWeapon=parsedInitWeapon;
    const char *runTime=strstr(data,"\nrun_seconds=");
    const char *runEligible=strstr(data,"\nrun_eligible=");
    int eligible=0;
    bool runValid=runTime && runEligible &&
        sscanf(runTime+1,"run_seconds=%lf",&loaded.runSeconds)==1 &&
        sscanf(runEligible+1,"run_eligible=%d",&eligible)==1 &&
        isfinite(loaded.runSeconds) && loaded.runSeconds>=0 && eligible>=0 && eligible<=1;
    loaded.runEligible=runValid && eligible==1;
    UnloadFileText(data);
    /* Checkpoints de ondas removidas retomam no chefe da mesma área. */
    if ((loaded.map==CLEARING_MAP || loaded.map==CEMETERY_MAP || loaded.map==BONELAND_MAP) &&
        loaded.stage>CampaignWaveCount(loaded.map) && loaded.stage<CLEARING_BOSS_STAGE)
        loaded.stage=CLEARING_BOSS_STAGE;
    bool valid=initWeaponValid && finalSealValid && angelValid && villageValid && igrisValid &&
        (!currentFormat || seen==0 || seen==1) && loaded.map>=0 &&
        loaded.map<=(aftermathFormat ? NECROMANCER_MAP : BONELAND_MAP - 1) &&
        (loaded.map != BONELAND_MAP || loaded.stage <= BONELAND_WAVES || loaded.stage >= CLEARING_BOSS_STAGE) &&
        (loaded.map != NECROMANCER_MAP || loaded.stage >= CLEARING_BOSS_STAGE) &&
        (loaded.map != TEMPLAR_FIELD_MAP || loaded.stage <= TEMPLAR_FIELD_WAVES || loaded.stage >= CLEARING_BOSS_STAGE) &&
        loaded.weapon>=0 && loaded.weapon<3 && loaded.stage>=1 && loaded.stage<=CLEARING_COMPLETE_STAGE &&
        loaded.health>0 && loaded.health<=loaded.maxHealth && loaded.maxHealth<=10000 &&
        isfinite(loaded.damage) && loaded.damage>=1 && loaded.damage<=100 &&
        isfinite(loaded.speed) && loaded.speed>=100 && loaded.speed<=1000 &&
        isfinite(loaded.bossPower) && loaded.bossPower>=1 && loaded.bossPower<=100 &&
        loaded.strongWeapon>=-1 && loaded.strongWeapon<3 &&
        loaded.epicDamageStacks>=0 && loaded.epicDamageStacks<=100;
    for (int i=0;i<3;i++) if (loaded.weaponLevel[i]<0 || loaded.weaponLevel[i]>MAX_WEAPON_LEVEL) valid=false;
    if (loaded.guardianAftermath<GUARDIAN_UNPURIFIED || loaded.guardianAftermath>GUARDIAN_PASSAGE_COMPLETE ||
        (loaded.guardianAftermath>=GUARDIAN_LIBERATION_PENDING &&
         loaded.guardianAftermath<=GUARDIAN_PASSAGE_PENDING &&
         (loaded.map!=0 || loaded.stage!=CLEARING_COMPLETE_STAGE))) valid=false;
    if (valid) {
        if (!run8Format) {
            loaded.epicDamageStacks = loaded.strongWeapon >= 0 ? 1 : 0;
        }
        SetRunWeaponLevel(&loaded, loaded.weaponLevel[loaded.weapon]);
        *p=loaded;
    }
    return valid;
}

int LoadMenuCheckpointWeapon(void) {
    RunProgress progress;
    return LoadClearingCheckpoint(&progress) ? progress.weapon : -1;
}

bool InitMenu(Menu *m) {
    *m = (Menu){0};
    m->background = LoadTexture("assets/menu_principal_arte.png");
    m->mainButtons = LoadMainButtons();
    if (!m->background.id || !m->mainButtons.id) { UnloadMenu(m); return false; }
    SetTextureFilter(m->background, TEXTURE_FILTER_POINT);
    SetTextureFilter(m->mainButtons, TEXTURE_FILTER_POINT);
    m->scene = LoadMenuScene();
    if (m->scene) {
        m->buttonGlow = LoadTexture("assets/menu/buttons/brilho.png");
        if (m->buttonGlow.id) SetTextureFilter(m->buttonGlow, TEXTURE_FILTER_POINT);
    }
    if (!m->scene || GetVideoSettings()->reduceMenuMotion) m->introTime = menuIntroDuration;
    m->fogTexture = LoadTexture("assets/menu_nevoa_textura.png");
    if (!m->fogTexture.id) { UnloadMenu(m); return false; }
    SetTextureFilter(m->fogTexture, TEXTURE_FILTER_BILINEAR);
    const char *paths[4] = {"assets/menu_iniciar.png", "assets/menu_continuar.png",
                            "assets/menu_config.png", "assets/menu_creditos.png"};
    for (int i=0; i<4; i++) {
        m->buttons[i] = LoadTexture(paths[i]);
        if (!m->buttons[i].id) { UnloadMenu(m); return false; }
        SetTextureFilter(m->buttons[i], TEXTURE_FILTER_POINT);
    }
    static const char *creditFiles[4] = {
        "assets/credits/arthur.png", "assets/credits/marcos.png",
        "assets/credits/silvio.png", "assets/credits/joao_paulo.png"
    };
    for (int i=0; i<4; i++) {
        m->creditPhotos[i] = LoadTexture(creditFiles[i]);
        if (!m->creditPhotos[i].id) { UnloadMenu(m); return false; }
        SetTextureFilter(m->creditPhotos[i], TEXTURE_FILTER_BILINEAR);
    }
    m->hasSave = LoadMenuCheckpointWeapon() >= 0;
    m->pause.background = LoadTexture("assets/pausa_fundo.png");
    m->pause.frame = LoadPauseFrame();
    m->pause.labels = LoadTexture("assets/pausa_rotulos.png");
    m->pause.shader = LoadShader(NULL, "assets/shaders/pausa.fs");
    if (m->pause.shader.id && m->pause.shader.id != rlGetShaderIdDefault()) {
        m->pause.timeLocation = GetShaderLocation(m->pause.shader, "time");
        m->pause.layerLocation = GetShaderLocation(m->pause.shader, "layer");
        m->pause.fogLocation = GetShaderLocation(m->pause.shader, "fogTexture");
    } else m->pause.shader = (Shader){0};
    if (m->pause.background.id) SetTextureFilter(m->pause.background, TEXTURE_FILTER_BILINEAR);
    if (m->pause.frame.id) SetTextureFilter(m->pause.frame, TEXTURE_FILTER_BILINEAR);
    InitGameAudio(&m->audio);
    return true;
}

void UnloadMenu(Menu *m) {
    if (m->volumeChanged) SaveAudioSettings(&m->audio);
    UnloadGameAudio(&m->audio);
    if (m->pause.shader.id) UnloadShader(m->pause.shader);
    if (m->pause.labels.id) UnloadTexture(m->pause.labels);
    if (m->pause.frame.id) UnloadTexture(m->pause.frame);
    if (m->pause.background.id) UnloadTexture(m->pause.background);
    for (int i=0; i<4; i++) if (m->buttons[i].id) UnloadTexture(m->buttons[i]);
    for (int i=0; i<4; i++) if (m->creditPhotos[i].id) UnloadTexture(m->creditPhotos[i]);
    if (m->fogTexture.id) UnloadTexture(m->fogTexture);
    if (m->mainButtons.id) UnloadTexture(m->mainButtons);
    if (m->buttonGlow.id) UnloadTexture(m->buttonGlow);
    UnloadMenuScene(m->scene);
    if (m->background.id) UnloadTexture(m->background);
    *m = (Menu){0};
}

static Rectangle PauseButtonBounds(int index) {
    return MenuButtonBounds(index, 836.0f, 543.0f, 370.0f);
}
static const Rectangle pauseBack = {696, 710, 280, 84};
static const Rectangle volumeBars[AUDIO_CHANNELS] = {
    [AUDIO_MASTER]={566, 442, 540, 16},
    [AUDIO_MUSIC]={566, 526, 540, 16},
    [AUDIO_EFFECTS]={566, 610, 540, 16}
};
static const Rectangle muteButton = {694, 640, 284, 54};

static Rectangle PauseCanvas(void) {
    float scale = fminf(GetScreenWidth()/1672.0f, GetScreenHeight()/941.0f);
    float width = 1672 * scale, height = 941 * scale;
    return (Rectangle){(GetScreenWidth()-width)/2, (GetScreenHeight()-height)/2, width, height};
}

static Vector2 SettingsMouse(Rectangle canvas) {
    Vector2 mouse = GetMousePosition();
    float scale = canvas.width / 1672.0f;
    return (Vector2){(mouse.x-canvas.x)/scale, (mouse.y-canvas.y)/scale};
}

/* Abas das Configurações e linhas da aba Vídeo, em coordenadas da arte (1672x941). */
static const Rectangle settingsTabs[2] = {{566, 288, 260, 56}, {846, 288, 260, 56}};
enum { VIDEO_ROW_FULLSCREEN, VIDEO_ROW_RENDER, VIDEO_ROW_FPS, VIDEO_ROW_COUNTER, VIDEO_ROW_LIGHTING, VIDEO_ROW_MENU_MOTION, VIDEO_ROWS };

static Rectangle VideoRow(int row) {
    return (Rectangle){455, 377 + row*47.0f, 762, 43};
}

static Rectangle VideoValue(int row) {
    Rectangle bounds = VideoRow(row);
    return (Rectangle){bounds.x + bounds.width - 350, bounds.y, 350, bounds.height};
}

static void ChangeVideoOption(int row, int direction) {
    VideoSettings *video = GetVideoSettings();
    switch (row) {
        case VIDEO_ROW_FULLSCREEN: ToggleGameFullscreen(); break;
        case VIDEO_ROW_RENDER: CycleVideoRenderScale(video, direction); break;
        case VIDEO_ROW_FPS:
            CycleVideoFpsLimit(video, direction);
            SetTargetFPS(video->fpsLimit);
            break;
        case VIDEO_ROW_COUNTER: video->showFps = !video->showFps; break;
        case VIDEO_ROW_LIGHTING: video->dynamicLighting = !video->dynamicLighting; break;
        case VIDEO_ROW_MENU_MOTION: video->reduceMenuMotion = !video->reduceMenuMotion; break;
    }
    SaveVideoSettings(video);
}

static void UpdateVideoSettings(Menu *menu, Vector2 mouse, bool click) {
    if (KeyPressed(KEY_UP) || KeyPressed(KEY_W) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_UP))
        menu->videoFocus = (menu->videoFocus + VIDEO_ROWS - 1) % VIDEO_ROWS;
    if (KeyPressed(KEY_DOWN) || KeyPressed(KEY_S) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        menu->videoFocus = (menu->videoFocus + 1) % VIDEO_ROWS;
    int direction = (KeyPressed(KEY_RIGHT) || KeyPressed(KEY_D) || KeyPressed(KEY_ENTER) ||
                    PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) ? 1 :
                    (KeyPressed(KEY_LEFT) || KeyPressed(KEY_A) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) ? -1 : 0;
    if (direction) ChangeVideoOption(menu->videoFocus, direction);
    if (!click) return;
    for (int row = 0; row < VIDEO_ROWS; row++) {
        if (!CheckCollisionPointRec(mouse, VideoRow(row))) continue;
        Rectangle value = VideoValue(row);
        menu->videoFocus = row;
        ChangeVideoOption(row, mouse.x < value.x + value.width/2 ? -1 : 1);
    }
}

static bool UpdateSettings(Menu *menu) {
    Vector2 mouse = SettingsMouse(PauseCanvas());
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool back = KeyPressed(KEY_ESCAPE) || KeyPressed(KEY_BACKSPACE) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
                (click && CheckCollisionPointRec(mouse, pauseBack));
    if (KeyPressed(KEY_TAB) || KeyPressed(KEY_Q) || KeyPressed(KEY_E) ||
        PadPressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1) || PadPressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ||
        (UsingArcade() && PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)))
        menu->settingsTab = 1 - menu->settingsTab;
    for (int tab = 0; tab < 2; tab++)
        if (click && CheckCollisionPointRec(mouse, settingsTabs[tab])) menu->settingsTab = tab;
    if (!back && KeyPressed(KEY_F)) {
        ToggleGameFullscreen();
        SaveVideoSettings(GetVideoSettings());
    }
    if (menu->settingsTab == 1) {
        if (!back) UpdateVideoSettings(menu, mouse, click);
        return back;
    }
    static const AudioChannel next[] = {
        [AUDIO_MASTER]=AUDIO_MUSIC, [AUDIO_MUSIC]=AUDIO_EFFECTS, [AUDIO_EFFECTS]=AUDIO_MASTER
    };
    static const AudioChannel previous[] = {
        [AUDIO_MASTER]=AUDIO_EFFECTS, [AUDIO_MUSIC]=AUDIO_MASTER, [AUDIO_EFFECTS]=AUDIO_MUSIC
    };
    if (KeyPressed(KEY_UP) || KeyPressed(KEY_W) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_UP))
        menu->volumeFocus = previous[menu->volumeFocus];
    if (KeyPressed(KEY_DOWN) || KeyPressed(KEY_S) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        menu->volumeFocus = next[menu->volumeFocus];
    if (KeyPressed(KEY_M) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_UP) ||
        (UsingArcade() && PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) ||
        (click && CheckCollisionPointRec(mouse,muteButton))) {
        menu->audio.muted=!menu->audio.muted;
        SaveAudioSettings(&menu->audio);
    }
    if (click) {
        for (int i = 0; i < AUDIO_CHANNELS; i++) {
            Rectangle bar = volumeBars[i];
            bar.y -= 19;
            bar.height += 38;
            if (CheckCollisionPointRec(mouse, bar)) {
                menu->volumeFocus = i;
                menu->volumeDrag = i+1;
            }
        }
    }

    int channel = menu->volumeDrag ? menu->volumeDrag-1 : menu->volumeFocus;
    int value = menu->audio.volume[channel];
    if (menu->volumeDrag && (click || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        Rectangle bar = volumeBars[channel];
        value = (int)roundf(100.0f*(mouse.x-bar.x)/bar.width);
    } else {
        if (KeyPressed(KEY_LEFT) || KeyPressed(KEY_A) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) value -= 5;
        if (KeyPressed(KEY_RIGHT) || KeyPressed(KEY_D) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) value += 5;
    }
    value = (int)fmaxf(0, fminf(100, value));
    if (value != menu->audio.volume[channel]) {
        menu->audio.volume[channel] = value;
        menu->volumeChanged = true;
    }
    if (back || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (menu->volumeChanged) {
            SaveAudioSettings(&menu->audio);
            if (channel == AUDIO_EFFECTS) PlayAudioFeedback(&menu->audio);
            menu->volumeChanged = false;
        }
        menu->volumeDrag = 0;
    }
    return back;
}

/* A tela cheia exclusiva (ToggleFullscreen) mantinha o jogo desenhado por cima de
   tudo após Alt+Tab, porque a raylib desliga a minimização automática ao perder o
   foco. A janela sem borda do tamanho do monitor tem a mesma resolução nativa e
   deixa o sistema trocar de janela normalmente. */
bool IsGameFullscreen(void) {
    return IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
}

void ToggleGameFullscreen(void) {
    ToggleBorderlessWindowed();
    /* A raylib 5.5 também marca a janela sem borda como "sempre no topo", o que a
       deixaria por cima das outras depois de Alt+Tab. */
    if (IsWindowState(FLAG_WINDOW_TOPMOST)) ClearWindowState(FLAG_WINDOW_TOPMOST);
    VideoSettings *video = GetVideoSettings();
    video->fullscreen = IsGameFullscreen();
    /* Fora da tela cheia, a janela usa a resolução escolhida nas Configurações. */
    if (!video->fullscreen) ApplyVideoWindowSize();
}

PauseAction UpdatePauseMenu(Menu *menu) {
    PauseMenu *pause = &menu->pause;
    float dt = GetFrameTime();
    pause->time += dt;
    Vector2 mouse = SettingsMouse(PauseCanvas());
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool escape = KeyPressed(KEY_ESCAPE) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
                  PadPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT);
    if (pause->page == PAUSE_SETTINGS) {
        if (UpdateSettings(menu) || PadPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT)) pause->page = PAUSE_OPTIONS;
        return PAUSE_NONE;
    }
    if (pause->page != PAUSE_OPTIONS) {
        if (escape || KeyPressed(KEY_BACKSPACE) || (click && CheckCollisionPointRec(mouse, pauseBack))) {
            pause->page = PAUSE_OPTIONS;
        }
        return PAUSE_NONE;
    }
    if (escape) return PAUSE_RESUME;
    int previousSelection=pause->selected;
    if (KeyPressed(KEY_UP) || KeyPressed(KEY_W) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_UP)) pause->selected = (pause->selected + 3) % 4;
    if (KeyPressed(KEY_DOWN) || KeyPressed(KEY_S) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) pause->selected = (pause->selected + 1) % 4;
    Vector2 movement = GetMouseDelta();
    if (click || (!UsingGamepad() && (movement.x != 0 || movement.y != 0))) {
        for (int i = 0; i < 4; i++)
            if (CheckCollisionPointRec(mouse, PauseButtonBounds(i))) pause->selected = i;
    }
    UpdateButtonFocus(pause->buttonFocus, pause->selected, dt);
    if (previousSelection != pause->selected) PlaySfx(SFX_UI_FOCUS);
    if (click ? !CheckCollisionPointRec(mouse, PauseButtonBounds(pause->selected)) :
                !KeyPressed(KEY_ENTER) && !KeyPressed(KEY_SPACE) &&
                !PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) return PAUSE_NONE;
    PlayAudioFeedback(&menu->audio);
    if (pause->selected == 0) return PAUSE_RESUME;
    if (pause->selected == 3) return PAUSE_MAIN_MENU;
    pause->page = pause->selected == 1 ? PAUSE_SETTINGS : PAUSE_CONTROLS;
    return PAUSE_NONE;
}

static Rectangle PauseRect(Rectangle canvas, Rectangle logical) {
    float scale = canvas.width / 1672.0f;
    return (Rectangle){canvas.x + logical.x*scale, canvas.y + logical.y*scale,
                       logical.width*scale, logical.height*scale};
}

static void DrawPauseText(Rectangle canvas, const char *label, float y, int size, Color color) {
    float scale = canvas.width / 1672.0f;
    int fontSize = (int)fmaxf(1, size * scale);
    DrawGameText(label, (int)(canvas.x + (canvas.width - MeasureGameText(label, fontSize))/2),
             (int)(canvas.y + y*scale), fontSize, color);
}

static void DrawPauseLayer(const PauseMenu *pause, int layer, Texture2D texture,
                           Rectangle source, Rectangle destination, Color tint) {
    BeginShaderMode(pause->shader);
    SetShaderValue(pause->shader, pause->layerLocation, &layer, SHADER_UNIFORM_INT);
    DrawTexturePro(texture, source, destination, (Vector2){0}, 0, tint);
    EndShaderMode();
}

static void DrawPausePlate(const Menu *menu, Rectangle bounds, Color tint) {
    Texture2D plate = menu->buttons[2];
    if (menu->pause.shader.id)
        DrawPauseLayer(&menu->pause, PAUSE_PLATE, plate,
                       (Rectangle){0, 0, plate.width, plate.height}, bounds, tint);
    else
        DrawRectangleRounded(bounds, .18f, 8, MAROON);
}

static void DrawButtonLabel(const char *label, Rectangle bounds, Color tint) {
    Rectangle textArea = {bounds.x + bounds.width*.332f, bounds.y + bounds.height*.24f,
                          bounds.width*.553f, bounds.height*.52f};
    Color ink = ColorTint(buttonText, tint);
    if (ink.a < 170) ink.a = 170;
    DrawGameTextIn(label, textArea, bounds.height*.35f, ink, GAME_TEXT_TITLE);
}

static void DrawLabeledButton(const Menu *menu, int index, const char *label,
                              Rectangle bounds, Color tint) {
    Texture2D button = menu->buttons[index];
    if (!menu->pause.shader.id) {
        DrawPausePlate(menu, bounds, tint);
        DrawGameTextIn(label, bounds, bounds.height*.33f, ColorTint(buttonText, tint), GAME_TEXT_TITLE);
        return;
    }
    DrawPauseLayer(&menu->pause, MENU_BUTTON, button, (Rectangle){0, 0, button.width, button.height}, bounds, tint);
    DrawButtonLabel(label, bounds, tint);
}

static void DrawPauseButton(const Menu *menu, Rectangle canvas, int index) {
    static const char *labels[] = {"Continuar", "Configurações", "Controles", "Voltar ao Menu"};
    const PauseMenu *pause = &menu->pause;
    Rectangle logical = PauseButtonBounds(index);
    Rectangle bounds = PauseRect(canvas, logical);
    float focus = pause->buttonFocus[index];
    Color tint = MenuButtonTint(bounds, focus, false);

    if (index < 2) {
        DrawLabeledButton(menu, index+1, labels[index], bounds, tint);
        return;
    }

    DrawPausePlate(menu, bounds, tint);
    if (!pause->shader.id || !pause->labels.id) {
        DrawGameTextIn(labels[index], bounds, bounds.height*.33f, ColorTint(buttonText, tint), GAME_TEXT_TITLE);
        return;
    }

    static const Rectangle icons[] = {{651, 402, 128, 82}, {662, 560, 112, 92}};
    Rectangle icon = icons[index-2];
    float iconHeight = fminf(bounds.height*.37f, bounds.width*.11f*icon.height/icon.width);
    DrawPauseLayer(pause, PAUSE_ICON, pause->labels, icon,
                   (Rectangle){bounds.x + bounds.width*.21f - icon.width*iconHeight/icon.height/2,
                               bounds.y + (bounds.height-iconHeight)/2,
                               icon.width*iconHeight/icon.height, iconHeight}, tint);
    DrawRectangleRec((Rectangle){bounds.x + bounds.width*.304f, bounds.y + bounds.height*.34f,
                                 bounds.width*.005f, bounds.height*.30f}, Fade(GOLD, .62f));
    DrawButtonLabel(labels[index], bounds, tint);
}

static void DrawPauseTextButton(const Menu *menu, Rectangle canvas, Rectangle logical,
                                const char *label, int fontSize) {
    Rectangle bounds = PauseRect(canvas, logical);
    float focus = CheckCollisionPointRec(GetMousePosition(), bounds) ? 1.0f : 0.0f;
    Color tint = MenuButtonTint(bounds, focus, false);
    DrawPausePlate(menu, bounds, tint);
    DrawGameTextIn(label, bounds, fontSize*canvas.width/1672.0f, ColorTint(buttonText, tint), GAME_TEXT_BODY);
}

static void DrawSettingsTabs(const Menu *menu, Rectangle canvas) {
    static const char *names[2] = {"Áudio", "Vídeo"};
    for (int tab = 0; tab < 2; tab++) {
        Rectangle bounds = PauseRect(canvas, settingsTabs[tab]);
        bool active = menu->settingsTab == tab;
        Color tint = MenuButtonTint(bounds, active ? 1.0f : 0.0f, false);
        if (!active) tint = ColorBrightness(tint, -.35f);
        DrawPausePlate(menu, bounds, tint);
        DrawGameTextIn(names[tab], bounds, 32*canvas.width/1672.0f,
                       active ? GOLD : ColorTint(buttonText, tint), GAME_TEXT_BODY);
    }
}

static void DrawVideoSettings(const Menu *menu, Rectangle canvas) {
    static const char *labels[VIDEO_ROWS] = {
        "Tela cheia", "Renderização", "Limite de FPS", "Contador de FPS", "Iluminação dinâmica", "Movimento do menu"
    };
    const VideoSettings *video = GetVideoSettings();
    float scale = canvas.width/1672.0f;
    for (int row = 0; row < VIDEO_ROWS; row++) {
        Rectangle bounds = PauseRect(canvas, VideoRow(row));
        bool focused = menu->videoFocus == row;
        DrawRectangleRounded(bounds, .3f, 8, focused ? (Color){87, 47, 53, 235} : (Color){40, 22, 28, 190});
        if (focused) DrawRectangleRoundedLinesEx(bounds, .3f, 8, fmaxf(1, 2*scale), GOLD);
        int size = (int)fmaxf(12, 31*scale);
        DrawGameText(labels[row], (int)(bounds.x + 22*scale), (int)(bounds.y + (bounds.height-size)/2),
                     size, focused ? GOLD : RAYWHITE);
        const char *value = "";
        switch (row) {
            case VIDEO_ROW_FULLSCREEN: value = IsGameFullscreen() ? "Ligada" : "Desligada"; break;
            case VIDEO_ROW_RENDER:
                value = video->renderScale >= 100 ? "100% (nativa)" : TextFormat("%d%%", video->renderScale);
                break;
            case VIDEO_ROW_FPS: value = TextFormat("%d FPS", video->fpsLimit); break;
            case VIDEO_ROW_COUNTER: value = video->showFps ? "Ligado" : "Desligado"; break;
            case VIDEO_ROW_LIGHTING: value = video->dynamicLighting ? "Ligada" : "Desligada"; break;
            case VIDEO_ROW_MENU_MOTION: value = video->reduceMenuMotion ? "Reduzido" : "Completo"; break;
        }
        DrawGameTextIn(TextFormat("<   %s   >", value), PauseRect(canvas, VideoValue(row)),
                       31*scale, focused ? GOLD : RAYWHITE, GAME_TEXT_BODY);
    }
    const char *hint = InputHint("Setas ou clique alteram  ·  Tab troca de aba", "Direcional ajusta  ·  L1/R1 troca de aba");
    if (menu->videoFocus == VIDEO_ROW_RENDER)
        hint = "Abaixo de 100%, a imagem fica menos nítida e mais leve";
    if (menu->videoFocus == VIDEO_ROW_LIGHTING)
        hint = "Luzes do cenário e dos ataques";
    if (menu->videoFocus == VIDEO_ROW_MENU_MOTION)
        hint = "Reduzido congela o cenário e pula as transições animadas";
    DrawPauseText(canvas, hint, 660, 27, LIGHTGRAY);
}

static void DrawSettings(const Menu *menu, Rectangle canvas) {
    DrawMenuPanel(PauseRect(canvas, (Rectangle){395, 148, 882, 668}));
    DrawGameTextIn("Configurações", PauseRect(canvas, (Rectangle){395, 198, 882, 76}),
                   62*canvas.width/1672.0f, buttonText, GAME_TEXT_TITLE);
    DrawSettingsTabs(menu, canvas);
    if (menu->settingsTab == 1) {
        DrawVideoSettings(menu, canvas);
        DrawPauseTextButton(menu, canvas, pauseBack, InputHint("Voltar  ·  Esc", "Voltar  ·  [Círculo]"), 28);
        return;
    }
    static const char *names[AUDIO_CHANNELS] = {"Música", "Efeitos sonoros", "Volume geral"};
    float scale = canvas.width/1672.0f;
    for (int i = 0; i < AUDIO_CHANNELS; i++) {
        Rectangle bar = PauseRect(canvas, volumeBars[i]);
        int size = (int)fmaxf(12, 37*scale);
        bool focused = menu->volumeFocus == i;
        DrawGameText(names[i], (int)bar.x, (int)(bar.y-42*scale), size, focused ? GOLD : RAYWHITE);
        const char *percent = TextFormat("%d%%", menu->audio.volume[i]);
        DrawGameText(percent, (int)(bar.x+bar.width-MeasureGameText(percent,size)),
                 (int)(bar.y-42*scale), size, RAYWHITE);
        DrawRectangleRounded(bar, .6f, 8, (Color){87, 47, 53, 255});
        float filled = bar.width*menu->audio.volume[i]/100.0f;
        if (filled > 0.0f)
            DrawRectangleRec((Rectangle){bar.x,bar.y,filled,bar.height}, focused ? GOLD : ORANGE);
        DrawCircleV((Vector2){bar.x+filled,bar.y+bar.height/2}, 11*scale, focused ? GOLD : RAYWHITE);
    }
    const char *muteLabel = menu->audio.muted
        ? InputHint("Ativar som  ·  M", UsingArcade() ? "Ativar som  ·  [Cruz]" : "Ativar som  ·  [Triângulo]")
        : InputHint("Silenciar  ·  M", UsingArcade() ? "Silenciar  ·  [Cruz]" : "Silenciar  ·  [Triângulo]");
    DrawPauseTextButton(menu, canvas, muteButton, muteLabel, 26);
    DrawPauseTextButton(menu, canvas, pauseBack, InputHint("Voltar  ·  Esc", "Voltar  ·  [Círculo]"), 28);
}

void DrawPauseMenu(const Menu *menu) {
    const PauseMenu *pause = &menu->pause;
    Rectangle canvas = PauseCanvas();
    if (pause->shader.id)
        SetShaderValue(pause->shader, pause->timeLocation, &pause->time, SHADER_UNIFORM_FLOAT);
    if (pause->background.id) {
        Texture2D background = pause->background;
        float scale = fmaxf((float)GetScreenWidth()/background.width, (float)GetScreenHeight()/background.height);
        float width = background.width*scale, height = background.height*scale;
        Rectangle source = {0, 0, background.width, background.height};
        float x = (GetScreenWidth()-width)/2;
        // Na janela estreita, deixe a lua dentro do enquadramento.
        x = fmaxf(GetScreenWidth()-width, fminf(x, GetScreenWidth()*.84f - width*.812f));
        Rectangle destination = {x, (GetScreenHeight()-height)/2, width, height};
        if (pause->shader.id) {
            BeginShaderMode(pause->shader);
            const int layer = PAUSE_SCENE;
            SetShaderValue(pause->shader, pause->layerLocation, &layer, SHADER_UNIFORM_INT);
            SetShaderValueTexture(pause->shader, pause->fogLocation, menu->fogTexture);
            DrawTexturePro(background, source, destination, (Vector2){0}, 0, WHITE);
            EndShaderMode();
        } else DrawTexturePro(background, source, destination, (Vector2){0}, 0, WHITE);
    }
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BLACK, pause->page == PAUSE_OPTIONS ? .12f : .35f));
    if (pause->page == PAUSE_OPTIONS) {
        if (pause->shader.id && pause->frame.id) {
            Rectangle source = {0, 0, pause->frame.width, pause->frame.height*.67f};
            Rectangle destination = PauseRect(canvas, (Rectangle){300, 23, 1072, 1072*source.height/source.width});
            DrawPauseLayer(pause, PAUSE_FRAME, pause->frame, source, destination, WHITE);
        }
        DrawGameTextIn("Pausa", PauseRect(canvas, (Rectangle){720, 144, 232, 66}),
                       63*canvas.width/1672.0f, buttonText, GAME_TEXT_TITLE);
        for (int i = 0; i < 4; i++) DrawPauseButton(menu, canvas, i);
        return;
    }

    if (pause->page == PAUSE_SETTINGS) {
        DrawSettings(menu, canvas);
        return;
    }
    Rectangle panel = PauseRect(canvas, (Rectangle){395, 148, 882, 668});
    DrawMenuPanel(panel);
    DrawPauseTextButton(menu, canvas, pauseBack, InputHint("Voltar  ·  Esc", "Voltar  ·  [Círculo]"), 28);
    DrawGameTextIn("Controles", PauseRect(canvas, (Rectangle){395, 198, 882, 80}),
                   65*canvas.width/1672.0f, buttonText, GAME_TEXT_TITLE);
    DrawPauseText(canvas, UsingArcade() ? "Alavanca  ·  Mover e mirar" :
                  InputHint("WASD  ·  Mover", "Analógico esquerdo  ·  Mover / analógico direito  ·  Mirar"), 331, 37, RAYWHITE);
    DrawPauseText(canvas, InputHint("Espaço / Clique direito  ·  Esquivar com qualquer arma", "L1/L2  ·  Esquivar com qualquer arma"), 400, 34, RAYWHITE);
    DrawPauseText(canvas, InputHint("Clique esquerdo  ·  Atacar ou acelerar a Bíblia", "R1/R2  ·  Atacar ou acelerar a Bíblia"), 469, 34, RAYWHITE);
    DrawPauseText(canvas, InputHint("E  ·  Interagir e selar", "[Quadrado]  ·  Interagir e selar"), 538, 37, RAYWHITE);
}

static MenuAction BeginMenuDeparture(Menu *m) {
    if (!m->scene || GetVideoSettings()->reduceMenuMotion) return MENU_START;
    m->departureTime = 0;
    m->starting = true;
    return MENU_NONE;
}

bool MenuControlsVisible(const Menu *m) {
    return m->introTime >= menuIntroDuration && !m->starting;
}

MenuAction UpdateMenu(Menu *m) {
    const bool reduced = GetVideoSettings()->reduceMenuMotion;
    const float dt = fminf(GetFrameTime(), .1f);
    if (!reduced) m->sceneTime += dt;
    if (!m->scene || reduced) m->introTime = menuIntroDuration;
    if (m->starting) {
        m->departureTime += dt;
        if (!reduced && m->departureTime < menuDepartureDuration) return MENU_NONE;
        m->starting = false;
        return MENU_START;
    }
    if (m->introTime < menuIntroDuration) {
        m->introTime = fminf(menuIntroDuration, m->introTime + dt);
        if (GetKeyPressed() || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || PadButtonPressed()) {
            m->introTime = menuIntroDuration;
            return MENU_NONE;
        }
        if (m->introTime < menuIntroDuration) return MENU_NONE;
    }
    m->hasSave = LoadMenuCheckpointWeapon() >= 0;
    bool back = KeyPressed(KEY_ESCAPE) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    bool confirm = KeyPressed(KEY_ENTER) || KeyPressed(KEY_SPACE) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool ranking = KeyPressed(KEY_L) || PadPressed(GAMEPAD_BUTTON_MIDDLE_LEFT) ||
                   (UsingArcade() && PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT));
    if (m->page == 4) {
        if (back || KeyPressed(KEY_BACKSPACE) || ranking) m->page=0;
        return MENU_NONE;
    }
    if (m->page == 0 && ranking) { m->page=4; return MENU_NONE; }
    if (m->page == 3) {
        if (back) m->page = 0;
        else if (confirm) { m->page = 0; return BeginMenuDeparture(m); }
        return MENU_NONE;
    }
    if (m->page == 1) {
        if (UpdateSettings(m)) m->page = 0;
        return MENU_NONE;
    }
    if (m->page == 2) {
        if (back || KeyPressed(KEY_BACKSPACE) || confirm) m->page = 0;
        return MENU_NONE;
    }
    if (KeyPressed(KEY_ESCAPE)) return MENU_QUIT;
    int previousSelection=m->selected;
    if (KeyPressed(KEY_DOWN) || KeyPressed(KEY_S) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        m->selected = (m->selected + 1) % MAIN_BUTTON_COUNT;
    if (KeyPressed(KEY_UP) || KeyPressed(KEY_W) || PadNavigate(GAMEPAD_BUTTON_LEFT_FACE_UP))
        m->selected = (m->selected + MAIN_BUTTON_COUNT - 1) % MAIN_BUTTON_COUNT;
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();
    Vector2 mouseDelta = GetMouseDelta();
    if (click || (!UsingGamepad() && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f))) {
        for (int i = 0; i < MAIN_BUTTON_COUNT; i++)
            if (CheckCollisionPointRec(mouse, MainButtonBounds(m, i))) m->selected = i;
    }
    if (previousSelection != m->selected) PlaySfx(SFX_UI_FOCUS);
    if (click && CheckCollisionPointRec(mouse, TestButtonBounds())) {
        PlayAudioFeedback(&m->audio);
        return MENU_TEST_VILLA;
    }
    if (click && CheckCollisionPointRec(mouse, TestChapelButtonBounds())) {
        PlayAudioFeedback(&m->audio);
        return MENU_TEST_CHAPEL;
    }
    if (click && CheckCollisionPointRec(mouse, TestInvertedChapelButtonBounds())) {
        PlayAudioFeedback(&m->audio);
        return MENU_TEST_INVERTED_CHAPEL;
    }
    if (!click && !confirm) return MENU_NONE;
    if (click && !CheckCollisionPointRec(mouse, MainButtonBounds(m, m->selected))) return MENU_NONE;
    if (m->selected != MAIN_CONTINUE || m->hasSave) PlayAudioFeedback(&m->audio);
    switch (m->selected) {
        case MAIN_START: if (m->hasSave) { m->page = 3; return MENU_NONE; } return BeginMenuDeparture(m);
        case MAIN_CONTINUE: return m->hasSave ? MENU_CONTINUE : MENU_NONE;
        case MAIN_SETTINGS: m->page = 1; return MENU_NONE;
        case MAIN_CREDITS: m->page = 2; return MENU_NONE;
        case MAIN_QUIT: return MENU_QUIT;
        default: return MENU_NONE;
    }
}

static void DrawMainButton(const Menu *m, int index) {
    Rectangle bounds = MainButtonBounds(m, index);
    Rectangle sprite = mainButtonSprites[index];
    bool selected = index == m->selected;
    bool disabled = index == MAIN_CONTINUE && !m->hasSave;
    bool reduced = GetVideoSettings()->reduceMenuMotion;
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool pressed = !disabled && ((m->starting && selected) ||
                   (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)));
    float opacity = SmoothRange(2.03f + index*.22f, 2.60f + index*.22f, m->introTime);
    if (m->starting && !selected) opacity *= 1-SmoothRange(0, .25f, m->departureTime);
    if (opacity <= 0) return;
    float scale = fminf(bounds.width / sprite.width, bounds.height / sprite.height);
    if (!selected && !hovered) scale *= 0.96f;
    if (pressed) scale *= .97f;
    Rectangle dest = {
        bounds.x + (bounds.width - sprite.width * scale) / 2,
        bounds.y + (bounds.height - sprite.height * scale) / 2,
        sprite.width * scale, sprite.height * scale
    };
    Color tint = pressed ? (Color){255, 236, 189, 255} :
                 (selected || hovered) ? WHITE : (Color){223, 218, 215, 255};
    tint = Fade(tint, opacity*(disabled ? .50f : 1));
    DrawTexturePro(m->mainButtons, sprite, dest, (Vector2){0}, 0, tint);
    if (m->buttonGlow.id && !disabled && (selected || hovered)) {
        int pulse = reduced ? 5 : (int)(fmodf(m->sceneTime, 1.6f)*7.5f);
        int frame = index*12 + pulse;
        Rectangle glow = {(frame%6)*360, (frame/6)*93, 360, 93};
        Color light = pressed ? (Color){255,225,146,255} : WHITE;
        DrawTexturePro(m->buttonGlow, glow, dest, (Vector2){0}, 0, Fade(light, opacity*(selected ? 1 : .60f)));
    }
    if (selected) {
        float centerY = bounds.y + bounds.height / 2;
        float radius = fmaxf(2, bounds.height * 0.045f);
        Color marker = Fade(disabled ? buttonText : (Color){255, 106, 82, 255}, opacity*(disabled ? .5f : 1));
        DrawPoly((Vector2){bounds.x + radius, centerY}, 4, radius, 0, marker);
        DrawPoly((Vector2){bounds.x + bounds.width - radius, centerY}, 4, radius, 0, marker);
    }
}

static void DrawMainMenuShortcut(const char *label, Rectangle bounds) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    Color border = Fade(buttonText, hovered ? 0.75f : 0.32f);
    DrawRectangleRounded(bounds, 0.20f, 8, Fade((Color){30, 12, 24, 255}, hovered ? 0.88f : 0.70f));
    DrawRectangleRoundedLinesEx(bounds, 0.20f, 8, 1, border);
    DrawGameTextIn(label, bounds, 20, hovered ? RAYWHITE : buttonText, GAME_TEXT_TITLE);
}

void DrawMenu(const Menu *m) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){22, 15, 22, 255});
    Rectangle crop = MainMenuBackdropSource(m->background);
    Rectangle canvas = MainMenuCanvas(m);
    bool reduced = GetVideoSettings()->reduceMenuMotion;
    float departure = m->starting ? m->departureTime / menuDepartureDuration : 0;
    if (m->scene && !reduced)
        DrawMenuScene(m->scene, canvas, crop, m->sceneTime, m->introTime, departure, false);
    else
        DrawTexturePro(m->background, crop, canvas, (Vector2){0}, 0, WHITE);
    if (m->page != 0) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
    if (m->page==4) {
        SpeedrunBoard board;
        SpeedrunLoad(&board);
        float w=fminf(720,GetScreenWidth()-32.0f);
        Rectangle panel={(GetScreenWidth()-w)/2,12,w,GetScreenHeight()-24.0f};
        DrawMenuPanel(panel);
        float rowHeight=fminf(42.0f,(panel.height-112.0f)/SPEEDRUN_TOP);
        DrawGameTextIn("Ranking de speedrun",(Rectangle){panel.x+12,panel.y+12,w-24,44},
                       34,buttonText,GAME_TEXT_TITLE);
        if (!board.count) DrawGameTextIn("Nenhuma campanha concluída ainda",
            (Rectangle){panel.x+16,panel.y+80,w-32,40},24,RAYWHITE,GAME_TEXT_BODY);
        for (int i=0;i<board.count;i++) {
            char timeText[40], row[96];
            SpeedrunFormat(timeText,sizeof(timeText),board.times[i]);
            snprintf(row,sizeof(row),"%2d.  %s",i+1,board.names[i]);
            Color color=i==0?GOLD:RAYWHITE;
            DrawGameTextIn(row,(Rectangle){panel.x+16,panel.y+66+i*rowHeight,w*.62f-16,rowHeight},
                           (int)fminf(24.0f,rowHeight*.78f),color,GAME_TEXT_BODY);
            DrawGameTextIn(timeText,(Rectangle){panel.x+w*.62f,panel.y+66+i*rowHeight,w*.38f-16,rowHeight},
                           (int)fminf(24.0f,rowHeight*.78f),color,GAME_TEXT_BODY);
        }
        DrawGameTextIn(InputHint("L ou Esc: voltar", "[Círculo] ou [Share]: voltar"),(Rectangle){panel.x+16,panel.y+panel.height-48,w-32,32},
                       20,LIGHTGRAY,GAME_TEXT_BODY);
        return;
    }
    if (m->page == 0) {
        for (int i = 0; i < MAIN_BUTTON_COUNT; i++) DrawMainButton(m, i);
        if (!MenuControlsVisible(m)) {
            float darkness = m->starting ? SmoothRange(.25f, 1, departure) : 1-SmoothRange(0, 1.1f, m->introTime);
            if (m->starting) DrawRectangleRec(canvas, Fade((Color){143, 12, 54, 255}, .13f*departure));
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, darkness));
            return;
        }
        if (!m->hasSave) {
            const char *hint = "Continuar disponível\napós iniciar uma partida";
            Rectangle button = MainButtonBounds(m, MAIN_CONTINUE);
            float x = button.x + button.width + 12;
            float width = fminf(224, GetScreenWidth()-x-12);
            Rectangle notice = {x, button.y + (button.height-54)/2, width, 54};
            DrawRectangleRounded(notice, .35f, 8, Fade(BLACK, .65f));
            DrawGameTextIn(hint, (Rectangle){notice.x+8,notice.y+4,notice.width-16,notice.height-8},
                           18, RAYWHITE, GAME_TEXT_BODY);
        }
        DrawGameTextIn(InputHint("L: Ranking de speedrun", "[Share]: ranking de speedrun"), (Rectangle){16, GetScreenHeight()-48, 230, 30},
                       18, buttonText, GAME_TEXT_BODY);
        if (UsingGamepad())
            DrawGameTextIn(InputHint("", "Direcional: navegar  ·  [Cruz]: confirmar"),
                           (Rectangle){16, GetScreenHeight()-79, 300, 30},
                           18, buttonText, GAME_TEXT_BODY);
        DrawMainMenuShortcut("Capela Invertida", TestInvertedChapelButtonBounds());
        DrawMainMenuShortcut("Capela", TestChapelButtonBounds());
        DrawMainMenuShortcut("Teste", TestButtonBounds());
        return;
    }
    if (m->page==1) {
        DrawSettings(m, PauseCanvas());
        return;
    }
    if (m->page==2) {
        static const char *names[4] = {"Arthur", "Marcos", "Silvio", "João Paulo"};
        float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
        float pw = fminf(920.0f, sw-24.0f), ph = fminf(630.0f, sh-24.0f);
        Rectangle panel = {(sw-pw)/2, (sh-ph)/2, pw, ph};
        DrawMenuPanel(panel);
        float titleH = fminf(54.0f, ph*0.13f);
        DrawGameTextIn("Créditos", (Rectangle){panel.x+12,panel.y+8,pw-24,titleH},
                       (int)fminf(42.0f,titleH*.8f),buttonText,GAME_TEXT_TITLE);
        int columns = pw >= 720.0f ? 4 : 2;
        int rows = 4/columns;
        float gap = fminf(18.0f,pw*.025f);
        float margin = fminf(36.0f,pw*.05f);
        float top = panel.y+titleH+20.0f;
        float bottom = panel.y+ph-72.0f;
        float cardW = (pw-2*margin-(columns-1)*gap)/columns;
        float cardH = (bottom-top-(rows-1)*gap)/rows;
        for (int i=0;i<4;i++) {
            Rectangle card = {panel.x+margin+(i%columns)*(cardW+gap),
                              top+(i/columns)*(cardH+gap),cardW,cardH};
            DrawRectangleRounded(card,.08f,8,Fade(BLACK,.66f));
            DrawRectangleRoundedLinesEx(card,.08f,8,2,Fade(GOLD,.72f));
            float labelH = fminf(38.0f,cardH*.23f);
            Rectangle photo = {card.x+5,card.y+5,card.width-10,card.height-labelH-8};
            Texture2D t = m->creditPhotos[i];
            if (t.id) {
                float sourceAspect = (float)t.width/t.height;
                float targetAspect = photo.width/photo.height;
                Rectangle src = {0,0,(float)t.width,(float)t.height};
                if (sourceAspect > targetAspect) {
                    src.width = t.height*targetAspect;
                    src.x = (t.width-src.width)/2;
                } else {
                    src.height = t.width/targetAspect;
                    src.y = (t.height-src.height)/2;
                }
                DrawTexturePro(t,src,photo,(Vector2){0,0},0,WHITE);
            }
            DrawGameTextIn(names[i],(Rectangle){card.x+4,card.y+card.height-labelH-2,
                           card.width-8,labelH},(int)fminf(26.0f,labelH*.8f),RAYWHITE,GAME_TEXT_BODY);
        }
        DrawGameTextIn("Áudio · TheNikonProductions, falcospizaetus e Merrick079 (Freesound)",
                       (Rectangle){panel.x+12,bottom+4,pw-24,22},
                       (int)fminf(17.0f,pw/35.0f),LIGHTGRAY,GAME_TEXT_BODY);
        DrawGameTextIn(InputHint("Esc: voltar", "[Círculo]: voltar"),(Rectangle){panel.x+12,panel.y+ph-37,pw-24,30},
                       20,LIGHTGRAY,GAME_TEXT_BODY);
        return;
    }
    Rectangle p = {GetScreenWidth()/2.0f-330, GetScreenHeight()/2.0f-140,660,280};
    DrawMenuPanel(p);
    DrawGameTextEx("Nova Partida", (Vector2){p.x+50,p.y+34},
                   42, buttonText, GAME_TEXT_TITLE);
    DrawGameText("Substituir o progresso salvo?",(int)p.x+50,(int)p.y+105,25,RAYWHITE);
    DrawGameText(InputHint("Enter: confirmar", "[Cruz]: confirmar"),(int)p.x+50,(int)p.y+155,22,RAYWHITE);
    DrawGameText(InputHint("Esc: voltar", "[Círculo]: voltar"),(int)p.x+50,(int)p.y+218,22,RAYWHITE);
}
