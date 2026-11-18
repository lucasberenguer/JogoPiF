#include "dev_tools.h"
#include "game_text.h"
#include <math.h>
#include <stdio.h>

static Rectangle Panel(void) {
    float scale=fminf(GetScreenWidth()/1180.0f,GetScreenHeight()/700.0f);
    return (Rectangle){(GetScreenWidth()-1120*scale)/2,(GetScreenHeight()-660*scale)/2,
                       1120*scale,660*scale};
}

static Rectangle Place(Rectangle logical) {
    Rectangle panel=Panel();
    float scale=panel.width/1120;
    return (Rectangle){panel.x+logical.x*scale,panel.y+logical.y*scale,
                       logical.width*scale,logical.height*scale};
}

static Rectangle Control(int index) {
    if (index<6) return Place((Rectangle){28+(index%2)*546,110+(index/2)*62,518,48});
    if (index==17) return Place((Rectangle){862,598,230,42});
    int button=index-6;
    return Place((Rectangle){28+(button%3)*364,306+(button/3)*56,336,48});
}

static void AdjustField(DevTools *tools, int index, int direction) {
    switch (index) {
        case 0: tools->map=1-tools->map; break;
        case 1: tools->wave=1+(tools->wave-1+direction+5)%5; break;
        case 2: tools->weapon=(tools->weapon+direction+3)%3; break;
        case 3: tools->weaponLevel=(tools->weaponLevel+direction+4)%4; break;
        case 4: tools->invincible=!tools->invincible; break;
        case 5: tools->collision=!tools->collision; break;
    }
}

DevAction UpdateDevTools(DevTools *tools, bool inPractice) {
    if (IsKeyPressed(KEY_ESCAPE)) return DEV_CLOSE;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)) tools->selected=(tools->selected+1)%18;
    if (IsKeyPressed(KEY_UP)) tools->selected=(tools->selected+17)%18;
    int direction=(int)IsKeyPressed(KEY_RIGHT)-(int)IsKeyPressed(KEY_LEFT);
    if (direction) {
        if (tools->selected<6) AdjustField(tools,tools->selected,direction);
        else tools->selected=(tools->selected+direction+18)%18;
    }
    Vector2 mouse=GetMousePosition(), motion=GetMouseDelta();
    if (motion.x!=0 || motion.y!=0)
        for (int i=0; i<18; i++) if (CheckCollisionPointRec(mouse,Control(i))) tools->selected=i;
    bool activate=IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        activate=false;
        for (int i=0; i<18; i++) if (CheckCollisionPointRec(mouse,Control(i))) {
            tools->selected=i;
            activate=true;
        }
        if (activate && tools->selected<6) {
            Rectangle field=Control(tools->selected);
            AdjustField(tools,tools->selected,mouse.x<field.x+field.width*.65f ? -1 : 1);
            return DEV_NONE;
        }
    }
    if (!activate) return DEV_NONE;
    if (tools->selected<6) {
        AdjustField(tools,tools->selected,1);
        return DEV_NONE;
    }
    if (!inPractice && (tools->selected==12 || tools->selected==13)) return DEV_NONE;
    static const DevAction actions[]={DEV_STAGE,DEV_BOSS_ONE,DEV_BOSS_TWO,DEV_OPENING,DEV_GUARDIAN,
        DEV_SEAL,DEV_HEAL,DEV_CLEAR_STAGE,DEV_LIBERATION,DEV_PASSAGE,DEV_MENU,DEV_CLOSE};
    return actions[tools->selected-6];
}

void DrawDevTools(const DevTools *tools, bool inPractice) {
    const Color text={250,232,207,255}, gold={231,182,112,255};
    Rectangle panel=Panel();
    float scale=panel.width/1120;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){13,8,15,255});
    DrawRectangleRounded(panel,.025f,8,(Color){33,13,27,255});
    DrawRectangleRoundedLinesEx(panel,.025f,8,1,gold);
    DrawGameTextIn("Ferramentas de desenvolvimento",Place((Rectangle){28,16,1064,45}),
                   34*scale,text,false);
    DrawGameTextIn("Escolha o cenário, a arma e o ponto da luta",Place((Rectangle){28,64,1064,30}),
                   23*scale,gold,false);
    static const char *names[]={"Mapa","Onda","Arma","Upgrade da arma","Invulnerabilidade","Colisões"};
    const char *weapons[]={"Arma de fogo","Bíblia","Katana"};
    char waveText[24],levelText[24];
    snprintf(waveText,sizeof(waveText),"%d / 4",tools->wave);
    snprintf(levelText,sizeof(levelText),"%d / 3",tools->weaponLevel);
    const char *values[]={tools->map ? "Cemitério" : "Clareira",
        tools->wave==5 ? "5 · Boss" : waveText,weapons[tools->weapon],
        levelText,tools->invincible ? "Ligada" : "Desligada",
        tools->collision ? "Visíveis" : "Ocultas"};
    for (int i=0; i<6; i++) {
        Rectangle field=Control(i);
        bool focused=tools->selected==i;
        DrawRectangleRec(field,focused ? (Color){72,32,43,255} : (Color){23,13,25,255});
        DrawRectangleLinesEx(field,1,focused ? gold : (Color){92,55,58,255});
        Rectangle label={field.x+12*scale,field.y,field.width*.43f,field.height};
        Rectangle value={field.x+field.width*.48f,field.y,field.width*.46f,field.height};
        DrawGameTextIn(names[i],label,fmaxf(17,23*scale),text,false);
        DrawGameTextIn(TextFormat("<  %s  >",values[i]),value,fmaxf(17,24*scale),gold,false);
    }
    static const char *labels[]={"Iniciar onda selecionada","Boss · fase 1","Boss · fase 2",
        "Abertura em HQ","Cutscene do Guardião","Testar selo do boss",
        "Curar e recarregar","Encerrar onda atual","Libertação do Guardião","Altar e passagem",
        "Menu principal","Fechar · F1 / Esc"};
    for (int i=6; i<18; i++) {
        Rectangle button=Control(i);
        bool enabled=inPractice || (i!=12 && i!=13);
        bool focused=tools->selected==i;
        Color ink=enabled ? text : (Color){155,134,141,255};
        DrawRectangleRec(button,focused && enabled ? (Color){114,37,46,255} : (Color){49,22,34,255});
        DrawRectangleLinesEx(button,focused ? 2 : 1,focused ? gold : (Color){116,65,62,255});
        DrawGameTextIn(labels[i-6],(Rectangle){button.x+8*scale,button.y,button.width-16*scale,button.height},
                       fmaxf(18,25*scale),ink,false);
    }
    const char *status=tools->status[0] ? tools->status :
        "Os cenários usam prática. Continuar no menu retoma seu checkpoint.";
    DrawGameTextIn(status,Place((Rectangle){28,536,1064,45}),fmaxf(17,22*scale),gold,false);
    DrawGameTextIn("Setas ajustam e navegam · Enter executa · Clique para selecionar",
        Place((Rectangle){28,598,800,42}),fmaxf(16,21*scale),text,false);
}

bool StartDevEncounter(const DevTools *tools, DevAction action, Encounter *e,
                       EnemyGroup *enemies, Clearing *map, Vector2 *player, int *health) {
    if (action!=DEV_STAGE && action!=DEV_BOSS_ONE && action!=DEV_BOSS_TWO &&
        action!=DEV_GUARDIAN && action!=DEV_SEAL && action!=DEV_LIBERATION && action!=DEV_PASSAGE) return false;
    if (tools->map<0 || tools->map>1 || tools->wave<1 || tools->wave>5 ||
        tools->weapon<0 || tools->weapon>2 || tools->weaponLevel<0 || tools->weaponLevel>3) return false;
    bool aftermath=action==DEV_LIBERATION || action==DEV_PASSAGE;
    bool cemetery=action==DEV_GUARDIAN || aftermath ? false : tools->map==1;
    if (map->cemetery!=cemetery || map->village || map->chapel) {
        Clearing next={0};
        if (!(cemetery ? InitCemetery(&next) : InitClearing(&next))) return false;
        UnloadClearing(map);
        *map=next;
    }
    RunProgress progress=NewRunProgress(tools->weapon);
    progress.stage=action==DEV_STAGE ? tools->wave : CLEARING_BOSS_STAGE;
    progress.weaponLevel[tools->weapon]=tools->weaponLevel;
    progress.guardianIntroSeen=action!=DEV_GUARDIAN;
    if (aftermath) {
        progress.stage=CLEARING_COMPLETE_STAGE;
        progress.guardianAftermath=action==DEV_LIBERATION ? GUARDIAN_LIBERATION_PENDING : GUARDIAN_PASSAGE_PENDING;
    }
    *player=map->spawn;
    *health=progress.maxHealth;
    BeginEncounter(e,enemies,map,progress,*player);
    e->practice=true;
    e->presentation=(StagePresentation){0};
    if (action==DEV_BOSS_TWO) {
        Enemy *boss=&enemies->list[0];
        boss->guardianPhase=GUARDIAN_PHASE_TWO;
        boss->health=boss->maxHealth/2;
        boss->state=ENEMY_IDLE;
        boss->stateTime=0;
    } else if (action==DEV_SEAL) {
        Enemy *boss=&enemies->list[0];
        boss->guardianPhase=GUARDIAN_PHASE_TWO;
        boss->health=0;
        boss->state=ENEMY_FALLEN;
        boss->stateTime=0;
        *player=boss->position;
        OpenEncounterSeal(e,enemies,*player);
    }
    return true;
}
