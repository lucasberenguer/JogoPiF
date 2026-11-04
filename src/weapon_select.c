#include "weapon_select.h"
#include <math.h>

static Rectangle CardBounds(int index) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float width = fminf(sw * 0.27f, sh * 0.46f * 1.5f);
    float height = width / 1.5f;
    float gap = fminf(sw * 0.015f, 24.0f);
    float left = (sw - 3.0f*width - 2.0f*gap)/2.0f;
    return (Rectangle){left + index*(width+gap), sh*0.53f-height/2.0f, width, height};
}

const char *WeaponName(Weapon weapon) {
    const char *names[] = {"Arma de Fogo", "Biblia Sagrada", "Katana"};
    return weapon >= 0 && weapon < WEAPON_COUNT ? names[weapon] : "Nenhuma";
}

bool InitWeaponSelect(WeaponSelect *s) {
    *s = (WeaponSelect){0};
    s->frame = LoadTexture("assets/selecao_arma_fundo.png");
    const char *paths[] = {"assets/arma_fogo_carta.png", "assets/biblia_carta.png",
                           "assets/katana_carta.png"};
    if (!s->frame.id) return false;
    for (int i=0; i<WEAPON_COUNT; i++) {
        s->cards[i] = LoadTexture(paths[i]);
        if (!s->cards[i].id) { UnloadWeaponSelect(s); return false; }
        SetTextureFilter(s->cards[i], TEXTURE_FILTER_BILINEAR);
    }
    SetTextureFilter(s->frame, TEXTURE_FILTER_BILINEAR);
    return true;
}

void UnloadWeaponSelect(WeaponSelect *s) {
    for (int i=0; i<WEAPON_COUNT; i++) if (s->cards[i].id) UnloadTexture(s->cards[i]);
    if (s->frame.id) UnloadTexture(s->frame);
    *s = (WeaponSelect){0};
}

int UpdateWeaponSelect(WeaponSelect *s) {
    if (IsKeyPressed(KEY_ESCAPE)) return -2;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) s->selected = (s->selected+1)%WEAPON_COUNT;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) s->selected = (s->selected+WEAPON_COUNT-1)%WEAPON_COUNT;
    Vector2 mouse = GetMousePosition();
    Vector2 delta = GetMouseDelta();
    if (delta.x != 0 || delta.y != 0)
        for (int i=0; i<WEAPON_COUNT; i++) if (CheckCollisionPointRec(mouse, CardBounds(i))) s->selected=i;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i=0; i<WEAPON_COUNT; i++) if (CheckCollisionPointRec(mouse, CardBounds(i))) return i;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) return s->selected;
    return -1;
}

void DrawWeaponSelect(const WeaponSelect *s) {
    DrawTexturePro(s->frame, (Rectangle){0,0,(float)s->frame.width,(float)s->frame.height},
                   (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
                   (Vector2){0},0,WHITE);
    for (int i=0; i<WEAPON_COUNT; i++) {
        Rectangle r = CardBounds(i);
        if (i==s->selected) {
            DrawRectangleRounded((Rectangle){r.x-7,r.y-7,r.width+14,r.height+14},
                                 0.05f,8,Fade(GOLD,0.75f));
        }
        DrawTexturePro(s->cards[i], (Rectangle){0,0,(float)s->cards[i].width,
                       (float)s->cards[i].height},r,(Vector2){0},0,WHITE);
    }
    const char *hint = "Clique na arma ou use as setas e Enter  |  Esc: voltar";
    int fontSize = GetScreenHeight() < 800 ? 18 : 22;
    DrawText(hint,(GetScreenWidth()-MeasureText(hint,fontSize))/2,
             GetScreenHeight()-fontSize-24,fontSize,RAYWHITE);
}
