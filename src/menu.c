#include "menu.h"
#include <stdio.h>
#include <math.h>
#include "rlgl.h"

enum { PAUSE_SCENE = 0, PAUSE_PLATE = 2, PAUSE_LABEL = 3, PAUSE_FRAME = 4 };

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

static Rectangle ButtonBounds(int i) {
    float w = fminf(GetScreenWidth() * 0.32f, 530.0f);
    float groupHeight = 4*w*.30f + 3*fmaxf(8.0f, w*.30f*.04f);
    float centerY = GetScreenHeight()*.54f - groupHeight*.04f;
    return MenuButtonBounds(i, GetScreenWidth()/2.0f, centerY, w);
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
    char data[256];
    snprintf(data,sizeof(data),"PIF-RUN-5 %d %d %d %d %.6f %.6f %.6f %d %d %d %d %d\n",
        p->weapon,p->stage,p->health,p->maxHealth,p->damage,p->speed,p->bossPower,
        p->weaponLevel[0],p->weaponLevel[1],p->weaponLevel[2],p->strongWeapon,p->map);
    return SaveFileText(SavePath(), data);
}

bool LoadClearingCheckpoint(RunProgress *p) {
    if (!FileExists(SavePath())) return false;
    char *data = LoadFileText(SavePath());
    if (!data) return false;
    RunProgress loaded={0};
    int fields=sscanf(data,"PIF-RUN-5 %d %d %d %d %f %f %f %d %d %d %d %d",
        &loaded.weapon,&loaded.stage,&loaded.health,&loaded.maxHealth,&loaded.damage,
        &loaded.speed,&loaded.bossPower,&loaded.weaponLevel[0],&loaded.weaponLevel[1],
        &loaded.weaponLevel[2],&loaded.strongWeapon,&loaded.map);
    if (fields==12) fields=11;
    else fields=sscanf(data,"PIF-CLEARING-4 %d %d %d %d %f %f %f %d %d %d %d",
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
    UnloadFileText(data);
    bool valid=loaded.map>=0 && loaded.map<=1 && loaded.weapon>=0 && loaded.weapon<3 && loaded.stage>=1 && loaded.stage<=CLEARING_COMPLETE_STAGE &&
        loaded.health>0 && loaded.health<=loaded.maxHealth && loaded.maxHealth<=10000 &&
        isfinite(loaded.damage) && loaded.damage>=1 && loaded.damage<=100 &&
        isfinite(loaded.speed) && loaded.speed>=100 && loaded.speed<=1000 &&
        isfinite(loaded.bossPower) && loaded.bossPower>=1 && loaded.bossPower<=100 &&
        loaded.strongWeapon>=-1 && loaded.strongWeapon<3;
    for (int i=0;i<3;i++) if (loaded.weaponLevel[i]<0 || loaded.weaponLevel[i]>3) valid=false;
    if (valid) *p=loaded;
    return valid;
}

int LoadMenuCheckpointWeapon(void) {
    RunProgress progress;
    return LoadClearingCheckpoint(&progress) ? progress.weapon : -1;
}

bool InitMenu(Menu *m) {
    *m = (Menu){0};
    m->background = LoadTexture("assets/menu_cenario.png");
    m->moon = LoadTexture("assets/menu_lua.png");
    m->seal = LoadTexture("assets/menu_selo.png");
    m->necromancer = LoadTexture("assets/menu_necromante.png");
    m->fog = LoadTexture("assets/menu_nevoa.png");
    m->fogTexture = LoadTexture("assets/menu_nevoa_textura.png");
    for (int i=0; i<10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/menu_capa_%02d.png", i);
        m->hero[i] = LoadTexture(path);
    }
    const char *paths[4] = {"assets/menu_iniciar.png", "assets/menu_continuar.png",
                            "assets/menu_config.png", "assets/menu_creditos.png"};
    if (!m->background.id || !m->moon.id || !m->seal.id || !m->necromancer.id || !m->fog.id || !m->fogTexture.id) {
        UnloadMenu(m); return false;
    }
    for (int i=0; i<10; i++) if (!m->hero[i].id) { UnloadMenu(m); return false; }
    SetTextureFilter(m->background, TEXTURE_FILTER_POINT);
    SetTextureFilter(m->moon, TEXTURE_FILTER_POINT);
    SetTextureFilter(m->seal, TEXTURE_FILTER_POINT);
    SetTextureFilter(m->necromancer, TEXTURE_FILTER_POINT);
    SetTextureFilter(m->fog, TEXTURE_FILTER_POINT);
    SetTextureFilter(m->fogTexture, TEXTURE_FILTER_BILINEAR);
    for (int i=0; i<10; i++) SetTextureFilter(m->hero[i], TEXTURE_FILTER_POINT);
    for (int i=0; i<4; i++) {
        m->buttons[i] = LoadTexture(paths[i]);
        if (!m->buttons[i].id) { UnloadMenu(m); return false; }
        SetTextureFilter(m->buttons[i], TEXTURE_FILTER_POINT);
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
    int width, height;
    m->video = OpenMenuVideo("assets/videos/Menu_Principal.mp4", &width, &height, &m->videoInterval);
    if (m->video) {
        const unsigned char *pixels = ReadMenuVideo(m->video);
        if (pixels) {
            Image firstFrame = {(void *)pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            m->videoFrame = LoadTextureFromImage(firstFrame);
        }
        if (m->videoFrame.id) SetTextureFilter(m->videoFrame, TEXTURE_FILTER_BILINEAR);
        else {
            CloseMenuVideo(m->video);
            m->video = NULL;
        }
    }
    if (!m->video) TraceLog(LOG_WARNING, "Video do menu indisponivel; usando o fundo animado");
    return true;
}

void UnloadMenu(Menu *m) {
    if (m->pause.shader.id) UnloadShader(m->pause.shader);
    if (m->pause.labels.id) UnloadTexture(m->pause.labels);
    if (m->pause.frame.id) UnloadTexture(m->pause.frame);
    if (m->pause.background.id) UnloadTexture(m->pause.background);
    if (m->videoFrame.id) UnloadTexture(m->videoFrame);
    CloseMenuVideo(m->video);
    for (int i=0; i<4; i++) if (m->buttons[i].id) UnloadTexture(m->buttons[i]);
    for (int i=0; i<10; i++) if (m->hero[i].id) UnloadTexture(m->hero[i]);
    if (m->fog.id) UnloadTexture(m->fog);
    if (m->fogTexture.id) UnloadTexture(m->fogTexture);
    if (m->necromancer.id) UnloadTexture(m->necromancer);
    if (m->seal.id) UnloadTexture(m->seal);
    if (m->moon.id) UnloadTexture(m->moon);
    if (m->background.id) UnloadTexture(m->background);
    *m = (Menu){0};
}

static Rectangle PauseButtonBounds(int index) {
    return MenuButtonBounds(index, 836.0f, 543.0f, 370.0f);
}
static const Rectangle pauseFullscreen = {626, 403, 420, 126};
static const Rectangle pauseBack = {696, 710, 280, 84};

static Rectangle PauseCanvas(void) {
    float scale = fminf(GetScreenWidth()/1672.0f, GetScreenHeight()/941.0f);
    float width = 1672 * scale, height = 941 * scale;
    return (Rectangle){(GetScreenWidth()-width)/2, (GetScreenHeight()-height)/2, width, height};
}

PauseAction UpdatePauseMenu(PauseMenu *pause) {
    float dt = GetFrameTime();
    pause->time += dt;
    Rectangle canvas = PauseCanvas();
    float scale = canvas.width / 1672.0f;
    Vector2 mouse = GetMousePosition();
    mouse = (Vector2){(mouse.x-canvas.x)/scale, (mouse.y-canvas.y)/scale};
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool escape = IsKeyPressed(KEY_ESCAPE);
    if (pause->page != PAUSE_OPTIONS) {
        if (escape || IsKeyPressed(KEY_BACKSPACE) || (click && CheckCollisionPointRec(mouse, pauseBack))) {
            pause->page = PAUSE_OPTIONS;
        } else if (pause->page == PAUSE_SETTINGS &&
                   (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ENTER) ||
                    (click && CheckCollisionPointRec(mouse, pauseFullscreen)))) {
            ToggleFullscreen();
        }
        return PAUSE_NONE;
    }
    if (escape) return PAUSE_RESUME;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) pause->selected = (pause->selected + 3) % 4;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) pause->selected = (pause->selected + 1) % 4;
    Vector2 movement = GetMouseDelta();
    if (click || movement.x != 0 || movement.y != 0) {
        for (int i = 0; i < 4; i++)
            if (CheckCollisionPointRec(mouse, PauseButtonBounds(i))) pause->selected = i;
    }
    UpdateButtonFocus(pause->buttonFocus, pause->selected, dt);
    if (click ? !CheckCollisionPointRec(mouse, PauseButtonBounds(pause->selected)) :
                !IsKeyPressed(KEY_ENTER) && !IsKeyPressed(KEY_SPACE)) return PAUSE_NONE;
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
    DrawText(label, (int)(canvas.x + (canvas.width - MeasureText(label, fontSize))/2),
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

static void DrawPauseButton(const Menu *menu, Rectangle canvas, int index) {
    const PauseMenu *pause = &menu->pause;
    Rectangle logical = PauseButtonBounds(index);
    Rectangle bounds = PauseRect(canvas, logical);
    float focus = pause->buttonFocus[index];
    Color tint = MenuButtonTint(bounds, focus, false);

    if (index < 2) {
        Texture2D button = menu->buttons[index+1];
        DrawTexturePro(button, (Rectangle){0, 0, button.width, button.height},
                       bounds, (Vector2){0}, 0, tint);
        return;
    }

    DrawPausePlate(menu, bounds, tint);
    if (!pause->shader.id || !pause->labels.id) {
        DrawPauseText(canvas, index == 2 ? "Controles" : "Voltar ao Menu", logical.y + 39, 34, GOLD);
        return;
    }

    static const Rectangle icons[] = {{651, 402, 128, 82}, {662, 560, 112, 92}};
    static const Rectangle labels[] = {{880, 404, 319, 78}, {838, 558, 481, 84}};
    Rectangle icon = icons[index-2], label = labels[index-2];
    float labelHeight = fminf(bounds.height*.35f, bounds.width*.50f*label.height/label.width);
    float iconHeight = fminf(bounds.height*.37f, bounds.width*.11f*icon.height/icon.width);
    DrawPauseLayer(pause, PAUSE_LABEL, pause->labels, icon,
                   (Rectangle){bounds.x + bounds.width*.21f - icon.width*iconHeight/icon.height/2,
                               bounds.y + (bounds.height-iconHeight)/2,
                               icon.width*iconHeight/icon.height, iconHeight}, tint);
    DrawRectangleRec((Rectangle){bounds.x + bounds.width*.304f, bounds.y + bounds.height*.34f,
                                 bounds.width*.005f, bounds.height*.30f}, Fade(GOLD, .62f));
    float labelWidth = label.width*labelHeight/label.height;
    DrawPauseLayer(pause, PAUSE_LABEL, pause->labels, label,
                   (Rectangle){bounds.x + bounds.width*.618f - labelWidth/2,
                               bounds.y + (bounds.height-labelHeight)/2, labelWidth, labelHeight}, tint);
}

static void DrawPauseTextButton(const Menu *menu, Rectangle canvas, Rectangle logical,
                                const char *label, int fontSize) {
    Rectangle bounds = PauseRect(canvas, logical);
    float focus = CheckCollisionPointRec(GetMousePosition(), bounds) ? 1.0f : 0.0f;
    DrawPausePlate(menu, bounds, MenuButtonTint(bounds, focus, false));
    DrawPauseText(canvas, label, logical.y + (logical.height-fontSize)/2, fontSize, RAYWHITE);
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
        } else DrawPauseText(canvas, "Pausa", 170, 82, GOLD);
        for (int i = 0; i < 4; i++) DrawPauseButton(menu, canvas, i);
        return;
    }

    Rectangle panel = PauseRect(canvas, (Rectangle){395, 148, 882, 668});
    DrawMenuPanel(panel);
    DrawPauseTextButton(menu, canvas, pauseBack, "Voltar  ·  Esc", 28);
    if (pause->page == PAUSE_SETTINGS) {
        DrawPauseText(canvas, "Configurações", 211, 60, GOLD);
        DrawPauseTextButton(menu, canvas, pauseFullscreen,
                            IsWindowFullscreen() ? "Tela cheia ligada" : "Tela cheia desligada", 34);
        DrawPauseText(canvas, "F ou Enter alterna o modo de tela", 542, 30, RAYWHITE);
    } else {
        DrawPauseText(canvas, "Controles", 211, 60, GOLD);
        DrawPauseText(canvas, "WASD  ·  Mover", 331, 37, RAYWHITE);
        DrawPauseText(canvas, "Espaço  ·  Esquivar com qualquer arma", 400, 37, RAYWHITE);
        DrawPauseText(canvas, "Clique esquerdo  ·  Atacar com katana ou arma de fogo", 469, 30, RAYWHITE);
        DrawPauseText(canvas, "Clique direito  ·  Acelerar a Bíblia ou esquivar", 531, 34, RAYWHITE);
        DrawPauseText(canvas, "E  ·  Interagir e selar", 600, 37, RAYWHITE);
    }
}

MenuAction UpdateMenu(Menu *m) {
    float dt = GetFrameTime();
    m->effectTime += dt;
    if (m->video) {
        m->videoElapsed = fmin(m->videoElapsed + dt, 3 * m->videoInterval);
        const unsigned char *pixels = NULL;
        while (m->videoElapsed >= m->videoInterval) {
            m->videoElapsed -= m->videoInterval;
            pixels = ReadMenuVideo(m->video);
            if (!pixels) {
                TraceLog(LOG_WARNING, "Falha ao ler o video do menu; usando o fundo animado");
                CloseMenuVideo(m->video);
                m->video = NULL;
                break;
            }
        }
        if (pixels) UpdateTexture(m->videoFrame, pixels);
    }
    m->hasSave = LoadMenuCheckpointWeapon() >= 0;
    if (m->page == 3) {
        if (IsKeyPressed(KEY_ESCAPE)) m->page = 0;
        if (IsKeyPressed(KEY_ENTER)) { m->page = 0; return MENU_START; }
        return MENU_NONE;
    }
    if (m->page == 1) {
        if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ENTER)) ToggleFullscreen();
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) m->page = 0;
        return MENU_NONE;
    }
    if (m->page == 2) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ENTER)) m->page = 0;
        return MENU_NONE;
    }
    if (IsKeyPressed(KEY_ESCAPE)) return MENU_QUIT;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) m->selected = (m->selected+1)%4;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) m->selected = (m->selected+3)%4;
    Vector2 mouse = GetMousePosition();
    Vector2 mouseDelta = GetMouseDelta();
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
        for (int i=0; i<4; i++) if (CheckCollisionPointRec(mouse, ButtonBounds(i))) m->selected = i;
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    UpdateButtonFocus(m->buttonFocus, m->selected == 1 && !m->hasSave ? -1 : m->selected, dt);
    if (!click && !IsKeyPressed(KEY_ENTER) && !IsKeyPressed(KEY_SPACE)) return MENU_NONE;
    if (click && !CheckCollisionPointRec(mouse, ButtonBounds(m->selected))) return MENU_NONE;
    switch (m->selected) {
        case 0: if (m->hasSave) { m->page = 3; return MENU_NONE; } return MENU_START;
        case 1: return m->hasSave ? MENU_CONTINUE : MENU_NONE;
        case 2: m->page = 1; return MENU_NONE;
        default: m->page = 2; return MENU_NONE;
    }
}

static void DrawMenuBackground(const Menu *m) {
    Texture2D background = m->video ? m->videoFrame : m->background;
    float s = fmaxf((float)GetScreenWidth()/background.width,
                    (float)GetScreenHeight()/background.height);
    Rectangle dst = {(GetScreenWidth()-background.width*s)/2,
                     (GetScreenHeight()-background.height*s)/2,
                     background.width*s, background.height*s};
    DrawTexturePro(background, (Rectangle){0,0,(float)background.width,(float)background.height},
                   dst, (Vector2){0}, 0, WHITE);
    if (m->video) return;
    float t = m->effectTime;
    float pulse = 1.0f + 0.025f*sinf(t*1.5f);
    Rectangle moon = {dst.x+(1230.0f-235.0f*pulse)*s, dst.y+(155.0f-235.0f*pulse)*s,
                      470.0f*pulse*s, 470.0f*pulse*s};
    DrawTexturePro(m->moon, (Rectangle){0,0,(float)m->moon.width,(float)m->moon.height},
                   moon, (Vector2){0}, 0, Fade(WHITE,0.90f));
    Rectangle seal = {dst.x+1200*s,dst.y+275*s,330*s,330*s};
    DrawTexturePro(m->seal, (Rectangle){0,0,(float)m->seal.width,(float)m->seal.height},
                   seal, (Vector2){165*s,165*s}, t*10.0f, Fade(WHITE,0.72f));
    Rectangle necro = {dst.x+1020*s,dst.y+225*s,390*s,260*s};
    DrawTexturePro(m->necromancer,(Rectangle){0,0,(float)m->necromancer.width,(float)m->necromancer.height},
                   necro,(Vector2){0},0,WHITE);
    static const int capeLoop[] = {0,1,2,3,4,5,6,7,8,9,8,7,6,5,4,3,2,1};
    int frame = capeLoop[((int)(t*10.0f))%(int)(sizeof(capeLoop)/sizeof(capeLoop[0]))];
    Texture2D hero = m->hero[frame];
    Rectangle protagonist = {dst.x+50*s,dst.y+345*s,620*s,568*s};
    DrawTexturePro(hero,(Rectangle){0,0,(float)hero.width,(float)hero.height},
                   protagonist,(Vector2){0},0,WHITE);
    float drift = fmodf(t*19.0f, 160.0f);
    for (int i=0; i<2; i++) {
        Rectangle fog = {dst.x+(-120.0f+drift+i*1450.0f)*s,dst.y+520*s,1672*s,420*s};
        DrawTexturePro(m->fog,(Rectangle){0,360,(float)m->fog.width,580},
                       fog,(Vector2){0},0,Fade(WHITE,0.38f));
    }
    Rectangle distantFog = {dst.x-45*s+fmodf(t*7.0f,90.0f)*s,dst.y+120*s,
                            dst.width,dst.height};
    DrawTexturePro(m->fogTexture,(Rectangle){0,0,(float)m->fogTexture.width,(float)m->fogTexture.height},
                   distantFog,(Vector2){0},0,Fade(WHITE,0.22f));
    for (int i=0; i<48; i++) {
        float phase = (float)i*37.7f;
        float x = fmodf(phase*13.0f+17.0f*sinf(t*0.4f+phase),1672.0f);
        float y = fmodf(phase*29.0f-t*(8.0f+(i%5)*3.0f)+9400.0f,941.0f);
        float radius = (i%7==0 ? 2.2f : 1.2f)*s;
        DrawCircleV((Vector2){dst.x+x*s,dst.y+y*s},radius,
                    Fade((Color){255,64,78,255},0.35f+0.25f*sinf(t*1.3f+phase)*sinf(t*1.3f+phase)));
    }
}

void DrawMenu(const Menu *m) {
    DrawMenuBackground(m);
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.25f));
    if (m->page == 0) {
        for (int i=0; i<4; i++) {
            Rectangle b = ButtonBounds(i);
            bool disabled = i==1 && !m->hasSave;
            DrawTexturePro(m->buttons[i], (Rectangle){0,0,(float)m->buttons[i].width,
                           (float)m->buttons[i].height}, b, (Vector2){0}, 0,
                           MenuButtonTint(b, m->buttonFocus[i], disabled));
        }
        if (!m->hasSave) DrawText("Continuar disponivel apos iniciar uma partida",
                                  GetScreenWidth()/2-210,GetScreenHeight()-42,18,RAYWHITE);
        return;
    }
    Rectangle p = {GetScreenWidth()/2.0f-330, GetScreenHeight()/2.0f-140,660,280};
    DrawMenuPanel(p);
    if (m->page==1) {
        DrawText("CONFIGURACOES",(int)p.x+50,(int)p.y+34,34,GOLD);
        DrawText("F ou Enter: alternar tela cheia",(int)p.x+50,(int)p.y+110,23,RAYWHITE);
    } else if (m->page==2) {
        DrawText("CREDITOS",(int)p.x+50,(int)p.y+34,34,GOLD);
        DrawText("Jogo PIF",(int)p.x+50,(int)p.y+110,26,RAYWHITE);
    } else {
        DrawText("NOVA PARTIDA",(int)p.x+50,(int)p.y+34,34,GOLD);
        DrawText("Substituir o progresso salvo?",(int)p.x+50,(int)p.y+105,25,RAYWHITE);
        DrawText("Enter: confirmar",(int)p.x+50,(int)p.y+155,22,RAYWHITE);
    }
    DrawText("Esc: voltar",(int)p.x+50,(int)p.y+218,22,RAYWHITE);
}
