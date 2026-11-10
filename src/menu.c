#include "menu.h"
#include <stdio.h>
#include <math.h>

static const char *SavePath(void) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/partida_pif.sav", GetApplicationDirectory());
    return path;
}

static Rectangle ButtonBounds(int i) {
    if (i==4) {
        float width=fminf(310,GetScreenWidth()*.27f);
        return (Rectangle){GetScreenWidth()-width-20,GetScreenHeight()-72.0f,width,48};
    }
    float w = fminf(GetScreenWidth() * 0.32f, 530.0f);
    float h = w * 0.30f;
    float gap = fmaxf(8.0f, h * 0.04f);
    float top = (GetScreenHeight() - (4*h + 3*gap)) * 0.54f;
    return (Rectangle){(GetScreenWidth() - w)/2.0f, top + i*(h+gap), w, h};
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
    SetTextureFilter(m->fogTexture, TEXTURE_FILTER_POINT);
    for (int i=0; i<10; i++) SetTextureFilter(m->hero[i], TEXTURE_FILTER_POINT);
    for (int i=0; i<4; i++) {
        m->buttons[i] = LoadTexture(paths[i]);
        if (!m->buttons[i].id) { UnloadMenu(m); return false; }
        SetTextureFilter(m->buttons[i], TEXTURE_FILTER_POINT);
    }
    m->hasSave = LoadMenuCheckpointWeapon() >= 0;
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
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) m->selected = (m->selected+1)%5;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) m->selected = (m->selected+4)%5;
    Vector2 mouse = GetMousePosition();
    Vector2 mouseDelta = GetMouseDelta();
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
        for (int i=0; i<5; i++) if (CheckCollisionPointRec(mouse, ButtonBounds(i))) m->selected = i;
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (!click && !IsKeyPressed(KEY_ENTER) && !IsKeyPressed(KEY_SPACE)) return MENU_NONE;
    if (click && !CheckCollisionPointRec(mouse, ButtonBounds(m->selected))) return MENU_NONE;
    switch (m->selected) {
        case 0: if (m->hasSave) { m->page = 3; return MENU_NONE; } return MENU_START;
        case 1: return m->hasSave ? MENU_CONTINUE : MENU_NONE;
        case 2: m->page = 1; return MENU_NONE;
        case 4: return MENU_TEST_CEMETERY;
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
            if (i==m->selected && !disabled)
                DrawRectangleRounded((Rectangle){b.x+16,b.y+10,b.width-32,b.height-20},
                                     0.3f,8,Fade(GOLD,0.35f));
            DrawTexturePro(m->buttons[i], (Rectangle){0,0,(float)m->buttons[i].width,
                           (float)m->buttons[i].height}, b, (Vector2){0}, 0,
                           disabled ? Fade(WHITE,0.48f) : WHITE);
        }
        Rectangle test=ButtonBounds(4);
        DrawRectangleRounded(test,.15f,6,m->selected==4?(Color){96,52,112,255}:(Color){35,22,45,240});
        DrawRectangleRoundedLinesEx(test,.15f,6,2,m->selected==4?GOLD:GRAY);
        int font=(int)fminf(19,test.width/13);
        const char *label="TESTAR CEMITERIO";
        DrawText(label,(int)(test.x+(test.width-MeasureText(label,font))/2),(int)test.y+15,font,RAYWHITE);
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
