#include "game_text.h"
#include "weapon_select.h"
#include <math.h>

static const Rectangle cardTitle = {.422f,.177f,.493f,.116f};
static const Rectangle cardDescription = {.422f,.351f,.505f,.204f};
static const Rectangle cardAbility = {.563f,.612f,.362f,.174f};
static const Rectangle selectTitle = {.286f,.057f,.431f,.079f};
static const Color parchment = {255, 234, 204, 255};

static Rectangle RelativeRect(Rectangle bounds, Rectangle fraction) {
    return (Rectangle){bounds.x + fraction.x*bounds.width, bounds.y + fraction.y*bounds.height,
                       fraction.width*bounds.width, fraction.height*bounds.height};
}

static Texture2D LoadWeaponArt(const char *path, const Rectangle *textAreas, int count, Color backing) {
    Image source = LoadImage(path);
    if (!source.data) return (Texture2D){0};
    Color *pixels = LoadImageColors(source);
    Texture2D texture = {0};
    if (pixels) {
        for (int area = 0; area < count; area++) {
            Rectangle r = RelativeRect((Rectangle){0,0,source.width,source.height}, textAreas[area]);
            int top = (int)r.y, bottom = (int)ceilf(r.y+r.height);
            for (int x = (int)r.x; x < r.x+r.width; x++) {
                Color above = pixels[(top-1)*source.width+x];
                Color below = pixels[bottom*source.width+x];
                if (above.g > 32) above = backing;
                if (below.g > 32) below = backing;
                // Reconstrói o fundo da inscrição com amostras de cima e de baixo da placa.
                for (int y = top; y < bottom; y++) {
                    Color *pixel = &pixels[y*source.width+x];
                    Color plate = ColorLerp(above, below, (float)(y-top+1)/(bottom-top+1));
                    float feather = fminf(1, fminf(y-top+1, bottom-y)/3.0f);
                    *pixel = ColorLerp(*pixel, plate, feather);
                }
            }
        }
        Image prepared = {pixels, source.width, source.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        texture = LoadTextureFromImage(prepared);
        UnloadImageColors(pixels);
    }
    UnloadImage(source);
    return texture;
}

static Rectangle CardBounds(int index) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float width = fminf(sw * 0.29f, sh * 0.46f * 1.5f);
    float height = width / 1.5f;
    float gap = fminf(sw * 0.015f, 24.0f);
    float left = (sw - 3.0f*width - 2.0f*gap)/2.0f;
    return (Rectangle){left + index*(width+gap), sh*0.53f-height/2.0f, width, height};
}

const char *WeaponName(Weapon weapon) {
    const char *names[] = {"Arma de Fogo", "Bíblia Sagrada", "Katana"};
    return weapon >= 0 && weapon < WEAPON_COUNT ? names[weapon] : "Nenhuma";
}

bool InitWeaponSelect(WeaponSelect *s) {
    *s = (WeaponSelect){0};
    // A divisão preserva a ponta dourada no centro da moldura acima do título.
    const Rectangle heading[] = {{.286f,.057f,.205f,.079f}, {.509f,.057f,.208f,.079f},
                                 {.491f,.067f,.018f,.069f}};
    const Rectangle copy[] = {cardTitle, cardDescription, cardAbility};
    s->frame = LoadWeaponArt("assets/selecao_arma_fundo.png", heading, 3, (Color){36,5,16,255});
    const char *paths[] = {"assets/arma_fogo_carta.png", "assets/biblia_carta.png",
                           "assets/katana_carta.png"};
    if (!s->frame.id) return false;
    for (int i=0; i<WEAPON_COUNT; i++) {
        s->cards[i] = LoadWeaponArt(paths[i], copy, 3, (Color){29,5,14,255});
        if (!s->cards[i].id) { UnloadWeaponSelect(s); return false; }
        SetTextureFilter(s->cards[i], TEXTURE_FILTER_POINT);
    }
    SetTextureFilter(s->frame, TEXTURE_FILTER_POINT);
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

static void DrawCardCopy(const char *const *lines, int lineCount, Rectangle area, float size) {
    float widest = 0;
    for (int line = 0; line < lineCount; line++)
        widest = fmaxf(widest, MeasureGameTextEx(lines[line], size, false).x);
    float height = MeasureGameTextEx(lines[0], size, false).y + (lineCount-1)*size;
    float scale = fminf(1, fminf(area.width/widest, area.height/height));
    size *= scale;
    float y = area.y + (area.height - height*scale)/2;
    for (int line = 0; line < lineCount; line++)
        DrawGameTextEx(lines[line], (Vector2){area.x, y + line*size}, size, parchment, false);
}

static void DrawCardText(Weapon weapon, Rectangle card) {
    static const char *const descriptions[][3] = {
        {"Arma confiável e precisa,", "com potência para atingir", "inimigos à distância."},
        {"Relicário de fé que dissipa", "a corrupção, purifica e", "fortalece a determinação."},
        {"Lâmina rápida e precisa,", "ideal para eliminar", "grupos grandes."}
    };
    static const char *const abilities[][3] = {
        {"Dispara projéteis", "que ferem à distância."},
        {"Invoca luz sagrada", "para purificar e repelir", "inimigos corrompidos."},
        {"Corta em arco à frente", "do personagem."}
    };
    Rectangle title = RelativeRect(card, cardTitle);
    Rectangle description = RelativeRect(card, (Rectangle){.428f,.351f,.498f,.225f});
    Rectangle ability = RelativeRect(card, (Rectangle){.569f,.612f,.358f,.182f});
    DrawGameTextIn(WeaponName(weapon), title, card.width*.084f, parchment, true);
    DrawCardCopy(descriptions[weapon], 3, description, card.width*.052f);
    DrawCardCopy(abilities[weapon], abilities[weapon][2] ? 3 : 2, ability, card.width*.048f);
}

void DrawWeaponSelect(const WeaponSelect *s) {
    Rectangle screen = {0, 0, GetScreenWidth(), GetScreenHeight()};
    DrawTexturePro(s->frame, (Rectangle){0,0,(float)s->frame.width,(float)s->frame.height},
                   screen, (Vector2){0},0,WHITE);
    Rectangle title = RelativeRect(screen, selectTitle);
    DrawGameTextIn("Selecione sua Arma", title, title.height*.96f, parchment, true);
    for (int i=0; i<WEAPON_COUNT; i++) {
        Rectangle r = CardBounds(i);
        if (i==s->selected) {
            DrawRectangleRounded((Rectangle){r.x-7,r.y-7,r.width+14,r.height+14},
                                 0.05f,8,Fade(GOLD,0.75f));
        }
        DrawTexturePro(s->cards[i], (Rectangle){0,0,(float)s->cards[i].width,
                       (float)s->cards[i].height},r,(Vector2){0},0,WHITE);
        DrawCardText((Weapon)i, r);
    }
    const char *hint = "Clique na arma ou use as setas e Enter  ·  Esc para voltar";
    Rectangle footer = RelativeRect(screen, (Rectangle){.12f,.795f,.76f,.055f});
    DrawGameTextIn(hint, footer, fmaxf(18, screen.height*.030f), parchment, false);
}
