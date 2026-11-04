#include "menu.h"
#include <stdio.h>
#include <math.h>

static const char *SavePath(void) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/partida_pif.sav", GetApplicationDirectory());
    return path;
}

static Rectangle ButtonBounds(int i) {
    float w = fminf(GetScreenWidth() * 0.32f, 530.0f);
    float h = w * 0.30f;
    float gap = fmaxf(8.0f, h * 0.04f);
    float top = (GetScreenHeight() - (4*h + 3*gap)) * 0.54f;
    return (Rectangle){(GetScreenWidth() - w)/2.0f, top + i*(h+gap), w, h};
}

bool SaveMenuCheckpoint(int weapon) {
    char data[64];
    if (weapon < 0 || weapon > 2) return false;
    snprintf(data, sizeof(data), "PIF-CLEARING-2 %d\n", weapon);
    return SaveFileText(SavePath(), data);
}

int LoadMenuCheckpointWeapon(void) {
    if (!FileExists(SavePath())) return -1;
    char *data = LoadFileText(SavePath());
    if (!data) return -1;
    int weapon = -1;
    if (sscanf(data, "PIF-CLEARING-2 %d", &weapon) != 1 || weapon < 0 || weapon > 2)
        weapon = -1;
    UnloadFileText(data);
    return weapon;
}

bool InitMenu(Menu *m) {
    *m = (Menu){0};
    m->background = LoadTexture("assets/menu_fundo.png");
    const char *paths[4] = {"assets/menu_iniciar.png", "assets/menu_continuar.png",
                            "assets/menu_config.png", "assets/menu_creditos.png"};
    if (!m->background.id) return false;
    for (int i=0; i<4; i++) {
        m->buttons[i] = LoadTexture(paths[i]);
        if (!m->buttons[i].id) { UnloadMenu(m); return false; }
        SetTextureFilter(m->buttons[i], TEXTURE_FILTER_BILINEAR);
    }
    m->hasSave = LoadMenuCheckpointWeapon() >= 0;
    return true;
}

void UnloadMenu(Menu *m) {
    for (int i=0; i<4; i++) if (m->buttons[i].id) UnloadTexture(m->buttons[i]);
    if (m->background.id) UnloadTexture(m->background);
    *m = (Menu){0};
}

MenuAction UpdateMenu(Menu *m) {
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
    if (!click && !IsKeyPressed(KEY_ENTER) && !IsKeyPressed(KEY_SPACE)) return MENU_NONE;
    if (click && !CheckCollisionPointRec(mouse, ButtonBounds(m->selected))) return MENU_NONE;
    switch (m->selected) {
        case 0: if (m->hasSave) { m->page = 3; return MENU_NONE; } return MENU_START;
        case 1: return m->hasSave ? MENU_CONTINUE : MENU_NONE;
        case 2: m->page = 1; return MENU_NONE;
        default: m->page = 2; return MENU_NONE;
    }
}

void DrawMenu(const Menu *m) {
    float s = fmaxf((float)GetScreenWidth()/m->background.width,
                    (float)GetScreenHeight()/m->background.height);
    Rectangle dst = {(GetScreenWidth()-m->background.width*s)/2,
                     (GetScreenHeight()-m->background.height*s)/2,
                     m->background.width*s, m->background.height*s};
    DrawTexturePro(m->background, (Rectangle){0,0,(float)m->background.width,(float)m->background.height},
                   dst, (Vector2){0}, 0, WHITE);
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.25f));
    if (m->page == 0) {
        for (int i=0; i<4; i++) {
            Rectangle b = ButtonBounds(i);
            bool disabled = i==1 && !m->hasSave;
            if (i==m->selected && !disabled)
                DrawRectangleRounded((Rectangle){b.x+16,b.y+10,b.width-32,b.height-20},
                                     0.3f,8,Fade(GOLD,0.35f));
            DrawTexturePro(m->buttons[i], (Rectangle){0,0,(float)m->buttons[i].width,
                           (float)m->buttons[i].height}, b, (Vector2){0}, 0,
                           disabled ? Fade(WHITE,0.48f) : WHITE);
        }
        if (!m->hasSave) DrawText("Continuar disponivel apos iniciar uma partida",
                                  GetScreenWidth()/2-210,GetScreenHeight()-42,18,RAYWHITE);
        return;
    }
    Rectangle p = {GetScreenWidth()/2.0f-330, GetScreenHeight()/2.0f-140,660,280};
    DrawRectangleRounded(p,0.08f,12,Fade((Color){31,5,20,255},0.96f));
    DrawRectangleRoundedLinesEx(p,0.08f,12,3,GOLD);
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
