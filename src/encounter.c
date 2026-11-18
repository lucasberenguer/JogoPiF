#include "game_text.h"
#include "encounter.h"
#include "menu.h"
#include "raymath.h"
#include <stdio.h>

enum { REWARD_DAMAGE, REWARD_HEALTH, REWARD_SPEED, REWARD_WEAPON, REWARD_SWAP };
static const struct {
    const char *expression;
    const char *choices[3]; /* correct first; the slot is shuffled */
} bossQuestions[] = {
    {"P e (Q ou nao Q)", {"P", "nao P", "Q"}},
    {"nao (P ou Q)", {"nao P e nao Q", "nao P ou nao Q", "P e Q"}},
    {"(P e Q) ou (nao P e Q)", {"Q", "P", "P ou Q"}},
    {"nao (P e Q)", {"nao P ou nao Q", "nao P e nao Q", "P ou Q"}},
};

static const char *WeaponTitle(int weapon) {
    return weapon == 0 ? "Arma de fogo" : weapon == 1 ? "Biblia" : "Katana";
}

static void StartStage(Encounter *e, EnemyGroup *enemies, const Clearing *clearing, Vector2 player) {
    if (e->progress.stage == CLEARING_COMPLETE_STAGE) {
        for (int i=0; i<ENEMY_COUNT; i++) enemies->list[i] = (Enemy){.state=ENEMY_GONE};
        ClearEnemyAttacks(enemies);
        e->phase = ENCOUNTER_CLEAR;
    } else {
        e->phase = ENCOUNTER_FIGHT;
        const char *areaTitle = clearing->chapel ? "Capela do Anjo"
            : (clearing->village ? "Vila Lascada"
            : (clearing->cemetery ? "Cemitério das Raízes" : "Clareira do Selo"));
        const char *bossTitle = (clearing->village || clearing->chapel) ? NULL
            : (clearing->cemetery ? "Coveiro das Raízes" : "Guardião da Clareira");
        BeginStagePresentation(&e->presentation,
            (clearing->village || clearing->chapel) ? 0 : e->progress.stage, areaTitle,
            e->progress.stage==CLEARING_BOSS_STAGE ? bossTitle : NULL);
        if (clearing->cemetery || clearing->village) e->presentation.portraitCenterX=.73f;
        e->guardianIntroPending=!clearing->cemetery && !clearing->village && !clearing->chapel &&
                                e->progress.stage==CLEARING_BOSS_STAGE &&
                                !e->progress.guardianIntroSeen;
        if (e->progress.stage == CLEARING_BOSS_STAGE) StartArenaBoss(enemies, clearing, player, e->progress.bossPower);
        else StartEnemyWave(enemies, clearing, e->progress.stage, player, e->progress.damage);
    }
}

void BeginEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                    RunProgress progress, Vector2 player) {
    progress.map = clearing->chapel ? 3 : (clearing->village ? 2 : (clearing->cemetery ? 1 : 0));
    if ((clearing->village || clearing->chapel) && progress.stage < CLEARING_BOSS_STAGE)
        progress.stage = CLEARING_BOSS_STAGE;
    *e = (Encounter){.progress=progress, .sealEnemy=-1};
    StartStage(e, enemies, clearing, player);
    BeginAreaEntry(&e->presentation);
}

bool EncounterPaused(const Encounter *e) {
    return e->guardianIntroPending || e->presentation.remaining>0 || e->phase == ENCOUNTER_REWARD ||
           e->phase == ENCOUNTER_SEAL || e->phase == ENCOUNTER_LETTER ||
           e->phase == ENCOUNTER_CHAPEL_TEXT || e->phase == ENCOUNTER_DEFEAT;
}

void CompleteGuardianIntro(Encounter *e, bool cinematicPlayed) {
    if (!e->guardianIntroPending) return;
    e->guardianIntroPending=false;
    e->progress.guardianIntroSeen=true;
    if (cinematicPlayed) e->presentation=(StagePresentation){0};
    if (!e->practice && !SaveClearingCheckpoint(&e->progress))
        TraceLog(LOG_WARNING,"Nao foi possivel salvar a conclusao da cutscene do Guardiao.");
}

bool ClearingAltarOpen(const Encounter *e, const Clearing *clearing) {
    return !clearing->cemetery && !clearing->village && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE;
}

bool CanEnterCemetery(const Encounter *e, const Clearing *clearing,
                      Vector2 player, bool interactPressed) {
    return interactPressed && ClearingAltarOpen(e, clearing) && !EncounterPaused(e) &&
           CheckCollisionPointCircle(player, clearing->pedestal, CLEARING_ALTAR_RADIUS);
}

bool CanEnterVillage(const Encounter *e, const Clearing *clearing,
                     Vector2 player, bool interactPressed) {
    (void)interactPressed;
    return clearing->cemetery && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

bool CanEnterChapel(const Encounter *e, const Clearing *clearing,
                    Vector2 player, bool interactPressed) {
    (void)interactPressed;
    return clearing->village && e->phase == ENCOUNTER_CLEAR &&
           e->progress.stage == CLEARING_COMPLETE_STAGE && !EncounterPaused(e) &&
           CheckCollisionPointRec(player, clearing->exit);
}

float EncounterDamage(const Encounter *e) {
    int weapon = e->progress.weapon;
    return e->progress.damage * (1 + .2f*e->progress.weaponLevel[weapon]) *
           (e->progress.strongWeapon == weapon ? 1.5f : 1.0f);
}

bool EncounterBossVisible(const Encounter *e, const EnemyGroup *enemies) {
    return !e->guardianIntroPending && e->presentation.remaining <= 0 && e->progress.stage == CLEARING_BOSS_STAGE &&
           enemies->list[0].type == ENEMY_BOSS && enemies->list[0].health > 0;
}

static void RollRewards(Encounter *e) {
    if (e->progress.stage == CLEARING_WAVES) {
        e->rewardCount=2;
        e->rewards[0]=(Reward){REWARD_WEAPON,e->progress.weapon,0};
        e->rewards[1]=(Reward){REWARD_HEALTH,-1,GetRandomValue(3,6)*10};
        return;
    }
    e->rewardCount=3;
    int swap = GetRandomValue(0,99) < 80 ? GetRandomValue(0,2) : -1;
    for (int i=0; i<3; i++) {
        int kind=GetRandomValue(0,3);
        if (kind == REWARD_WEAPON && e->progress.weaponLevel[e->progress.weapon] >= 4)
            kind=REWARD_DAMAGE;
        if (i==swap) kind=REWARD_SWAP;
        e->rewards[i]=(Reward){kind, kind==REWARD_SWAP ?
            (e->progress.weapon+GetRandomValue(1,2))%3 : e->progress.weapon,0};
    }
}

static Rectangle ChoiceRect(int index, int count) {
    float w=fminf(330, (GetScreenWidth()-80.0f)/count-16);
    return (Rectangle){GetScreenWidth()/2.0f-(count*w+(count-1)*16)/2+index*(w+16),
                       GetScreenHeight()/2.0f-30,w,140};
}

static int ChoiceInput(int count) {
    for (int i=0; i<count; i++) {
        if (IsKeyPressed(KEY_ONE+i)) return i;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(),ChoiceRect(i,count))) return i;
    }
    return -1;
}

static void ApplyReward(Encounter *e, Reward reward, int *health) {
    bool strong=e->progress.stage==CLEARING_WAVES;
    e->notice[0]=0;
    if (reward.kind==REWARD_DAMAGE) e->progress.damage+=.15f;
    else if (reward.kind==REWARD_HEALTH) {
        int added=strong?reward.amount:20;
        e->progress.maxHealth+=added;
        *health+=added;
        if (strong) { e->progress.damage+=.2f; e->progress.speed+=15; }
    } else if (reward.kind==REWARD_SPEED) e->progress.speed+=15;
    else if (reward.kind==REWARD_WEAPON) {
        if (strong) e->progress.strongWeapon=e->progress.weapon;
        else if (e->progress.weaponLevel[e->progress.weapon]<4) e->progress.weaponLevel[e->progress.weapon]++;
    } else {
        int old=e->progress.weapon;
        e->progress.weapon=reward.weapon;
        int bonus=GetRandomValue(0,2);
        float strength=.08f+.03f*e->progress.weaponLevel[old];
        if (bonus==0) {
            e->progress.damage+=strength;
            snprintf(e->notice,sizeof(e->notice),"Troca: dano +%d%%",(int)(strength*100));
        } else if (bonus==1) {
            e->progress.maxHealth+=15; *health+=15;
            snprintf(e->notice,sizeof(e->notice),"Troca: vida maxima +15");
        } else {
            e->progress.speed+=12;
            snprintf(e->notice,sizeof(e->notice),"Troca: velocidade +12");
        }
    }
}

bool ChooseEncounterReward(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                           Vector2 player, int *health, int choice) {
    if (e->phase!=ENCOUNTER_REWARD || choice<0 || choice>=e->rewardCount) return false;
    ApplyReward(e,e->rewards[choice],health);
    e->progress.stage++;
    e->progress.health=*health;
    if (!e->practice) SaveClearingCheckpoint(&e->progress);
    StartStage(e,enemies,clearing,player);
    return true;
}

bool OpenEncounterSeal(Encounter *e, const EnemyGroup *enemies, Vector2 player) {
    if (e->phase!=ENCOUNTER_FIGHT || e->presentation.remaining>0) return false;
    for (int i=0;i<ENEMY_COUNT;i++) if (enemies->list[i].state==ENEMY_FALLEN &&
        Vector2Distance(enemies->list[i].position,player)<95) {
        e->sealEnemy=i; e->question=GetRandomValue(0,3); e->answer=GetRandomValue(0,2);
        e->phase=ENCOUNTER_SEAL;
        return true;
    }
    return false;
}

bool AnswerEncounterSeal(Encounter *e, EnemyGroup *enemies, int *health, int choice) {
    if (e->phase!=ENCOUNTER_SEAL || e->sealEnemy<0 || e->sealEnemy>=ENEMY_COUNT) return false;
    Enemy *enemy=&enemies->list[e->sealEnemy];
    int count=enemy->type==ENEMY_BOSS?3:2;
    if (choice<0 || choice>=count) return false;
    int correct=enemy->type==ENEMY_BOSS?e->answer:(e->question==0 || e->question==3?0:1);
    ClearEnemyAttacks(enemies);
    if (choice==correct) enemy->state=ENEMY_GONE;
    else {
        enemy->maxHealth=(int)fminf(65000,enemy->maxHealth*1.2f);
        enemy->health=enemy->maxHealth;
        enemy->power*=1.15f;
        enemy->state=ENEMY_IDLE; enemy->stateTime=0; enemy->attackCooldown=1;
        snprintf(e->notice,sizeof(e->notice),"Selo incorreto: o inimigo voltou mais forte");
        if (enemy->type==ENEMY_BOSS) {
            enemy->guardianPhase = GUARDIAN_PHASE_ONE;
            enemy->guardianMelee = false;
            enemy->guardianMeleeCooldown = 0;
            enemy->bossAttack = (enemies->cemetery ? COVEIRO_ATTACK_COUNT : GUARDIAN_ATTACK_COUNT) - 1;
            e->progress.bossPower=fminf(100,e->progress.bossPower*1.2f);
            e->progress.health=*health;
            if (!e->practice) SaveClearingCheckpoint(&e->progress);
        }
    }
    e->phase=ENCOUNTER_FIGHT;
    return true;
}

bool UpdateEncounter(Encounter *e, EnemyGroup *enemies, const Clearing *clearing,
                     Vector2 player, int *health) {
    if (*health<=0) { e->phase=ENCOUNTER_DEFEAT; return false; }
    if (e->guardianIntroPending || e->presentation.remaining>0) return false;
    if (e->progress.map == 3) {
        if (e->phase == ENCOUNTER_CHAPEL_TEXT) {
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                e->phase = ENCOUNTER_FIGHT;
            }
            return false;
        }
        if (IsKeyPressed(KEY_E)) {
            float w = (float)clearing->background.width, h = (float)clearing->background.height;
            if (Vector2Distance(player, (Vector2){w * 0.50f, h * 0.44f}) <= 125.0f) {
                e->chapelTextId = 0; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.365f, h * 0.26f}) <= 95.0f) {
                e->chapelTextId = 1; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.63f, h * 0.26f}) <= 95.0f) {
                e->chapelTextId = 2; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.85f, h * 0.35f}) <= 115.0f) {
                e->chapelTextId = 3; e->phase = ENCOUNTER_CHAPEL_TEXT;
            } else if (Vector2Distance(player, (Vector2){w * 0.22f, h * 0.34f}) <= 95.0f) {
                e->chapelTextId = 4; e->phase = ENCOUNTER_CHAPEL_TEXT;
            }
        }
        return false;
    }
    if (e->phase==ENCOUNTER_FIGHT) {
        if (e->progress.map == 2 && !e->villageHordeStarted &&
            enemies->list[0].type == ENEMY_BOSS && enemies->list[0].state == ENEMY_FALLEN) {
            ClearEnemyAttacks(enemies);
            e->phase = ENCOUNTER_LETTER;
            return false;
        }
        if (EnemiesRemaining(enemies)==0) {
            ClearEnemyAttacks(enemies);
            if (e->progress.stage==CLEARING_BOSS_STAGE) {
                e->progress.stage=CLEARING_COMPLETE_STAGE;
                e->progress.health=*health;
                e->phase=ENCOUNTER_CLEAR;
                e->notice[0]=0;
                const char *sub = e->progress.map == 2
                    ? "Ultimo registro: \"Sobrevivi, mas falhei em salva-los...\""
                    : (e->progress.map == 1 ? "A saida a direita leva a Vila Lascada"
                                            : "O altar agora leva ao cemiterio");
                BeginStagePresentation(&e->presentation, 0, sub, NULL);
                e->presentation.announcement = e->progress.map == 2 ? "VILA LASCADA LIMPA"
                    : (e->progress.map == 1 ? "CEMITERIO LIMPO" : "CLAREIRA LIMPA");
                e->presentation.duration=e->presentation.remaining=2.5f;
                QueueAreaUnlock(&e->presentation, e->progress.map == 2 ? "Capela do Anjo"
                    : (e->progress.map == 1 ? "Vila Lascada" : "Cemitério das Raízes"));
                if (!e->practice) SaveClearingCheckpoint(&e->progress);
            } else {
                e->phase=ENCOUNTER_REWARD; e->rerolled=false; RollRewards(e);
            }
        } else if (IsKeyPressed(KEY_E) && !(e->progress.map == 2 && !e->villageHordeStarted)) {
            OpenEncounterSeal(e,enemies,player);
        }
    } else if (e->phase==ENCOUNTER_LETTER) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            e->villageHordeStarted = true;
            e->phase = ENCOUNTER_FIGHT;
            StartVillageHorde(enemies, clearing, player, e->progress.damage);
            BeginStagePresentation(&e->presentation, 0, "Os aldeoes mortos erguem-se da terra!", NULL);
            e->presentation.announcement = "A MALDICAO DESPERTA";
            e->presentation.duration = e->presentation.remaining = 2.2f;
            return true;
        }
    } else if (e->phase==ENCOUNTER_REWARD) {
        if (IsKeyPressed(KEY_R) && !e->rerolled) { e->rerolled=true; RollRewards(e); }
        int selected=ChoiceInput(e->rewardCount);
        if (selected>=0) {
            return ChooseEncounterReward(e,enemies,clearing,player,health,selected);
        }
    } else if (e->phase==ENCOUNTER_SEAL) {
        Enemy *enemy=&enemies->list[e->sealEnemy];
        int selected=ChoiceInput(enemy->type==ENEMY_BOSS?3:2);
        if (selected>=0) return AnswerEncounterSeal(e,enemies,health,selected);
    }
    return false;
}

static const char *RewardText(const Encounter *e, Reward reward) {
    if (e->progress.stage==CLEARING_WAVES) return reward.kind==REWARD_WEAPON ?
        "Arma atual\nDano +50%" : TextFormat("Status\nVida +%d / dano +20%%\nVelocidade +15",reward.amount);
    switch (reward.kind) {
        case REWARD_DAMAGE: return "Dano\n+15% de dano base";
        case REWARD_HEALTH: return "Vida maxima\n+20 de vida";
        case REWARD_SPEED: return "Velocidade\n+15 de velocidade";
        case REWARD_WEAPON: {
            int nextLevel = e->progress.weaponLevel[reward.weapon] + 1;
            const char *perk = "Dano +20%";
            if (reward.weapon == 0) {
                if (nextLevel == 1) perk = "Lamina Estendida (+alcance)";
                else if (nextLevel == 2) perk = "Passo da Ordem (-recarga dash)";
                else if (nextLevel == 3) perk = "Corte Amplo (+arco e recuo)";
                else if (nextLevel == 4) perk = "Onda Rubra (onda de choque)";
            } else if (reward.weapon == 1) {
                if (nextLevel == 1) perk = "Canone Expandido (5 livros)";
                else if (nextLevel == 2) perk = "Vortice Sagrado (+aceleracao)";
                else if (nextLevel == 3) perk = "Escudo (6 livros, bloqueia tiros)";
                else if (nextLevel == 4) perk = "Agua Benta (poca de dano)";
            } else if (reward.weapon == 2) {
                if (nextLevel == 1) perk = "Cadencia Rapida (+tiros/vel)";
                else if (nextLevel == 2) perk = "Disparo Duplo (2 projeteis)";
                else if (nextLevel == 3) perk = "Disparo Triplo (3 projeteis)";
                else if (nextLevel == 4) perk = "Pistolas Duplas (2x os tiros)";
            }
            return TextFormat("%s (%d/4)\n%s\nDano +20%%", WeaponTitle(reward.weapon), nextLevel, perk);
        }
        default: return TextFormat("Trocar por %s\nSubstitui a arma atual\nBonus surpresa",WeaponTitle(reward.weapon));
    }
}

void DrawEncounter(const Encounter *e, const EnemyGroup *enemies) {
    if (e->presentation.remaining>0) {
        Texture2D bossSprite = e->progress.map == 2 ? enemies->villageBossWalk
            : (e->progress.map == 1 ? enemies->cemeteryPortrait : enemies->bossPortrait);
        int cols = e->progress.map == 2 ? 6 : 1;
        int rows = e->progress.map == 2 ? 4 : 1;
        DrawStagePresentation(&e->presentation, bossSprite, cols, rows);
        return;
    }
    bool bossVisible = EncounterBossVisible(e, enemies);
    int statusY = 205 + (bossVisible ? ENCOUNTER_BOSS_HUD_HEIGHT : 0);
    const char *mapLabel = e->progress.map == 3 ? "Capela do Anjo"
        : (e->progress.map == 2 ? "Vila Lascada"
        : (e->progress.map == 1 ? "Cemitério" : "Clareira"));
    const char *clearLabel = e->progress.map == 3 ? "Capela do Anjo - portal para o Inferno Inicial aberto à esquerda"
        : (e->progress.map == 2 ? "Vila Lascada limpa - siga pelo arco ao topo"
        : (e->progress.map == 1 ? "Cemitério limpo - siga pela saída à direita"
                                : "Clareira limpa - altar liberado"));
    const char *bossLabel = e->progress.map == 2 ? "Líder da Vila"
        : (e->progress.map == 1 ? "Coveiro das Raízes" : "Guardião da Clareira");
    if (e->progress.map == 3) DrawGameText("Capela do Anjo - explore o santuário e aproxime-se do altar (E)", 24, statusY, 20, GOLD);
    else if (e->progress.stage<=CLEARING_WAVES) DrawGameText(TextFormat("%s - onda %d/5 | Restantes: %d",
        mapLabel,e->progress.stage,EnemiesRemaining(enemies)),24,statusY,20,GOLD);
    else DrawGameText(e->phase==ENCOUNTER_CLEAR ? clearLabel :
        (e->progress.map == 2 ? (e->villageHordeStarted ?
            TextFormat("Horda da Vila - Restantes: %d", EnemiesRemaining(enemies)) :
            "Duelo - Líder da Vila")
            : TextFormat("Onda 5/5 - %s", bossLabel)),24,statusY,20,GOLD);
    if (e->notice[0]) DrawGameText(e->notice,24,statusY+27,18,RAYWHITE);
    if (bossVisible) {
        const Enemy *boss=&enemies->list[0];
        bool phaseTwo = boss->guardianPhase != GUARDIAN_PHASE_ONE;
        const char *phase = boss->guardianPhase == GUARDIAN_PHASE_TRANSITION ? "Despertando..." :
                            phaseTwo ? "Fase 2/2" : "Fase 1/2";
        int w=(int)fminf(600,GetScreenWidth()*.5f), x=(GetScreenWidth()-w)/2;
        DrawRectangle(x-8,8,w+16,46,Fade(BLACK,.78f));
        DrawRectangle(x,31,w,16,DARKGRAY);
        DrawRectangle(x,31,w*boss->health/boss->maxHealth,16,phaseTwo ? PURPLE : MAROON);
        DrawRectangle(x+w/2,31,2,16,Fade(GOLD,.65f));
        const char *label = TextFormat("%s  %d/%d  |  %s",bossLabel,
                                       boss->health,boss->maxHealth,phase);
        DrawGameText(label,(GetScreenWidth()-MeasureGameText(label,18))/2,10,18,GOLD);
    }
    if (!EncounterPaused(e)) return;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.86f));
    if (e->phase == ENCOUNTER_CHAPEL_TEXT) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int bw = (int)fminf(760.0f, sw - 80.0f), bh = 370;
        int bx = (sw - bw) / 2, by = (sh - bh) / 2;
        DrawRectangle(bx, by, bw, bh, (Color){24, 28, 38, 245});
        DrawRectangleLinesEx((Rectangle){(float)bx, (float)by, (float)bw, (float)bh}, 2, GOLD);
        static const char *titles[] = {
            "Voz do Anjo Guardião",
            "Estátua do Guardião",
            "Estátua do Irmão Caído",
            "O Trono Partido",
            "Selo Secundário"
        };
        static const char *bodies[][6] = {
            {
                "\"Mortal... você chegou além das raízes e da vila caída.",
                "Meu irmão e eu protegíamos os vivos, até ele desafiar a morte",
                "e tornar-se o Necromante. Sacrifiquei meu poder nos Selos Sagrados.",
                "Leve minha bênção e siga firme contra a corrupção que se espalhou.\"",
                "", ""
            },
            {
                "A estátua permanece erguida, iluminada por um fio dourado.",
                "Na base lê-se: \"Ao Guardião que renunciou à própria força",
                "para selar o abismo e dar aos mortais mais um amanhecer.\"",
                "", "", ""
            },
            {
                "A face celestial está erodida e marcada por fissuras profundas.",
                "\"Aquele que negou a lei da morte partiu o primeiro selo",
                "e transformou sua própria glória em cinzas e maldição.\"",
                "", "", ""
            },
            {
                "O trono de pedra está partido e vazio há séculos.",
                "Nenhum dos dois irmãos voltou a sentar-se aqui desde a queda.",
                "", "", "", ""
            },
            {
                "Um círculo de proteção antigo e adormecido, gravado na pedra.",
                "Sua energia responde apenas quando o selo central se rompe.",
                "", "", "", ""
            }
        };
        int tid = (e->chapelTextId >= 0 && e->chapelTextId < 5) ? e->chapelTextId : 0;
        DrawGameTextIn(titles[tid], (Rectangle){(float)bx, (float)(by + 18), (float)bw, 34}, 28, GOLD, true);
        for (int i = 0; i < 6; i++) {
            if (bodies[tid][i][0]) DrawGameText(bodies[tid][i], bx + 36, by + 84 + i * 32, 20, RAYWHITE);
        }
        const char *hint = "[Espaço / Enter / Clique] Continuar";
        DrawGameText(hint, (sw - MeasureGameText(hint, 19)) / 2, by + bh - 44, 19, GOLD);
        return;
    }
    if (e->phase == ENCOUNTER_LETTER) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int bw = (int)fminf(720.0f, sw - 80.0f), bh = 380;
        int bx = (sw - bw) / 2, by = (sh - bh) / 2;
        DrawRectangle(bx, by, bw, bh, (Color){38, 28, 22, 245});
        DrawRectangleLinesEx((Rectangle){(float)bx, (float)by, (float)bw, (float)bh}, 2, GOLD);
        const char *ltitle = "Último Registro do Líder da Vila";
        DrawGameTextIn(ltitle, (Rectangle){(float)bx, (float)(by + 18), (float)bw, 34}, 28, GOLD, true);
        static const char *lines[] = {
            "\"Quando a névoa rubra desceu sobre a Vila Lascada, nenhum",
            "dos nossos conseguiu resistir à febre que tomou o vale.",
            "Enterramos dezenas no Cemitério das Raízes antes que os",
            "próprios mortos começassem a responder ao chamado do selo.",
            "",
            "Eu fui o único que permaneceu de pé, preso à culpa de",
            "sobreviver... Se você me derrubou, cuidado: a terra da",
            "vila ainda guarda os que não descansaram.\""
        };
        for (int i = 0; i < 8; i++) {
            DrawGameText(lines[i], bx + 36, by + 76 + i * 28, 19, RAYWHITE);
        }
        const char *hint = "[Espaço / Enter / Clique] Continuar";
        DrawGameText(hint, (sw - MeasureGameText(hint, 19)) / 2, by + bh - 44, 19, GOLD);
        return;
    }
    int titleY=(int)(GetScreenHeight()/2-160);
    const char *title=e->phase==ENCOUNTER_REWARD ? "Escolha seu bônus" :
                      e->phase==ENCOUNTER_SEAL ? "Selo - combate pausado" : "Fim da tentativa";
    DrawGameTextIn(title,(Rectangle){0,titleY,GetScreenWidth(),49},42,GOLD,true);
    if (e->phase==ENCOUNTER_DEFEAT) {
        DrawGameText("Esc: menu | Continuar reinicia esta onda ou boss",GetScreenWidth()/2-270,titleY+80,20,RAYWHITE);
        return;
    }
    int count=e->phase==ENCOUNTER_REWARD ? e->rewardCount :
              enemies->list[e->sealEnemy].type==ENEMY_BOSS ? 3 : 2;
    static const char *questions[]={"V e V = V", "V e F = V", "nao V = V", "V ou F = V"};
    if (e->phase==ENCOUNTER_SEAL) {
        const char *prompt=count==3 ? TextFormat("Qual expressão equivale a: %s ?",bossQuestions[e->question].expression)
                                    : questions[e->question];
        DrawGameText(prompt,(GetScreenWidth()-MeasureGameText(prompt,22))/2,titleY+60,22,RAYWHITE);
    }
    for (int i=0;i<count;i++) {
        Rectangle box=ChoiceRect(i,count);
        DrawRectangleRec(box,CheckCollisionPointRec(GetMousePosition(),box)?(Color){80,45,50,255}:(Color){40,25,35,255});
        DrawRectangleLinesEx(box,2,GOLD);
        const char *label;
        if (e->phase==ENCOUNTER_REWARD) label=RewardText(e,e->rewards[i]);
        else if (count==2) label=i==0?"Verdadeiro":"Falso";
        else label=bossQuestions[e->question].choices[(i+3-e->answer)%3];
        DrawGameText(TextFormat("%d. %s",i+1,label),(int)box.x+14,(int)box.y+20,19,RAYWHITE);
    }
    if (e->phase==ENCOUNTER_REWARD) DrawGameText(e->rerolled?"Novo sorteio utilizado":"R: sortear novamente (1 gratuito)",
        GetScreenWidth()/2-180,GetScreenHeight()/2+145,20,GOLD);
}
