#include "encounter.h"
#include "game_text.h"
#include "menu.h"
#include "dev_tools.h"
#include "bible.h"
#include "gun.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void Capture(const char *name, Encounter *encounter, EnemyGroup *enemies,
                    Clearing *clearing, Vector2 player) {
    RenderTexture2D target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
    BeginTextureMode(target);
    ClearBackground(BLACK);
    Camera2D camera={.zoom=.72f};
    BeginMode2D(camera);
    DrawClearing(clearing);
    if (ClearingAltarOpen(encounter, clearing)) DrawClearingAltarGlow(clearing, 0.5f);
    if (strstr(name,"collision")) DrawClearingCollisionGuide(clearing);
    DrawEnemies(enemies,player,false);
    DrawCircleV(player,15,SKYBLUE);
    DrawEnemies(enemies,player,true);
    DrawEnemyProjectiles(enemies);
    EndMode2D();
    if (!strstr(name,"collision") && !strstr(name,"directions") && !strstr(name,"attack_poses")) DrawEncounter(encounter,enemies);
    EndTextureMode();
    Image shot=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&shot);
    assert(ExportImage(shot,name));
    UnloadImage(shot);
    UnloadRenderTexture(target);
}

static void CheckVillageZombies(Encounter *encounter, EnemyGroup *enemies, Clearing *map) {
    StartVillageHorde(enemies, map, map->spawn, 1);
    assert(EnemiesRemaining(enemies) == 31 && enemies->villageSpawnRemaining == 12);
    for (int i = 0; i < 19; i++) {
        const Enemy *zombie = &enemies->list[i];
        assert(zombie->type == ENEMY_COMMON_MELEE && zombie->state == ENEMY_SPAWNING);
        assert(!EnemyCanBeHit(zombie));
    }
    int health = 100;
    UpdateEnemies(enemies, map, map->spawn, .66f, false, &health, 100);
    assert(enemies->villageSpawnRemaining == 11 && health == 100);
    assert(enemies->list[19].state == ENEMY_SPAWNING && !EnemyCanBeHit(&enemies->list[19]));
    UpdateEnemies(enemies, map, map->spawn, .32f, false, &health, 100);
    assert(enemies->list[0].state == ENEMY_SPAWNING);
    UpdateEnemies(enemies, map, map->spawn, .03f, false, &health, 100);
    assert(enemies->list[0].state == ENEMY_IDLE && EnemyCanBeHit(&enemies->list[0]));

    Vector2 ground = enemies->list[0].position;
    for (int i = 0; i < ENEMY_COUNT; i++) enemies->list[i] = (Enemy){.state=ENEMY_GONE};
    enemies->villageSpawnRemaining = 0;
    enemies->list[0] = (Enemy){.position=ground,.type=ENEMY_COMMON_MELEE,.state=ENEMY_ATTACK,
        .stateTime=VILLAGE_ZOMBIE_HIT_TIME-.02f,.health=62,.maxHealth=62,.power=1};
    UpdateEnemies(enemies, map, ground, .01f, false, &health, 100);
    assert(health == 100);
    UpdateEnemies(enemies, map, ground, .02f, false, &health, 100);
    assert(health == 88 && enemies->list[0].attackConnected);

    const EnemyState states[] = {ENEMY_IDLE, ENEMY_WALK, ENEMY_ATTACK, ENEMY_SPAWNING};
    for (int row = 0; row < 4; row++) for (int face = 0; face < 4; face++) {
        enemies->list[row*4+face] = (Enemy){.position={410+face*230,315+row*135},
            .type=ENEMY_COMMON_MELEE,.state=states[row],.facing=face,.walkTime=.24f,
            .stateTime=row == 3 ? .12f+.24f*face : VILLAGE_ZOMBIE_HIT_TIME,
            .health=62,.maxHealth=62};
    }
    Capture("audit/village_zombie_directions.png",encounter,enemies,map,(Vector2){30,30});
}

static void CheckLegacyCheckpoints(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/partida_pif.sav", GetApplicationDirectory());
    const int legacyStages[] = {6, 7, 8, 11, 12, 13, 14};
    const int maps[] = {0, 0, 1, 1, 1, 1, 1};
    const int stages[] = {CLEARING_BOSS_STAGE, CLEARING_COMPLETE_STAGE, 1, 4,
                          CLEARING_BOSS_STAGE, CLEARING_BOSS_STAGE, CLEARING_COMPLETE_STAGE};
    for (int i = 0; i < (int)(sizeof(legacyStages)/sizeof(legacyStages[0])); i++) {
        char contents[128];
        snprintf(contents, sizeof(contents), "PIF-CLEARING-3 2 %d 90 120 1.25 240 1.2 0 0 2 -1\n",
                 legacyStages[i]);
        assert(SaveFileText(path, contents));
        RunProgress progress;
        assert(LoadClearingCheckpoint(&progress));
        assert(progress.stage == stages[i] && progress.map == maps[i]);
        assert(progress.weapon == 2 && progress.health == 90 && progress.weaponLevel[2] == 2);
        assert(progress.guardianIntroSeen == (maps[i]==1 || stages[i]==CLEARING_COMPLETE_STAGE));
    }
    assert(SaveFileText(path,"PIF-RUN-5 2 5 100 100 1 240 1 0 0 0 -1 0\n"));
    RunProgress bossCheckpoint={0};
    assert(LoadClearingCheckpoint(&bossCheckpoint));
    assert(bossCheckpoint.stage==CLEARING_BOSS_STAGE && !bossCheckpoint.guardianIntroSeen);
    assert(SaveClearingCheckpoint(&bossCheckpoint));
    RunProgress currentCheckpoint={0};
    assert(LoadClearingCheckpoint(&currentCheckpoint) && !currentCheckpoint.guardianIntroSeen);
    assert(SaveFileText(path,"PIF-RUN-6 2 6 100 100 1 240 1 0 0 0 -1 0 1\n"));
    assert(LoadClearingCheckpoint(&currentCheckpoint));
    assert(currentCheckpoint.guardianAftermath==GUARDIAN_ALTAR_READY);
    for (int state=GUARDIAN_LIBERATION_PENDING; state<=GUARDIAN_PASSAGE_PENDING; state++) {
        currentCheckpoint.guardianAftermath=(GuardianAftermathState)state;
        assert(SaveClearingCheckpoint(&currentCheckpoint));
        RunProgress restored={0};
        assert(LoadClearingCheckpoint(&restored) && restored.guardianAftermath==state);
    }
    /* Fase 1: Migração PIF-RUN-7 -> PIF-RUN-8, nível único, strongWeapon -> epicDamageStacks, EncounterDamage e WeaponBaseDamage */
    assert(WeaponBaseDamage(0) == 5 && WeaponBaseDamage(1) == 12 && WeaponBaseDamage(2) == 17);
    assert(SaveFileText(path, "PIF-RUN-7 1 3 80 120 1.3 255 1 1 4 2 1 1 0 4\n"));
    RunProgress mig7 = {0};
    assert(LoadClearingCheckpoint(&mig7));
    assert(mig7.epicDamageStacks == 1);
    assert(mig7.weapon == 1);
    assert(mig7.weaponLevel[0] == 4 && mig7.weaponLevel[1] == 4 && mig7.weaponLevel[2] == 4);
    assert(SaveFileText(path, "PIF-RUN-7 2 2 90 100 1.0 240 1 0 1 5 -1 0 0 0\n"));
    assert(LoadClearingCheckpoint(&mig7));
    assert(mig7.epicDamageStacks == 0);
    assert(mig7.weapon == 2);
    assert(mig7.weaponLevel[0] == 5 && mig7.weaponLevel[1] == 5 && mig7.weaponLevel[2] == 5);
    mig7.epicDamageStacks = 3;
    assert(SaveClearingCheckpoint(&mig7));
    RunProgress mig8 = {0};
    assert(LoadClearingCheckpoint(&mig8));
    assert(mig8.epicDamageStacks == 3 && mig8.weaponLevel[0] == 5 && mig8.weaponLevel[1] == 5 && mig8.weaponLevel[2] == 5);

    Encounter dmgTest = {.progress = NewRunProgress(2)};
    dmgTest.progress.damage = 1.2f;
    SetRunWeaponLevel(&dmgTest.progress, 3);
    dmgTest.progress.epicDamageStacks = 2;
    /* Katana (2): 1.2 * (1 + 0.2*3 + 0.5*2 + 0) = 1.2 * 2.6 = 3.12 */
    assert(fabsf(EncounterDamage(&dmgTest) - 3.12f) < 0.0001f);
    /* Bíblia (1) N>=1: +0.15 => 1.2 * (1 + 0.6 + 1.0 + 0.15) = 1.2 * 2.75 = 3.30 */
    dmgTest.progress.weapon = 1;
    assert(fabsf(EncounterDamage(&dmgTest) - 3.30f) < 0.0001f);
    SetRunWeaponLevel(&dmgTest.progress, 0);
    /* Bíblia (1) N=0: +0 => 1.2 * (1 + 0 + 1.0 + 0) = 2.40 */
    assert(fabsf(EncounterDamage(&dmgTest) - 2.40f) < 0.0001f);
    /* Arma de fogo (0) N=1: +0 => 1.2 * (1 + 0.2 + 1.0 + 0) = 2.64 */
    dmgTest.progress.weapon = 0;
    SetRunWeaponLevel(&dmgTest.progress, 1);
    assert(fabsf(EncounterDamage(&dmgTest) - 2.64f) < 0.0001f);
    /* Arma de fogo (0) N=2: +0.25 => 1.2 * (1 + 0.4 + 1.0 + 0.25) = 1.2 * 2.65 = 3.18 */
    SetRunWeaponLevel(&dmgTest.progress, 2);
    assert(fabsf(EncounterDamage(&dmgTest) - 3.18f) < 0.0001f);

    /* Fase 2: Testes de upgrades das armas (Bíblia e Arma de fogo) */
    Bible bTest = {0};
    ResetBible(&bTest);
    Vector2 pPos = {500.0f, 500.0f};
    /* Base (N0) e N1: 4 livros, órbita 90 px, boost 1.25 s, recarga 4.0 s, não bloqueia projéteis, sem poças */
    UpdateBible(&bTest, 0.0f, true, 0, true, pPos);
    assert(fabsf(bTest.boostRemaining - 1.25f) < 0.001f && fabsf(bTest.boostCooldown - 4.0f) < 0.001f);
    assert(BibleTouchesCircle(&bTest, pPos, (Vector2){590.0f, 466.0f}, 1.0f));
    /* Em 90 px + 17 (BIBLE_HIT_RADIUS) + 1 = 108 px (x=608); x=612 está fora em 90 px mas dentro em 102 px (102+18=120 -> x=620) */
    assert(!BibleTouchesCircle(&bTest, pPos, (Vector2){612.0f, 466.0f}, 1.0f));
    assert(!BibleBlocksCircle(&bTest, pPos, (Vector2){590.0f, 466.0f}, 6.0f));
    /* N2: 5 livros, órbita 102 px */
    ResetBible(&bTest);
    UpdateBible(&bTest, 0.0f, false, 2, true, pPos);
    assert(BibleTouchesCircle(&bTest, pPos, (Vector2){612.0f, 466.0f}, 1.0f));
    /* N3: boost 1.65 s, recarga 2.8 s, ainda não bloqueia projéteis */
    ResetBible(&bTest);
    UpdateBible(&bTest, 0.0f, true, 3, true, pPos);
    assert(fabsf(bTest.boostRemaining - 1.65f) < 0.001f && fabsf(bTest.boostCooldown - 2.8f) < 0.001f);
    assert(!BibleBlocksCircle(&bTest, pPos, (Vector2){602.0f, 466.0f}, 6.0f));
    /* N4: 6 livros e bloqueia projéteis durante o boost, mas sem poças de água benta */
    ResetBible(&bTest);
    UpdateBible(&bTest, 0.7f, true, 4, true, pPos);
    assert(BibleBlocksCircle(&bTest, pPos, (Vector2){pPos.x + cosf(bTest.angle)*102.0f, pPos.y - 34.0f + sinf(bTest.angle)*102.0f}, 6.0f));
    assert(bTest.pools[0].remaining <= 0.0f);
    /* N5: poças de água benta a cada 3.5 s em combate */
    ResetBible(&bTest);
    UpdateBible(&bTest, 0.7f, false, 5, true, pPos);
    assert(bTest.pools[0].remaining > 0.0f && bTest.pools[0].tickReady);
    assert(BibleHolyWaterHits(&bTest, pPos, 10.0f));

    /* Arma de fogo: N0..N5 */
    Clearing gunMap = {0};
    Gun gTest = {0};
    ResetGun(&gTest);
    Vector2 aimPos = {600.0f, 465.0f};
    /* N0: 1 tiro, intervalo 0.20 s, hitsRemaining = 1 */
    UpdateGun(&gTest, &gunMap, pPos, aimPos, 0.0f, true, 0);
    assert(fabsf(gTest.shotCooldown - 0.20f) < 0.001f);
    int shotCount = 0;
    for (int i = 0; i < GUN_MAX_SHOTS; i++) if (gTest.shots[i].life > 0.0f) shotCount++;
    assert(shotCount == 1 && gTest.shots[0].hitsRemaining == 1);
    gTest.shots[0].delay = 0.0f;
    assert(GunHitEnemyCircle(&gTest, 0, gTest.shots[0].position, 10.0f) == WeaponBaseDamage(0));
    assert(gTest.shots[0].life <= 0.0f);
    /* N1: intervalo 0.14 s, 1 tiro */
    ResetGun(&gTest);
    UpdateGun(&gTest, &gunMap, pPos, aimPos, 0.0f, true, 1);
    assert(fabsf(gTest.shotCooldown - 0.14f) < 0.001f);
    /* N3: 2 tiros (+-5 graus), não perfura */
    ResetGun(&gTest);
    UpdateGun(&gTest, &gunMap, pPos, aimPos, 0.0f, true, 3);
    shotCount = 0;
    for (int i = 0; i < GUN_MAX_SHOTS; i++) if (gTest.shots[i].life > 0.0f) shotCount++;
    assert(shotCount == 2 && gTest.shots[0].hitsRemaining == 1);
    /* N4: tiros perfurantes (acertam até 2 inimigos distintos, nunca o mesmo duas vezes) */
    ResetGun(&gTest);
    UpdateGun(&gTest, &gunMap, pPos, aimPos, 0.0f, true, 4);
    assert(gTest.shots[0].hitsRemaining == 2);
    gTest.shots[0].delay = 0.0f;
    Vector2 sPos = gTest.shots[0].position;
    gTest.shots[1].life = 0.0f; /* Isola 1 tiro para testar perfuração */
    assert(GunHitEnemyCircle(&gTest, 3, sPos, 10.0f) == WeaponBaseDamage(0));
    assert(gTest.shots[0].life > 0.0f && gTest.shots[0].hitsRemaining == 1);
    /* Mesmo inimigo (índice 3) não leva segundo acerto do mesmo tiro */
    assert(GunHitEnemyCircle(&gTest, 3, sPos, 10.0f) == 0);
    assert(gTest.shots[0].life > 0.0f);
    /* Segundo inimigo (índice 4) leva dano integral e encerra o tiro */
    assert(GunHitEnemyCircle(&gTest, 4, sPos, 10.0f) == WeaponBaseDamage(0));
    assert(gTest.shots[0].life <= 0.0f);
    /* N5: duas pistolas = 4 tiros por disparo */
    ResetGun(&gTest);
    UpdateGun(&gTest, &gunMap, pPos, aimPos, 0.0f, true, 5);
    shotCount = 0;
    for (int i = 0; i < GUN_MAX_SHOTS; i++) if (gTest.shots[i].life > 0.0f) shotCount++;
    assert(shotCount == 4);

    /* Fase 3a: Testes de raridades, sorteio normal, remoção do nível 5 do pool e aplicação de efeitos */
    Clearing rewardMap = {0};
    EnemyGroup dummyEnemies = {0};
    for (int i = 0; i < ENEMY_COUNT; i++) dummyEnemies.list[i] = (Enemy){.state = ENEMY_GONE};
    Encounter rTest = {.progress = NewRunProgress(1), .practice = true};
    int rHealth = 100;
    int rarityCounts[RARITY_COUNT] = {0};
    /* 1. Sorteio normal e teste estatístico dos pesos (1000 telas = 3000 vagas, pesos 70/25/5) */
    int firstSlotRarity[RARITY_COUNT] = {0};
    for (int trial = 0; trial < 1000; trial++) {
        rTest.progress.stage = 1;
        rTest.progress.weapon = 1;
        SetRunWeaponLevel(&rTest.progress, 2);
        rTest.phase = ENCOUNTER_FIGHT;
        UpdateEncounter(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth);
        assert(rTest.phase == ENCOUNTER_REWARD && rTest.rewardCount == 3);
        firstSlotRarity[rTest.rewards[0].rarity]++;
        for (int i = 0; i < 3; i++) {
            Reward rw = rTest.rewards[i];
            assert(rw.rarity >= RARITY_COMMON && rw.rarity <= RARITY_EPIC);
            rarityCounts[rw.rarity]++;
            /* Nunca dois cartões do mesmo tipo na mesma tela */
            for (int j = i + 1; j < 3; j++) {
                assert(rw.kind != rTest.rewards[j].kind);
            }
            if (rw.rarity == RARITY_COMMON) {
                assert(rw.kind == REWARD_DAMAGE || rw.kind == REWARD_HEALTH || rw.kind == REWARD_SPEED);
            } else if (rw.rarity == RARITY_RARE) {
                assert(rw.kind == REWARD_SWAP);
                assert(rw.weapon != rTest.progress.weapon && rw.weapon >= 0 && rw.weapon < 3);
                assert(rw.bonus[0] >= REWARD_DAMAGE && rw.bonus[0] <= REWARD_SPEED);
                assert(rw.bonus[1] >= REWARD_DAMAGE && rw.bonus[1] <= REWARD_SPEED);
                assert(rw.bonus[0] != rw.bonus[1]);
            } else if (rw.rarity == RARITY_EPIC) {
                assert(rw.kind >= REWARD_WEAPON_UPGRADE && rw.kind <= REWARD_EPIC_FULL_HEAL);
                if (rw.kind == REWARD_EPIC_STATUS) {
                    assert(rw.amount == 50);
                }
            }
        }
    }
    /* Na 1ª vaga de cada tela (1000 amostras independentes sem esgotamento do raro),
       espera-se ~700 comuns (70%), ~250 raros (25%), ~50 épicos (5%).
       E no total de 3000 vagas (onde o raro tem teto de 1 por tela), comum domina > raro > épico. */
    assert(firstSlotRarity[RARITY_COMMON] >= 620 && firstSlotRarity[RARITY_COMMON] <= 780);
    assert(firstSlotRarity[RARITY_RARE] >= 180 && firstSlotRarity[RARITY_RARE] <= 320);
    assert(firstSlotRarity[RARITY_EPIC] >= 20 && firstSlotRarity[RARITY_EPIC] <= 90);
    assert(rarityCounts[RARITY_COMMON] > rarityCounts[RARITY_RARE] &&
           rarityCounts[RARITY_RARE] > rarityCounts[RARITY_EPIC] &&
           rarityCounts[RARITY_EPIC] >= 60);

    /* Fase 3b: Testes de reroll por cartão e da tela épica (onda 4, 3 cartões épicos distintos, sem reroll) */
    for (int trial = 0; trial < 200; trial++) {
        rTest.progress.stage = 1;
        SetRunWeaponLevel(&rTest.progress, 2);
        rTest.phase = ENCOUNTER_FIGHT;
        UpdateEncounter(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth);
        assert(rTest.phase == ENCOUNTER_REWARD && !rTest.rerolled);
        int slot = trial % 3;
        Reward before[3] = {rTest.rewards[0], rTest.rewards[1], rTest.rewards[2]};
        assert(!RerollEncounterReward(&rTest, -1) && !RerollEncounterReward(&rTest, 3));
        assert(RerollEncounterReward(&rTest, slot));
        assert(rTest.rerolled);
        /* O segundo reroll na mesma tela é recusado */
        assert(!RerollEncounterReward(&rTest, (slot + 1) % 3));
        /* Os outros 2 cartões permanecem intactos */
        for (int i = 0; i < 3; i++) {
            if (i != slot) assert(rTest.rewards[i].kind == before[i].kind);
        }
        /* O cartão renovado não repete o tipo anterior nem os outros dois presentes na tela */
        assert(rTest.rewards[slot].kind != before[0].kind &&
               rTest.rewards[slot].kind != before[1].kind &&
               rTest.rewards[slot].kind != before[2].kind);
    }

    /* Tela épica (após a última onda de cada mapa: Clareira = 3, Cemitério = 4, Terra dos Ossos = 3, Campo dos Templários = 2):
       3 cartões épicos distintos, sem reroll. E nas ondas anteriores desses mapas, tela normal com reroll permitido. */
    static const int epicMapIds[3] = {0, BONELAND_MAP, TEMPLAR_FIELD_MAP};
    static const int epicMapLastWaves[3] = {CLEARING_WAVES, BONELAND_WAVES, TEMPLAR_FIELD_WAVES};
    for (int m = 0; m < 3; m++) {
        rTest.progress.map = epicMapIds[m];
        int lastWave = epicMapLastWaves[m];
        /* Onda anterior à última: tela normal com reroll habilitado */
        rTest.progress.stage = lastWave - 1;
        SetRunWeaponLevel(&rTest.progress, 2);
        rTest.phase = ENCOUNTER_FIGHT;
        UpdateEncounter(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth);
        assert(rTest.phase == ENCOUNTER_REWARD && rTest.rewardCount == 3 && !rTest.rerolled);
        assert(RerollEncounterReward(&rTest, 0) && rTest.rerolled);
        /* Última onda do mapa: tela épica (3 épicos distintos, sem reroll) */
        for (int trial = 0; trial < 40; trial++) {
            rTest.progress.stage = lastWave;
            SetRunWeaponLevel(&rTest.progress, trial < 20 ? 2 : MAX_WEAPON_LEVEL);
            rTest.phase = ENCOUNTER_FIGHT;
            UpdateEncounter(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth);
            assert(rTest.phase == ENCOUNTER_REWARD && rTest.rewardCount == 3);
            assert(!RerollEncounterReward(&rTest, 0));
            for (int i = 0; i < 3; i++) {
                assert(rTest.rewards[i].rarity == RARITY_EPIC);
                assert(rTest.rewards[i].kind >= REWARD_WEAPON_UPGRADE &&
                       rTest.rewards[i].kind <= REWARD_EPIC_FULL_HEAL);
                if (trial >= 20) assert(rTest.rewards[i].kind != REWARD_WEAPON_UPGRADE);
                for (int j = i + 1; j < 3; j++) {
                    assert(rTest.rewards[i].kind != rTest.rewards[j].kind);
                }
            }
        }
    }
    rTest.progress.map = 0;

    /* 2. Ao chegar no nível 5, o upgrade de arma sai do sorteio */
    for (int trial = 0; trial < 300; trial++) {
        rTest.progress.stage = 1;
        SetRunWeaponLevel(&rTest.progress, MAX_WEAPON_LEVEL);
        rTest.phase = ENCOUNTER_FIGHT;
        UpdateEncounter(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth);
        assert(rTest.phase == ENCOUNTER_REWARD && rTest.rewardCount == 3);
        for (int i = 0; i < 3; i++) {
            assert(rTest.rewards[i].kind != REWARD_WEAPON_UPGRADE);
        }
    }

    /* 3. Aplicação de todos os tipos de cartão */
    /* Comum: Dano +0.15 */
    rTest.progress = NewRunProgress(0);
    rTest.practice = true;
    rHealth = 80;
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_DAMAGE, .rarity = RARITY_COMMON};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(fabsf(rTest.progress.damage - 1.15f) < 0.0001f);
    /* Comum: Vida máxima +20 e cura 20 */
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_HEALTH, .rarity = RARITY_COMMON};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(rTest.progress.maxHealth == 120 && rHealth == 100);
    /* Comum: Velocidade +15 */
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_SPEED, .rarity = RARITY_COMMON};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(fabsf(rTest.progress.speed - 219.0f) < 0.0001f);
    /* Raro: troca de arma mantendo o nível e concedendo 2 buffs comuns distintos */
    SetRunWeaponLevel(&rTest.progress, 3);
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_SWAP, .rarity = RARITY_RARE, .weapon = 2,
                                .bonus = {REWARD_DAMAGE, REWARD_SPEED}};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(rTest.progress.weapon == 2 && rTest.progress.initialWeapon == 0 && rTest.progress.weaponLevel[2] == 3);
    assert(fabsf(rTest.progress.damage - 1.30f) < 0.0001f && fabsf(rTest.progress.speed - 234.0f) < 0.0001f);
    assert(rTest.notice[0] == 0); /* Bônus surpresa antigo removido */
    /* Persistência de initialWeapon no save e reset completo ao morrer/repetir (NewRunProgress(initialWeapon)) */
    rTest.practice = false;
    assert(SaveClearingCheckpoint(&rTest.progress));
    RunProgress loadedSwap = {0};
    assert(LoadClearingCheckpoint(&loadedSwap));
    assert(loadedSwap.weapon == 2 && loadedSwap.initialWeapon == 0);
    RunProgress resetAfterDeath = NewRunProgress(loadedSwap.initialWeapon);
    assert(resetAfterDeath.weapon == 0 && resetAfterDeath.initialWeapon == 0);
    assert(resetAfterDeath.stage == 1 && resetAfterDeath.map == 0);
    assert(resetAfterDeath.health == 100 && resetAfterDeath.maxHealth == 100);
    assert(fabsf(resetAfterDeath.damage - 1.0f) < 0.0001f && fabsf(resetAfterDeath.speed - 204.0f) < 0.0001f);
    assert(resetAfterDeath.epicDamageStacks == 0 && resetAfterDeath.angelBlessingUses == 0);
    assert(resetAfterDeath.weaponLevel[0] == 0 && resetAfterDeath.weaponLevel[1] == 0 && resetAfterDeath.weaponLevel[2] == 0);
    rTest.practice = true;
    /* Épico (a): upgrade da arma equipada (+1 nível, até 5) */
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_WEAPON_UPGRADE, .rarity = RARITY_EPIC, .weapon = 2};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(rTest.progress.weaponLevel[0] == 4 && rTest.progress.weaponLevel[1] == 4 && rTest.progress.weaponLevel[2] == 4);
    /* Épico (b): dano das armas +50% (epicDamageStacks++) */
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_EPIC_DAMAGE, .rarity = RARITY_EPIC, .weapon = 2};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(rTest.progress.epicDamageStacks == 1);
    /* Épico (c): status (+50 vida máx e cura, +0.25 dano, +51 velocidade [+25% da base 204]) */
    rHealth = 50;
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_EPIC_STATUS, .rarity = RARITY_EPIC, .amount = 50};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(rTest.progress.maxHealth == 170 && rHealth == 100);
    assert(fabsf(rTest.progress.damage - 1.55f) < 0.0001f && fabsf(rTest.progress.speed - 285.0f) < 0.0001f);
    /* Épico (d): vida plena (+50 de vida máxima e cura toda a vida) */
    rHealth = 35;
    rTest.progress.stage = 1;
    rTest.phase = ENCOUNTER_REWARD;
    rTest.rewardCount = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_EPIC_FULL_HEAL, .rarity = RARITY_EPIC};
    assert(ChooseEncounterReward(&rTest, &dummyEnemies, &rewardMap, pPos, &rHealth, 0));
    assert(rTest.progress.maxHealth == 220 && rHealth == 220);

    /* Fase 4: Renderização visual das 3 raridades (comum, raro com 2 buffs, épico de +50% antes->depois) e reroll ativo/desativado */
    rTest = (Encounter){.progress = NewRunProgress(2), .phase = ENCOUNTER_REWARD, .rewardCount = 3, .sealEnemy = -1};
    SetRunWeaponLevel(&rTest.progress, 1); /* N1: mult = 1.2 => ceil(17 * 1.2) = 21; após +50% => mult = 1.7 => ceil(17 * 1.7) = 29 */
    rTest.progress.stage = 2;
    rTest.rewards[0] = (Reward){.kind = REWARD_DAMAGE, .rarity = RARITY_COMMON};
    rTest.rewards[1] = (Reward){.kind = REWARD_SWAP, .rarity = RARITY_RARE, .weapon = 1, .bonus = {REWARD_HEALTH, REWARD_SPEED}};
    rTest.rewards[2] = (Reward){.kind = REWARD_EPIC_DAMAGE, .rarity = RARITY_EPIC, .weapon = 2};
    RenderTexture2D uiTarget = LoadRenderTexture(1280, 720);
    BeginTextureMode(uiTarget);
    ClearBackground(BLACK);
    DrawEncounter(&rTest, &dummyEnemies);
    EndTextureMode();
    Image uiShot = LoadImageFromTexture(uiTarget.texture);
    ImageFlipVertical(&uiShot);
    assert(ExportImage(uiShot, "audit/rewards_rarities_active.png"));
    UnloadImage(uiShot);
    rTest.rerolled = true;
    rTest.rewards[0] = (Reward){.kind = REWARD_WEAPON_UPGRADE, .rarity = RARITY_EPIC, .weapon = 2};
    BeginTextureMode(uiTarget);
    ClearBackground(BLACK);
    DrawEncounter(&rTest, &dummyEnemies);
    EndTextureMode();
    uiShot = LoadImageFromTexture(uiTarget.texture);
    ImageFlipVertical(&uiShot);
    assert(ExportImage(uiShot, "audit/rewards_rarities_rerolled.png"));
    UnloadImage(uiShot);

    /* Captura adicional com acentos nos textos novos (Bíblia Empurrão/órbita/Poças, Arma de fogo até 2 inimigos, Vida máxima) */
    rTest.rerolled = false;
    SetRunWeaponLevel(&rTest.progress, 0);
    rTest.progress.weapon = 1;
    rTest.rewards[0] = (Reward){.kind = REWARD_HEALTH, .rarity = RARITY_COMMON};
    rTest.rewards[1] = (Reward){.kind = REWARD_WEAPON_UPGRADE, .rarity = RARITY_EPIC, .weapon = 1};
    rTest.rewards[2] = (Reward){.kind = REWARD_EPIC_FULL_HEAL, .rarity = RARITY_EPIC};
    BeginTextureMode(uiTarget);
    ClearBackground(BLACK);
    DrawEncounter(&rTest, &dummyEnemies);
    EndTextureMode();
    uiShot = LoadImageFromTexture(uiTarget.texture);
    ImageFlipVertical(&uiShot);
    assert(ExportImage(uiShot, "audit/rewards_accented_cards.png"));
    UnloadImage(uiShot);

    /* Captura visual da tela de Game Over com botões lado a lado */
    BeginTextureMode(uiTarget);
    ClearBackground((Color){16, 10, 22, 255});
    float sw = 1280.0f, sh = 720.0f;
    float centerX = sw / 2.0f, centerY = sh / 2.0f;
    float btnGap = 24.0f;
    float btnWidth = fminf(280.0f, (sw - 80.0f - btnGap) / 2.0f);
    float btnHeight = 64.0f;
    float btnY = centerY + 38.0f;
    Rectangle retry = {centerX - btnWidth - btnGap * 0.5f, btnY, btnWidth, btnHeight};
    Rectangle backToMenu = {centerX + btnGap * 0.5f, btnY, btnWidth, btnHeight};
    DrawGameTextIn("Game Over", (Rectangle){0.0f, centerY - 190.0f, sw, 96.0f},
                   92.0f, (Color){220, 45, 55, 255}, true);
    DrawGameTextIn("Fim da tentativa", (Rectangle){0.0f, centerY - 82.0f, sw, 52.0f},
                   48.0f, (Color){255, 119, 103, 255}, true);
    DrawGameTextIn("Sua tentativa terminou.", (Rectangle){0.0f, centerY - 18.0f, sw, 32.0f},
                   22.0f, RAYWHITE, false);
    DrawRectangleRec(retry, DARKGRAY);
    DrawRectangleLinesEx(retry, 2, GOLD);
    DrawGameTextIn("Repetir", (Rectangle){retry.x + 12.0f, retry.y + 8.0f, retry.width - 24.0f, retry.height - 16.0f},
                   32.0f, RAYWHITE, true);
    DrawRectangleRec(backToMenu, DARKGRAY);
    DrawRectangleLinesEx(backToMenu, 2, GOLD);
    DrawGameTextIn("Voltar ao Menu", (Rectangle){backToMenu.x + 12.0f, backToMenu.y + 8.0f, backToMenu.width - 24.0f, backToMenu.height - 16.0f},
                   32.0f, RAYWHITE, true);
    DrawGameTextIn("Enter ou R: nova tentativa  |  Esc: voltar ao menu",
                   (Rectangle){0.0f, btnY + btnHeight + 24.0f, sw, 28.0f},
                   18.0f, LIGHTGRAY, false);
    EndTextureMode();
    uiShot = LoadImageFromTexture(uiTarget.texture);
    ImageFlipVertical(&uiShot);
    assert(ExportImage(uiShot, "audit/game_over_side_by_side.png"));
    UnloadImage(uiShot);
    UnloadRenderTexture(uiTarget);
}

static void CheckDevTools(Encounter *e, EnemyGroup *enemies, Clearing *map) {
    RunProgress campaign=NewRunProgress(1);
    campaign.stage=3;
    campaign.health=75;
    assert(SaveClearingCheckpoint(&campaign));
    DevTools tools={.wave=1,.weapon=2,.weaponLevel=3,.selected=7};
    Vector2 player;
    int health;
    for (int area=0; area<2; area++) {
        tools.map=area;
        for (int wave=1; wave<=CampaignWaveCount(area); wave++) {
            tools.wave=wave;
            assert(StartDevEncounter(&tools,DEV_STAGE,e,enemies,map,&player,&health));
            assert(e->practice && e->progress.map==area && e->progress.stage==wave);
            assert(e->progress.weapon==2 && e->progress.weaponLevel[2]==3);
            assert(health==100 && EnemiesRemaining(enemies)==(area==CLEARING_MAP ? 14 : 7)+(wave-1)*4);
            assert(!IsMapActorBlocked(map,player));
        }
        assert(StartDevEncounter(&tools,DEV_BOSS_ONE,e,enemies,map,&player,&health));
        assert(e->practice && !e->guardianIntroPending && EnemiesRemaining(enemies)==1);
        assert(enemies->list[0].guardianPhase==GUARDIAN_PHASE_ONE);
        assert(StartDevEncounter(&tools,DEV_BOSS_TWO,e,enemies,map,&player,&health));
        assert(enemies->list[0].guardianPhase==GUARDIAN_PHASE_TWO);
        assert(enemies->list[0].health==enemies->list[0].maxHealth/2);
        assert(StartDevEncounter(&tools,DEV_SEAL,e,enemies,map,&player,&health));
        assert(e->phase==ENCOUNTER_SEAL && e->practice);
        assert(StartDevEncounter(&tools,DEV_STAGE,e,enemies,map,&player,&health));
        for (int i=0; i<ENEMY_COUNT; i++) enemies->list[i]=(Enemy){.state=ENEMY_GONE};
        UpdateEncounter(e,enemies,map,player,&health);
        assert(e->phase==ENCOUNTER_REWARD);
        assert(ChooseEncounterReward(e,enemies,map,player,&health,0));
    }
    assert(StartDevEncounter(&tools,DEV_GUARDIAN,e,enemies,map,&player,&health));
    assert(e->practice && e->guardianIntroPending && !map->cemetery);
    CompleteGuardianIntro(e,true);
    assert(StartDevEncounter(&tools,DEV_LIBERATION,e,enemies,map,&player,&health));
    assert(e->practice && GuardianLiberationPending(e) && EncounterPaused(e));
    CompleteGuardianLiberation(e);
    assert(ClearingAltarOpen(e,map) && !EncounterPaused(e));
    assert(StartDevEncounter(&tools,DEV_PASSAGE,e,enemies,map,&player,&health));
    assert(e->practice && GuardianPassagePending(e) && EncounterPaused(e));
    assert(CompleteGuardianPassage(e,enemies,map,&player,health));
    assert(e->practice && map->cemetery && e->progress.map==1);
    assert(e->progress.guardianAftermath==GUARDIAN_PASSAGE_COMPLETE);
    assert(!CompleteGuardianPassage(e,enemies,map,&player,health));
    /* Fase 5: Usar DevTools para colocar cada arma (0, 1, 2) em cada nível (1 a 5) e verificar estado, dano e cartões */
    tools.map = 0;
    tools.wave = 1;
    for (int w = 0; w < 3; w++) {
        tools.weapon = w;
        for (int lvl = 1; lvl <= MAX_WEAPON_LEVEL; lvl++) {
            tools.weaponLevel = lvl;
            assert(StartDevEncounter(&tools, DEV_STAGE, e, enemies, map, &player, &health));
            assert(e->practice && e->progress.weapon == w);
            for (int k = 0; k < 3; k++) assert(e->progress.weaponLevel[k] == lvl);
            float expectedBonus = (w == 1 && lvl >= 1) ? BIBLE_L1_DAMAGE_BONUS
                                : (w == 0 && lvl >= 2) ? GUN_L2_DAMAGE_BONUS : 0.0f;
            float expectedMult = 1.0f + WEAPON_LEVEL_DAMAGE_BONUS * lvl + expectedBonus;
            assert(fabsf(EncounterDamage(e) - expectedMult) < 0.0001f);
        }
    }
    RunProgress unchanged={0};
    assert(LoadClearingCheckpoint(&unchanged));
    assert(unchanged.stage==3 && unchanged.weapon==1 && unchanged.health==75 &&
           !unchanged.guardianIntroSeen);
    tools.weapon=99;
    assert(!StartDevEncounter(&tools,DEV_STAGE,e,enemies,map,&player,&health));
    tools.weapon=2;
    const int widths[]={1280,800};
    for (int i=0; i<2; i++) {
        SetWindowSize(widths[i],widths[i]*9/16);
        RenderTexture2D target=LoadRenderTexture(widths[i],widths[i]*9/16);
        BeginTextureMode(target);
        ClearBackground(BLACK);
        DrawDevTools(&tools,true);
        EndTextureMode();
        Image shot=LoadImageFromTexture(target.texture);
        ImageFlipVertical(&shot);
        assert(ExportImage(shot,TextFormat("audit/dev_tools_%d.png",widths[i])));
        UnloadImage(shot);
        UnloadRenderTexture(target);
    }

    SetWindowSize(1280, 720);
    tools.map = BONELAND_MAP;
    tools.wave = 1;
    assert(StartDevEncounter(&tools, DEV_STAGE, e, enemies, map, &player, &health));
    assert(e->practice && map->boneland && enemies->boneland && EnemiesRemaining(enemies) == 20);
    assert(e->presentation.areaEntry && e->presentation.waveTotal == 4 && EncounterPaused(e));
    assert(strcmp(e->presentation.mapName, "Terra dos Ossos") == 0);
    UpdateStagePresentation(&e->presentation, .8f);
    Capture("audit/boneland_area_intro.png", e, enemies, map, player);
    UpdateStagePresentation(&e->presentation, e->presentation.remaining);
    assert(!e->presentation.areaEntry && e->presentation.wave == 1 && e->presentation.waveTotal == 4);
    UpdateStagePresentation(&e->presentation, .6f);
    Capture("audit/boneland_wave_intro.png", e, enemies, map, player);
    UpdateStagePresentation(&e->presentation, e->presentation.remaining);
    assert(!EncounterPaused(e));
    Image ghoul = LoadImageFromTexture(enemies->bonelandGhoul);
    Image cultist = LoadImageFromTexture(enemies->bonelandCultist);
    assert(ghoul.data && cultist.data);
    assert(GetImageColor(ghoul, 5, 5).a == 0 && GetImageColor(cultist, 5, 5).a == 0);
    assert(GetImageColor(ghoul, 155, 120).a > 0 && GetImageColor(cultist, 175, 150).a > 0);
    UnloadImage(ghoul);
    UnloadImage(cultist);
    for (int i = 0; i < ENEMY_COUNT; i++) enemies->list[i].state = ENEMY_GONE;
    for (int i = 0; i < 8; i++) {
        enemies->list[i] = (Enemy){
            .type = i < 4 ? ENEMY_COMMON_MELEE : ENEMY_COMMON_RANGED,
            .state = ENEMY_ATTACK, .stateTime = .5f, .attackConnected = true,
            .facing = i % 4, .position = {550 + 190 * (i % 4), i < 4 ? 415 : 645}
        };
    }
    Capture("audit/boneland_attack_poses.png", e, enemies, map, player);
    assert(StartDevEncounter(&tools, DEV_BOSS_ONE, e, enemies, map, &player, &health));
    assert(e->presentation.areaEntry && e->presentation.wave == 4 && e->presentation.waveTotal == 4);
    assert(enemies->boneLordWalk.id && enemies->boneLordPounce.id && enemies->boneLordClaw.id);
    assert(enemies->boneLordDash.id && enemies->boneLordBite.id && enemies->boneLordPortrait.id);
    UpdateStagePresentation(&e->presentation, e->presentation.remaining);
    assert(e->presentation.boss && EncounterPaused(e));
    assert(strcmp(e->presentation.bossName, "Rasga-Ossos") == 0);
    UpdateStagePresentation(&e->presentation, .75f);
    Capture("audit/boneland_boss_intro.png", e, enemies, map, player);
    UpdateStagePresentation(&e->presentation, e->presentation.remaining);
    assert(!EncounterPaused(e));
    Enemy *boss = &enemies->list[0];
    boss->position = (Vector2){835, 530};
    boss->state = ENEMY_ATTACK;
    boss->bossAttack = BONE_CHARGE;
    boss->stateTime = .95f;
    boss->facing = 2;
    Capture("audit/boneland_boss_attack.png", e, enemies, map, player);
}

static void CheckSealDifficultyTableAndGlyphFallback(Encounter *e, EnemyGroup *enemies, Clearing *map) {
    /* 1. Verifica detecção de glifos com LogicSymbols.ttf presente e simulação de ausência */
    assert(GameTextHasGlyph(0x2227) && GameTextHasGlyph(0x2228));
    LoadGameFontsWithSymbolsPath("assets/fonts/DoesNotExist_LogicSymbols.ttf");
    assert(!GameTextHasGlyph(0x2227) && !GameTextHasGlyph(0x2228));

    /* Sem os glifos, OpenEncounterSeal deve gerar no estilo por extenso (sem ∧/∨) */
    Encounter fallbackEnc = *e;
    fallbackEnc.phase = ENCOUNTER_FIGHT;
    fallbackEnc.presentation = (StagePresentation){0};
    fallbackEnc.progress.map = 0;
    fallbackEnc.progress.stage = 1;
    enemies->list[0] = (Enemy){.type = ENEMY_SPECIAL, .state = ENEMY_FALLEN, .position = {400, 400}, .maxHealth = 100};
    assert(OpenEncounterSeal(&fallbackEnc, enemies, (Vector2){400, 400}));
    assert(strstr(fallbackEnc.sealQuestion.vf.exprText, LOGIC_SYM_AND) == NULL);
    assert(strstr(fallbackEnc.sealQuestion.vf.exprText, LOGIC_SYM_OR) == NULL);
    assert(strstr(fallbackEnc.sealQuestion.vf.exprText, "~") == NULL);

    enemies->list[0] = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_FALLEN, .position = {400, 400}, .maxHealth = 2200};
    fallbackEnc.phase = ENCOUNTER_FIGHT;
    assert(OpenEncounterSeal(&fallbackEnc, enemies, (Vector2){400, 400}));
    assert(strstr(fallbackEnc.sealQuestion.mc.questionText, LOGIC_SYM_AND) == NULL);
    assert(strstr(fallbackEnc.sealQuestion.mc.questionText, LOGIC_SYM_OR) == NULL);
    assert(strstr(fallbackEnc.sealQuestion.mc.questionText, "~") == NULL);

    /* Restaura as fontes com LogicSymbols.ttf */
    LoadGameFonts();
    assert(GameTextHasGlyph(0x2227) && GameTextHasGlyph(0x2228));

    /* 2. Testes estruturais para TODAS as linhas da tabela de dificuldade */
    int tableCount = EncounterSealTableCount();
    int prevBossMin = -1;
    int maxBossMin = -1, maxBossMax = -1;
    int necroMin = -1, necroMax = -1;
    LogicRng rng;
    LogicRngSeed(&rng, 20261122u);

    for (int m = 0; m < tableCount; m++) {
        const EncounterSealDifficultyRow *row = EncounterSealTableRow(m);
        if (m == 3 || m == 4 || m == 5) {
            assert(row == NULL);
            /* Mapa sem linha usa a linha de ID menor mais próxima (mapa 2) sem falhar */
            int fbMin = 0, fbMax = 0;
            assert(!EncounterSealRange(m, true, &fbMin, &fbMax));
            assert(fbMin == 6 && fbMax == 7);
            assert(!EncounterSealRange(m, false, &fbMin, &fbMax));
            assert(fbMin == 4 && fbMax == 5);
            continue;
        }
        assert(row != NULL && row->map == m);
        assert(row->hasBossMC);
        if (row->hasSpecialVF) {
            assert(row->specialMin <= row->specialMax);
            /* Boss > especiais no mesmo mapa */
            assert(row->bossMin > row->specialMax || (row->bossMin >= row->specialMax && row->bossMax > row->specialMax));
            assert(row->bossMin > row->specialMin && row->bossMax > row->specialMax);
            /* Faixa V/F atingível pelo gerador */
            LogicVFQuestion vfq;
            assert(LogicGenerateVF(&rng, row->specialMin, row->specialMax, LOGIC_STYLE_SYMBOLS, &vfq));
            assert(vfq.score >= row->specialMin && vfq.score <= row->specialMax);
        }
        assert(row->bossMin <= row->bossMax);
        /* O mínimo dos bosses cresce a cada mapa */
        assert(row->bossMin > prevBossMin);
        prevBossMin = row->bossMin;
        if (row->bossMin > maxBossMin) maxBossMin = row->bossMin;
        if (row->bossMax > maxBossMax) maxBossMax = row->bossMax;
        if (m == NECROMANCER_MAP) {
            necroMin = row->bossMin;
            necroMax = row->bossMax;
        }
        /* Faixa de boss atingível pelo gerador */
        LogicMCQuestion mcq;
        assert(LogicGenerateMC(&rng, row->bossMin, row->bossMax, LOGIC_STYLE_SYMBOLS, &mcq));
        assert(mcq.questionScore >= row->bossMin && mcq.questionScore <= row->bossMax);
    }
    /* O Necromante é o mais difícil */
    assert(necroMin == maxBossMin && necroMax == maxBossMax);

    /* 3. Capturas visuais de auditoria dos selos em 1280x720 e 800x450:
       V/F, 3 opções de boss e selo final (incluindo ~, ~~, ∧, ∨ e pior caso de texto) */
    const int res[2][2] = {{1280, 720}, {800, 450}};
    for (int r = 0; r < 2; r++) {
        SetWindowSize(res[r][0], res[r][1]);
        Encounter vis = *e;
        vis.presentation = (StagePresentation){0};
        vis.phase = ENCOUNTER_SEAL;
        vis.sealEnemy = 0;

        /* V/F com ~, ~~, ∧ e ∨ */
        vis.progress.map = 0;
        enemies->list[0] = (Enemy){.type = ENEMY_SPECIAL, .state = ENEMY_FALLEN, .position = {640, 360}, .maxHealth = 100};
        vis.sealQuestion.isBoss = false;
        snprintf(vis.sealQuestion.vf.valuesText, sizeof(vis.sealQuestion.vf.valuesText), "P = V   Q = F   R = V");
        snprintf(vis.sealQuestion.vf.exprText, sizeof(vis.sealQuestion.vf.exprText),
                 "~(~~P " LOGIC_SYM_AND " (~Q " LOGIC_SYM_OR " R))");
        char capPath[64];
        snprintf(capPath, sizeof(capPath), "audit/seal_vf_symbols_%dx%d.png", res[r][0], res[r][1]);
        Capture(capPath, &vis, enemies, map, (Vector2){640, 360});

        /* Múltipla escolha de boss (3 opções) com ~, ~~, ∧, ∨ */
        vis.progress.map = 0;
        enemies->list[0] = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_FALLEN, .position = {640, 360}, .maxHealth = 2200};
        vis.sealQuestion.isBoss = true;
        snprintf(vis.sealQuestion.mc.questionText, sizeof(vis.sealQuestion.mc.questionText),
                 "~(~~P " LOGIC_SYM_AND " (Q " LOGIC_SYM_OR " ~R))");
        snprintf(vis.sealQuestion.mc.choices[0], sizeof(vis.sealQuestion.mc.choices[0]),
                 "~P " LOGIC_SYM_OR " (~Q " LOGIC_SYM_AND " R)");
        snprintf(vis.sealQuestion.mc.choices[1], sizeof(vis.sealQuestion.mc.choices[1]),
                 "~P " LOGIC_SYM_AND " (~Q " LOGIC_SYM_OR " R)");
        snprintf(vis.sealQuestion.mc.choices[2], sizeof(vis.sealQuestion.mc.choices[2]),
                 "~~P " LOGIC_SYM_OR " (~Q " LOGIC_SYM_AND " ~R)");
        snprintf(capPath, sizeof(capPath), "audit/seal_boss_symbols_%dx%d.png", res[r][0], res[r][1]);
        Capture(capPath, &vis, enemies, map, (Vector2){640, 360});

        /* Selo final (Necromante) - pior caso de comprimento de texto */
        vis.progress.map = NECROMANCER_MAP;
        vis.progress.finalSealFailures = 1;
        snprintf(vis.sealQuestion.mc.questionText, sizeof(vis.sealQuestion.mc.questionText),
                 "~(P " LOGIC_SYM_AND " Q) " LOGIC_SYM_AND " ~(P " LOGIC_SYM_AND " ~Q) " LOGIC_SYM_AND " (Q " LOGIC_SYM_OR " R)");
        snprintf(vis.sealQuestion.mc.choices[0], sizeof(vis.sealQuestion.mc.choices[0]),
                 "~P " LOGIC_SYM_AND " (Q " LOGIC_SYM_OR " R)");
        snprintf(vis.sealQuestion.mc.choices[1], sizeof(vis.sealQuestion.mc.choices[1]),
                 "~P " LOGIC_SYM_OR " (Q " LOGIC_SYM_AND " ~R)");
        snprintf(vis.sealQuestion.mc.choices[2], sizeof(vis.sealQuestion.mc.choices[2]),
                 "(~P " LOGIC_SYM_AND " Q) " LOGIC_SYM_OR " (~P " LOGIC_SYM_AND " ~R)");
        snprintf(capPath, sizeof(capPath), "audit/seal_final_symbols_%dx%d.png", res[r][0], res[r][1]);
        Capture(capPath, &vis, enemies, map, (Vector2){640, 360});
    }
    for (int i = 0; i < ENEMY_COUNT; i++) enemies->list[i] = (Enemy){.state = ENEMY_GONE};
    SetWindowSize(1280, 720);
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"Encounter audit");
    assert(IsWindowReady());
    LoadGameFonts();
    CheckLegacyCheckpoints();
    Clearing clearing={0};
    EnemyGroup enemies={0};
    assert(InitClearing(&clearing));
    Vector2 pedestalApproach;
    assert(FindFreeMapPosition(&clearing, clearing.pedestal, &pedestalApproach));
    assert(CheckCollisionPointCircle(pedestalApproach, clearing.pedestal, CLEARING_ALTAR_RADIUS));
    assert(clearing.exit.width == 0);
    assert(InitEnemies(&enemies));
    Encounter encounter={0};
    CheckSealDifficultyTableAndGlyphFallback(&encounter, &enemies, &clearing);
    Vector2 player={835,540};
    int health=100;
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(0),player);
    assert(encounter.presentation.areaEntry && EncounterPaused(&encounter));
    assert(strcmp(encounter.presentation.mapName,"Clareira do Selo")==0);
    UpdateStagePresentation(&encounter.presentation,.7f);
    Capture("audit/clearing_area_entry.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,2.2f);
    assert(!encounter.presentation.areaEntry && encounter.presentation.wave==1);
    assert(EncounterPaused(&encounter));
    Capture("audit/clearing_collision_updated.png",&encounter,&enemies,&clearing,clearing.spawn);
    EnemyGroup meleePreview = enemies;
    for (int i=0;i<ENEMY_COUNT;i++) meleePreview.list[i]=(Enemy){.state=ENEMY_GONE};
    for (int row=0;row<3;row++) {
        for (int facing=0;facing<4;facing++) {
            meleePreview.list[row*4+facing]=(Enemy){.type=ENEMY_COMMON_MELEE,
                .state=row==0 ? ENEMY_IDLE : row==1 ? ENEMY_WALK : ENEMY_ATTACK,
                .stateTime=.34f,.walkTime=.28f,.facing=facing,
                .position={455+220*facing,345+165*row},.health=45,.maxHealth=45};
        }
    }
    Capture("audit/clearing_melee_directions.png",&encounter,&meleePreview,&clearing,player);
    for (int i=0;i<12;i++) meleePreview.list[i].type=ENEMY_COMMON_RANGED;
    Capture("audit/clearing_archer_directions.png",&encounter,&meleePreview,&clearing,player);
    for (int i=0;i<12;i++) {
        meleePreview.list[i].type=ENEMY_SPECIAL;
        meleePreview.list[i].stateTime=.52f;
    }
    meleePreview.list[12]=(Enemy){.type=ENEMY_SPECIAL,.state=ENEMY_FALLEN,
        .facing=3,.position={800,870},.maxHealth=100};
    Capture("audit/clearing_special_directions.png",&encounter,&meleePreview,&clearing,player);
    for (int wave=1;wave<=CLEARING_WAVES;wave++) {
        assert(encounter.progress.stage==wave);
        assert(EnemiesRemaining(&enemies)==14+(wave-1)*4);
        assert(EncounterPaused(&encounter));
        UpdateStagePresentation(&encounter.presentation,.1f);
        assert(EncounterPaused(&encounter));
        UpdateStagePresentation(&encounter.presentation,10);
        assert(!EncounterPaused(&encounter));
        int specials=0;
        for (int i=0;i<ENEMY_COUNT;i++) {
            Enemy *enemy=&enemies.list[i];
            if (enemy->state==ENEMY_GONE) continue;
            assert(enemy->state==ENEMY_SPAWNING);
            assert(!EnemyCanBeHit(enemy));
            assert(!IsClearingBlocked(&clearing,enemy->position));
            if (enemy->type==ENEMY_SPECIAL) specials++;
            enemy->state=ENEMY_IDLE;
            DamageEnemy(enemy,enemy->health);
        }
        assert(specials>=1+(wave-1)/2 && specials<=1+(wave-1)/2+(wave>=3));
        UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
        assert(EnemiesRemaining(&enemies)==specials);
        for (int i=0;i<ENEMY_COUNT;i++) if (enemies.list[i].state==ENEMY_FALLEN) {
            Enemy *enemy=&enemies.list[i];
            assert(OpenEncounterSeal(&encounter,&enemies,enemy->position));
            assert(EncounterPaused(&encounter));
            int correct=encounter.answer;
            if (wave==1) {
                int old=enemy->maxHealth;
                assert(AnswerEncounterSeal(&encounter,&enemies,&health,1-correct));
                assert(enemy->maxHealth>old && EnemyCanBeHit(enemy));
                DamageEnemy(enemy,enemy->health);
                UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
                assert(OpenEncounterSeal(&encounter,&enemies,enemy->position));
                correct=encounter.answer;
            }
            assert(AnswerEncounterSeal(&encounter,&enemies,&health,correct));
        }
        assert(EnemiesRemaining(&enemies)==0);
        UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
        assert(encounter.phase==ENCOUNTER_REWARD);
        assert(encounter.rewardCount==3);
        if (wave==1) Capture("audit/rewards.png",&encounter,&enemies,&clearing,player);
        if (wave==CLEARING_WAVES) Capture("audit/rewards_epic_wave3.png",&encounter,&enemies,&clearing,player);
        assert(!ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,9));
        assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
        RunProgress saved={0};
        assert(LoadClearingCheckpoint(&saved));
        assert(saved.stage==(wave==CLEARING_WAVES ? CLEARING_BOSS_STAGE : wave+1) &&
               saved.weapon==encounter.progress.weapon);
    }
    assert(encounter.progress.stage==CLEARING_BOSS_STAGE && EnemiesRemaining(&enemies)==1);
    assert(encounter.presentation.boss && EncounterPaused(&encounter));
    assert(encounter.guardianIntroPending && !encounter.progress.guardianIntroSeen);
    encounter.presentation.remaining=0;
    assert(EncounterPaused(&encounter));
    assert(!EncounterBossVisible(&encounter,&enemies));
    assert(!UpdateEncounter(&encounter,&enemies,&clearing,player,&health));
    CompleteGuardianIntro(&encounter,false);
    assert(!encounter.guardianIntroPending && encounter.progress.guardianIntroSeen);
    RunProgress afterIntro={0};
    assert(LoadClearingCheckpoint(&afterIntro) && afterIntro.guardianIntroSeen);
    Encounter cinematicResume={.progress=afterIntro,.guardianIntroPending=true,
        .presentation={.areaEntry=true,.duration=2.8f,.remaining=2.8f},.practice=true};
    CompleteGuardianIntro(&cinematicResume,true);
    UpdateStagePresentation(&cinematicResume.presentation,.05f);
    assert(!EncounterPaused(&cinematicResume) && !cinematicResume.presentation.areaEntry);
    BeginEncounter(&encounter,&enemies,&clearing,afterIntro,player);
    assert(!encounter.guardianIntroPending);
    UpdateStagePresentation(&encounter.presentation,2.8f);
    UpdateStagePresentation(&encounter.presentation,.7f);
    Capture("audit/boss_intro.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,10);
    assert(!EncounterPaused(&encounter));
    Enemy *boss=&enemies.list[0];
    assert(boss->type==ENEMY_BOSS && EnemyHurtRadius(boss)>20);
    boss->position=(Vector2){player.x-200,player.y};
    Vector2 spawnPosition = boss->position;
    health=100;
    assert(boss->state==ENEMY_SPAWNING && !EnemyCanBeHit(boss));
    Capture("audit/guardian_spawn_start.png",&encounter,&enemies,&clearing,player);
    for (int frame=0;frame<90;frame++) UpdateEnemies(&enemies,&clearing,player,1.0f/60,false,&health,100);
    assert(boss->state==ENEMY_SPAWNING);
    Capture("audit/guardian_spawn_middle.png",&encounter,&enemies,&clearing,player);
    for (int frame=90;frame<179;frame++) UpdateEnemies(&enemies,&clearing,player,1.0f/60,false,&health,100);
    DamageEnemy(boss,10);
    assert(boss->state==ENEMY_SPAWNING && boss->health==boss->maxHealth && health==100);
    assert(boss->position.x==spawnPosition.x && boss->position.y==spawnPosition.y);
    assert(!enemies.rootAttacks[0].active);
    Capture("audit/guardian_spawn_end.png",&encounter,&enemies,&clearing,player);
    UpdateEnemies(&enemies,&clearing,player,2.0f/60,false,&health,100);
    assert(boss->state==ENEMY_IDLE && EnemyCanBeHit(boss) && health==100);
    Capture("audit/guardian_spawn_idle.png",&encounter,&enemies,&clearing,player);
    boss->attackCooldown=0;
    UpdateEnemies(&enemies,&clearing,player,.05f,false,&health,100);
    assert(boss->state==ENEMY_ATTACK);
    DamageEnemy(boss,10);
    assert(boss->state==ENEMY_ATTACK); /* normal hits cannot cancel every special */
    UpdateEnemies(&enemies,&clearing,player,.65f,false,&health,100);
    assert(health==100); /* visible warning has not expired */
    UpdateEnemies(&enemies,&clearing,player,.15f,false,&health,100);
    assert(health<100);
    Capture("audit/boss_roots.png",&encounter,&enemies,&clearing,player);
    DamageEnemy(boss,boss->health);
    assert(boss->health==boss->maxHealth/2 && !EnemyCanBeHit(boss));
    UpdateEnemies(&enemies,&clearing,player,1.0f,true,&health,100);
    assert(boss->guardianPhase==GUARDIAN_PHASE_TWO && EnemyCanBeHit(boss));
    DamageEnemy(boss,boss->health);
    assert(boss->state==ENEMY_DIE && !EnemyCanBeHit(boss));
    assert(!OpenEncounterSeal(&encounter,&enemies,boss->position));
    EnemyGroup defeatDirections = enemies;
    for (int direction=0;direction<4;direction++) {
        defeatDirections.list[direction] = *boss;
        defeatDirections.list[direction].facing = direction;
        defeatDirections.list[direction].position = (Vector2){player.x-330+direction*200,player.y};
    }
    Capture("audit/guardian_defeat_directions.png",&encounter,&defeatDirections,&clearing,player);
    UpdateEnemies(&enemies,&clearing,player,.45f,true,&health,100);
    assert(boss->state==ENEMY_DIE && !OpenEncounterSeal(&encounter,&enemies,boss->position));
    assert(boss->position.x==spawnPosition.x && boss->position.y==spawnPosition.y);
    Capture("audit/guardian_defeat_middle.png",&encounter,&enemies,&clearing,player);
    UpdateEnemies(&enemies,&clearing,player,.45f,true,&health,100);
    assert(boss->state==ENEMY_FALLEN && !EnemyCanBeHit(boss));
    Capture("audit/guardian_defeat_fallen.png",&encounter,&enemies,&clearing,player);
    assert(!ClearingAltarOpen(&encounter,&clearing));
    assert(!CanEnterCemetery(&encounter,&clearing,pedestalApproach,true));
    for (int i=0;i<ENEMY_ROOT_COUNT;i++) assert(!enemies.rootAttacks[i].active);
    assert(OpenEncounterSeal(&encounter,&enemies,boss->position));
    Capture("audit/boss_seal.png",&encounter,&enemies,&clearing,player);
    float oldPower=encounter.progress.bossPower;
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,(encounter.answer+1)%3));
    assert(!GuardianLiberationPending(&encounter));
    assert(encounter.progress.guardianAftermath==GUARDIAN_UNPURIFIED);
    assert(encounter.progress.bossPower>oldPower && boss->health==boss->maxHealth);
    RunProgress revanche;
    assert(LoadClearingCheckpoint(&revanche));
    assert(revanche.stage==CLEARING_BOSS_STAGE && revanche.bossPower>1);
    DamageEnemy(boss,boss->health);
    assert(boss->health==boss->maxHealth/2 && !EnemyCanBeHit(boss));
    UpdateEnemies(&enemies,&clearing,player,1.0f,true,&health,100);
    assert(boss->guardianPhase==GUARDIAN_PHASE_TWO && EnemyCanBeHit(boss));
    DamageEnemy(boss,boss->health);
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,100);
    assert(OpenEncounterSeal(&encounter,&enemies,boss->position));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,encounter.answer));
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR && encounter.progress.stage==CLEARING_COMPLETE_STAGE);
    assert(!encounter.notice[0]);
    assert(GuardianLiberationPending(&encounter) && EncounterPaused(&encounter));
    assert(!ClearingAltarOpen(&encounter,&clearing));
    assert(!CanEnterCemetery(&encounter,&clearing,pedestalApproach,true));
    RunProgress pending;
    assert(LoadClearingCheckpoint(&pending) && pending.guardianAftermath==GUARDIAN_LIBERATION_PENDING);
    BeginEncounter(&encounter,&enemies,&clearing,pending,player);
    assert(GuardianLiberationPending(&encounter) && EnemiesRemaining(&enemies)==0);
    CompleteGuardianLiberation(&encounter);
    CompleteGuardianLiberation(&encounter); /* Repeated completion is harmless. */
    assert(ClearingAltarOpen(&encounter,&clearing));
    assert(!EncounterPaused(&encounter));
    assert(!CanEnterCemetery(&encounter,&clearing,pedestalApproach,false));
    Vector2 formerExit = {clearing.background.width*.46f, clearing.background.height*.985f};
    assert(!CanEnterCemetery(&encounter,&clearing,formerExit,true));
    assert(CanEnterCemetery(&encounter,&clearing,pedestalApproach,true));
    Capture("audit/clearing_altar_open.png",&encounter,&enemies,&clearing,pedestalApproach);
    RunProgress completed;
    assert(LoadClearingCheckpoint(&completed) && completed.stage==CLEARING_COMPLETE_STAGE);
    assert(completed.guardianAftermath==GUARDIAN_ALTAR_READY);
    assert(!BeginGuardianPassage(&encounter,&clearing,formerExit,true));
    assert(!BeginGuardianPassage(&encounter,&clearing,pedestalApproach,false));
    assert(BeginGuardianPassage(&encounter,&clearing,pedestalApproach,true));
    assert(!BeginGuardianPassage(&encounter,&clearing,pedestalApproach,true));
    assert(EncounterPaused(&encounter));
    assert(LoadClearingCheckpoint(&completed) && completed.guardianAftermath==GUARDIAN_PASSAGE_PENDING);
    BeginEncounter(&encounter,&enemies,&clearing,completed,pedestalApproach);
    assert(GuardianPassagePending(&encounter));
    assert(CompleteGuardianPassage(&encounter,&enemies,&clearing,&player,health));
    assert(clearing.cemetery && encounter.progress.map==1 && encounter.progress.stage==1);
    assert(!GuardianPassagePending(&encounter));
    assert(LoadClearingCheckpoint(&completed) && completed.map==1 &&
           completed.guardianAftermath==GUARDIAN_PASSAGE_COMPLETE);
    health=0;
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_DEFEAT && EncounterPaused(&encounter));
    UnloadClearing(&clearing);
    clearing=(Clearing){0};
    assert(InitCemetery(&clearing));
    enemies.cemetery=true;
    encounter.phase=ENCOUNTER_FIGHT;
    encounter.notice[0]=0;
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    for(int kind=0;kind<3;kind++) for(int face=0;face<4;face++) {
        int at=kind*4+face;
        enemies.list[at]=(Enemy){.position={380+face*270,300+kind*185},
            .type=kind==0?ENEMY_COMMON_MELEE:kind==1?ENEMY_COMMON_RANGED:ENEMY_BOSS,
            .state=ENEMY_WALK,.walkTime=.25f,.facing=face,.health=100,.maxHealth=100};
    }
    encounter.presentation.remaining=0;
    Capture("audit/cemetery_directions_v3.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    for (int face=0;face<4;face++) {
        enemies.list[face]=(Enemy){.position={380+face*270,520},.type=ENEMY_COMMON_RANGED,
            .state=ENEMY_ATTACK,.stateTime=CEM_LANTERN_RELEASE_TIME,.facing=face,.health=50,.maxHealth=50};
    }
    enemies.projectiles[0]=(EnemyProjectile){{800,455},{1,0},3.76f,10};
    enemies.impacts[0]=(EnemyImpact){.position={920,450},.remaining=.27f,.direction={1,0}};
    Capture("audit/cemetery_lantern_cast.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    ClearEnemyAttacks(&enemies);
    const EnemyState specialStates[]={ENEMY_IDLE, ENEMY_WALK, ENEMY_ATTACK, ENEMY_ATTACK};
    for (int row=0;row<4;row++) for (int face=0;face<4;face++) {
        enemies.list[row*4+face]=(Enemy){.position={430+face*250,290+row*155},
            .type=ENEMY_SPECIAL,.state=specialStates[row],.facing=face,.walkTime=.3f,
            .stateTime=row==3?CEM_SPECIAL_IMPACT_TIME:.35f,.health=110,.maxHealth=110};
    }
    Capture("audit/cemetery_special_directions.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    for(int attack=0;attack<3;attack++) for(int beat=0;beat<4;beat++) {
        enemies.list[attack*4+beat]=(Enemy){.position={380+beat*270,300+attack*185},
            .type=ENEMY_BOSS,.state=ENEMY_ATTACK,.bossAttack=attack,
            .stateTime=(float[]){.15f,.55f,.95f,1.8f}[beat],.facing=3,
            .health=2420,.maxHealth=2420};
    }
    Capture("audit/cemetery_attack_poses_v3.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    enemies.list[0]=(Enemy){.position={500,470},.type=ENEMY_BOSS,.state=ENEMY_ATTACK,
        .bossAttack=COVEIRO_SHOVEL,.facing=3,.stateTime=.95f,.health=2420,.maxHealth=2420};
    for(int i=0;i<3;i++) enemies.cemeteryProjectiles[i]=(CemeteryProjectile){.active=true,
        .position={850+i*150,350+i*110},.velocity={740,(i-1)*180},.life=1.9f-i*.08f};
    Capture("audit/cemetery_attack_poses_shovel_v4.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    ClearEnemyAttacks(&enemies);
    for(int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    for(int face=0;face<4;face++) for(int beat=0;beat<4;beat++) {
        enemies.list[face*4+beat]=(Enemy){.position={380+beat*270,250+face*155},
            .type=ENEMY_BOSS,.state=ENEMY_ATTACK,.guardianMelee=true,.facing=face,
            .stateTime=(float[]){.10f,.28f,.40f,.65f}[beat],.health=2420,.maxHealth=2420};
    }
    Capture("audit/cemetery_melee_poses_v4.png",&encounter,&enemies,&clearing,(Vector2){30,30});
    ClearEnemyAttacks(&enemies);
    player=clearing.spawn;health=100;
    RunProgress cemeteryProgress=NewRunProgress(0);
    cemeteryProgress.damage=1.4f;
    BeginEncounter(&encounter,&enemies,&clearing,cemeteryProgress,player);
    assert(encounter.progress.map==1 && encounter.progress.damage==1.4f);
    UpdateStagePresentation(&encounter.presentation,.6f);
    Capture("audit/cemetery_wave.png",&encounter,&enemies,&clearing,player);
    Capture("audit/cemetery_collision_updated.png",&encounter,&enemies,&clearing,player);
    for (int wave=1;wave<=CEMETERY_WAVES;wave++) {
        assert(encounter.progress.stage==wave);
        assert(EnemiesRemaining(&enemies)==7+(wave-1)*4);
        UpdateStagePresentation(&encounter.presentation,10);
        for (int i=0;i<ENEMY_COUNT;i++) {
            Enemy *enemy=&enemies.list[i];
            if (enemy->state==ENEMY_GONE) continue;
            assert(!IsMapActorBlocked(&clearing,enemy->position));
            enemy->state=ENEMY_IDLE;
            DamageEnemy(enemy,enemy->health);
        }
        UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,encounter.progress.maxHealth);
        assert(enemies.list[0].type==ENEMY_SPECIAL && enemies.list[0].state==ENEMY_FALLEN);
        for (int i=0;i<ENEMY_COUNT;i++) if (enemies.list[i].state==ENEMY_FALLEN) {
            assert(OpenEncounterSeal(&encounter,&enemies,enemies.list[i].position));
            assert(encounter.sealQuestion.score>=3 && encounter.sealQuestion.score<=4);
            int correct=encounter.answer;
            assert(AnswerEncounterSeal(&encounter,&enemies,&health,correct));
        }
        UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
        assert(encounter.phase==ENCOUNTER_REWARD);
        assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
        RunProgress saved;
        assert(LoadClearingCheckpoint(&saved) && saved.map==1 && saved.stage==wave+1);
    }
    assert(encounter.progress.stage==CLEARING_BOSS_STAGE && EnemiesRemaining(&enemies)==1);
    UpdateStagePresentation(&encounter.presentation,.7f);
    Capture("audit/cemetery_boss_intro.png",&encounter,&enemies,&clearing,player);
    UpdateStagePresentation(&encounter.presentation,10);
    assert(encounter.guardianIntroPending && EncounterPaused(&encounter));
    assert(!EncounterBossVisible(&encounter,&enemies));
    CompleteGuardianIntro(&encounter,true);
    assert(!EncounterPaused(&encounter));
    assert(EncounterBossVisible(&encounter,&enemies) && enemies.list[0].health==2420);
    Capture("audit/cemetery_boss_hud.png",&encounter,&enemies,&clearing,player);
    enemies.list[0].state=ENEMY_IDLE;
    DamageEnemy(&enemies.list[0],enemies.list[0].health);
    UpdateEnemies(&enemies,&clearing,player,1.0f,true,&health,encounter.progress.maxHealth);
    assert(enemies.list[0].guardianPhase==GUARDIAN_PHASE_TWO);
    DamageEnemy(&enemies.list[0],enemies.list[0].health);
    UpdateEnemies(&enemies,&clearing,player,.9f,true,&health,encounter.progress.maxHealth);
    assert(enemies.list[0].state==ENEMY_FALLEN);
    assert(OpenEncounterSeal(&encounter,&enemies,enemies.list[0].position));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,encounter.answer));
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR);
    assert(strcmp(encounter.presentation.announcement,"CEMITERIO LIMPO")==0);

    RunProgress normalSave=NewRunProgress(1);normalSave.stage=3;
    assert(SaveClearingCheckpoint(&normalSave));
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(0),player);
    encounter.practice=true;
    UpdateStagePresentation(&encounter.presentation,10);
    for (int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(ChooseEncounterReward(&encounter,&enemies,&clearing,player,&health,0));
    RunProgress preserved;
    assert(LoadClearingCheckpoint(&preserved));
    assert(preserved.weapon==1 && preserved.stage==3 && preserved.map==0);
    UpdateStagePresentation(&encounter.presentation,10);
    enemies.list[0]=(Enemy){.type=ENEMY_BOSS,.state=ENEMY_FALLEN,
        .position=player,.maxHealth=100,.power=1};
    assert(OpenEncounterSeal(&encounter,&enemies,player));
    assert(AnswerEncounterSeal(&encounter,&enemies,&health,(encounter.answer+1)%3));
    assert(LoadClearingCheckpoint(&preserved) && preserved.stage==3 && preserved.map==0);
    for (int i=0;i<ENEMY_COUNT;i++) enemies.list[i]=(Enemy){.state=ENEMY_GONE};
    encounter.progress.stage=CLEARING_BOSS_STAGE;
    UpdateEncounter(&encounter,&enemies,&clearing,player,&health);
    assert(encounter.phase==ENCOUNTER_CLEAR);
    assert(LoadClearingCheckpoint(&preserved) && preserved.stage==3 && preserved.map==0);

    Clearing bones = {0};
    assert(InitBoneland(&bones));
    assert(!IsMapActorBlocked(&bones, bones.spawn));
    assert(MapHasClearPath(&bones, bones.spawn, (Vector2){835, 460}));
    assert(IsClearingBlocked(&bones, (Vector2){336, 170}));
    assert(!IsMapActorBlocked(&bones, (Vector2){1100, 227}));
    Encounter boneEncounter;
    RunProgress boneProgress = NewRunProgress(0);
    int boneHealth = boneProgress.health;
    BeginEncounter(&boneEncounter, &enemies, &bones, boneProgress, bones.spawn);
    assert(boneEncounter.progress.map == BONELAND_MAP && EnemiesRemaining(&enemies) == 20);
    boneEncounter.practice = true;
    boneEncounter.phase = ENCOUNTER_REWARD;
    boneEncounter.rewardCount = 1;
    assert(ChooseEncounterReward(&boneEncounter, &enemies, &bones, bones.spawn, &boneHealth, 0));
    assert(boneEncounter.progress.stage == 2 && EnemiesRemaining(&enemies) == 26);
    boneEncounter.phase = ENCOUNTER_REWARD;
    boneEncounter.rewardCount = 1;
    assert(ChooseEncounterReward(&boneEncounter, &enemies, &bones, bones.spawn, &boneHealth, 0));
    assert(boneEncounter.progress.stage == 3 && EnemiesRemaining(&enemies) == 30);
    RunProgress boneResume;
    assert(SaveClearingCheckpoint(&boneEncounter.progress));
    assert(LoadClearingCheckpoint(&boneResume));
    assert(boneResume.map == BONELAND_MAP && boneResume.stage == 3);
    boneEncounter.phase = ENCOUNTER_REWARD;
    boneEncounter.rewardCount = 1;
    assert(ChooseEncounterReward(&boneEncounter, &enemies, &bones, bones.spawn, &boneHealth, 0));
    assert(boneEncounter.progress.stage == CLEARING_BOSS_STAGE);
    assert(boneEncounter.presentation.wave == 4);
    assert(!boneEncounter.guardianIntroPending && enemies.list[0].type == ENEMY_BOSS);
    assert(SaveClearingCheckpoint(&boneEncounter.progress));
    assert(LoadClearingCheckpoint(&boneResume));
    assert(boneResume.map == BONELAND_MAP && boneResume.stage == CLEARING_BOSS_STAGE);
    boneEncounter.presentation=(StagePresentation){0};
    enemies.list[0].state=ENEMY_FALLEN;
    assert(OpenEncounterSeal(&boneEncounter, &enemies, enemies.list[0].position));
    assert(AnswerEncounterSeal(&boneEncounter, &enemies, &boneHealth, boneEncounter.answer));
    UpdateEncounter(&boneEncounter, &enemies, &bones, bones.spawn, &boneHealth);
    assert(boneEncounter.phase == ENCOUNTER_CLEAR);
    Vector2 northPass = {835, 80};
    assert(!IsMapActorBlocked(&bones, northPass));
    assert(MapHasClearPath(&bones, bones.spawn, northPass));
    assert(!CanEnterTemplarField(&boneEncounter, &bones, northPass));
    boneEncounter.presentation = (StagePresentation){0};
    assert(CanEnterTemplarField(&boneEncounter, &bones, northPass));
    assert(!CanEnterTemplarField(&boneEncounter, &bones, bones.spawn));
    UnloadClearing(&bones);

    Clearing field = {0};
    assert(InitCampaignMap(&field, TEMPLAR_FIELD_MAP));
    Vector2 stair = {835, 270};
    assert(IsMapActorBlocked(&field, stair));
    assert(IsMapActorBlocked(&field, (Vector2){90, 215}));
    assert(IsMapActorBlocked(&field, (Vector2){1560, 245}));
    assert(!IsMapActorBlocked(&field, (Vector2){220, 545}));
    assert(!IsMapActorBlocked(&field, (Vector2){1430, 535}));
    assert(!IsMapActorBlocked(&field, field.spawn));
    Encounter templars;
    RunProgress fieldProgress = NewRunProgress(2);
    fieldProgress.angelSceneCompleted = true;
    fieldProgress.angelBlessingUses = 2;
    int fieldHealth = fieldProgress.health;
    BeginEncounter(&templars, &enemies, &field, fieldProgress, field.spawn);
    assert(templars.progress.map == TEMPLAR_FIELD_MAP);
    assert(templars.presentation.areaEntry && templars.presentation.waveTotal == 3);
    assert(templars.presentation.areaSubtitle);
    Enemy shield = {.templarRole = TEMPLAR_SHIELD, .state = ENEMY_IDLE,
                    .position = {100, 100}, .health = 100, .maxHealth = 100};
    DamageEnemyFrom(&shield, 20, (Vector2){100, 140});
    assert(shield.health == 90 && shield.state == ENEMY_IDLE);
    DamageEnemyFrom(&shield, 20, (Vector2){100, 60});
    assert(shield.health == 70 && shield.state == ENEMY_HURT);
    shield.state = ENEMY_ATTACK;
    DamageEnemyFrom(&shield, 20, (Vector2){100, 140});
    assert(shield.health == 50);
    for (int wave = 1; wave <= TEMPLAR_FIELD_WAVES; wave++) {
        assert(templars.progress.stage == wave && EnemiesRemaining(&enemies) == wave + 4);
        assert(enemies.list[0].templarRole == (wave == 1 ? TEMPLAR_SHIELD : TEMPLAR_STANDARD));
        for (int i = 0; i < wave + 4; i++) {
            Enemy *enemy = &enemies.list[i];
            assert(!IsMapActorBlocked(&field, enemy->position));
            enemy->state = ENEMY_IDLE;
            DamageEnemy(enemy, enemy->health * 2);
        }
        UpdateEnemies(&enemies, &field, field.spawn, 1, true, &fieldHealth, 100);
        assert(EnemiesRemaining(&enemies) == 0);
        templars.presentation = (StagePresentation){0};
        UpdateEncounter(&templars, &enemies, &field, field.spawn, &fieldHealth);
        assert(templars.phase == ENCOUNTER_REWARD);
        assert(ChooseEncounterReward(&templars, &enemies, &field, field.spawn, &fieldHealth, 0));
        assert(LoadClearingCheckpoint(&fieldProgress));
        assert(fieldProgress.map == TEMPLAR_FIELD_MAP && fieldProgress.stage == templars.progress.stage);
        assert(fieldProgress.angelSceneCompleted && fieldProgress.angelBlessingUses == 2);
    }
    assert(templars.progress.stage == CLEARING_BOSS_STAGE && EnemiesRemaining(&enemies) == 1);
    assert(enemies.marshalPortrait.id && templars.presentation.separateBossWave);
    assert(templars.presentation.wave == 3 && !templars.presentation.boss);
    UpdateStagePresentation(&templars.presentation, templars.presentation.remaining);
    assert(templars.presentation.boss && templars.presentation.remaining > 0);
    Enemy *marshal = &enemies.list[0];
    assert(marshal->templarRole == TEMPLAR_MARSHAL && !IsMapActorBlocked(&field, marshal->position));
    for (int role = TEMPLAR_SHIELD; role <= TEMPLAR_MARSHAL; role++)
        assert(enemies.templarAttack[role].id);
    marshal->state = ENEMY_IDLE;
    Vector2 marshalStart = marshal->position;
    UpdateEnemies(&enemies, &field, field.spawn, .3f, false, &fieldHealth, 100);
    assert(marshal->state == ENEMY_ATTACK && marshal->bossAttack == MARSHAL_CHARGE);
    marshal->state = ENEMY_IDLE;
    DamageEnemy(marshal, marshal->maxHealth);
    assert(marshal->guardianPhase == GUARDIAN_PHASE_TRANSITION);
    UpdateEnemies(&enemies, &field, field.spawn, 2, true, &fieldHealth, 100);
    assert(marshal->guardianPhase == GUARDIAN_PHASE_TWO);
    marshal->state = ENEMY_ATTACK;
    marshal->bossAttack = MARSHAL_CHARGE;
    marshal->attackDirection = (Vector2){0,1};
    marshal->stateTime = 1.35f;
    UpdateEnemies(&enemies, &field, field.spawn, .05f, false, &fieldHealth, 100);
    assert(marshal->state == ENEMY_ATTACK && marshal->bossAttack == MARSHAL_RETURN);
    marshal->state = ENEMY_IDLE;
    marshal->position = marshalStart;
    marshal->attackCooldown = 0;
    Vector2 target = {marshalStart.x, marshalStart.y + 125};
    UpdateEnemies(&enemies, &field, target, .01f, false, &fieldHealth, 100);
    assert(marshal->bossAttack == MARSHAL_RUPTURE);
    int before = fieldHealth;
    marshal->stateTime = .81f;
    UpdateEnemies(&enemies, &field, target, .02f, true, &fieldHealth, 100);
    assert(fieldHealth == before && enemies.templarBlood.remaining <= 0);
    marshal->stateTime = .81f;
    marshal->attackConnected = false;
    UpdateEnemies(&enemies, &field, target, .02f, false, &fieldHealth, 100);
    assert(fieldHealth < before && enemies.templarBlood.remaining > 0);
    DamageEnemy(marshal, marshal->health);
    UpdateEnemies(&enemies, &field, field.spawn, 1.5f, true, &fieldHealth, 100);
    assert(marshal->state == ENEMY_FALLEN && !EnemyCanBeHit(marshal));
    templars.presentation = (StagePresentation){0};
    UpdateEncounter(&templars, &enemies, &field, field.spawn, &fieldHealth);
    assert(templars.phase == ENCOUNTER_FIGHT && EnemiesRemaining(&enemies) == 1);
    assert(OpenEncounterSeal(&templars, &enemies, marshal->position));
    assert(templars.phase == ENCOUNTER_SEAL);
    float marshalPowerBefore = templars.progress.bossPower;
    assert(AnswerEncounterSeal(&templars, &enemies, &fieldHealth, (templars.answer + 1) % 3));
    assert(templars.phase == ENCOUNTER_FIGHT && templars.progress.bossPower > marshalPowerBefore);
    assert(marshal->health == marshal->maxHealth && EnemyCanBeHit(marshal));
    DamageEnemy(marshal, marshal->maxHealth);
    UpdateEnemies(&enemies, &field, field.spawn, 2, true, &fieldHealth, 100);
    DamageEnemy(marshal, marshal->health);
    UpdateEnemies(&enemies, &field, field.spawn, 1.5f, true, &fieldHealth, 100);
    assert(marshal->state == ENEMY_FALLEN);
    assert(OpenEncounterSeal(&templars, &enemies, marshal->position));
    assert(AnswerEncounterSeal(&templars, &enemies, &fieldHealth, templars.answer));
    UpdateEncounter(&templars, &enemies, &field, field.spawn, &fieldHealth);
    assert(templars.phase == ENCOUNTER_CLEAR && EnemiesRemaining(&enemies) == 0);
    assert(LoadClearingCheckpoint(&fieldProgress) && fieldProgress.stage == CLEARING_COMPLETE_STAGE);
    field.exitLocked = false;
    assert(MapHasClearPath(&field, field.spawn, stair));
    DevTools fieldTools = {.map = TEMPLAR_FIELD_MAP, .wave = 2, .weapon = 2};
    Vector2 fieldPlayer;
    assert(StartDevEncounter(&fieldTools, DEV_STAGE, &templars, &enemies, &field, &fieldPlayer, &fieldHealth));
    assert(templars.practice && EnemiesRemaining(&enemies) == 6);
    assert(templars.presentation.areaEntry && templars.presentation.wave == 2);
    assert(templars.presentation.waveTotal == 3);
    assert(StartDevEncounter(&fieldTools, DEV_SEAL, &templars, &enemies, &field, &fieldPlayer, &fieldHealth));
    assert(templars.practice && templars.phase == ENCOUNTER_SEAL && enemies.list[0].state == ENEMY_FALLEN);
    assert(LoadClearingCheckpoint(&fieldProgress) && fieldProgress.stage == CLEARING_COMPLETE_STAGE);
    UnloadClearing(&field);

    Clearing palace = {0};
    assert(InitCampaignMap(&palace, NECROMANCER_MAP));
    assert(enemies.necromancerPortrait.id);
    assert(MapHasClearPath(&palace, palace.spawn, palace.pedestal));
    assert(!IsMapActorBlocked(&palace, (Vector2){836,260}));
    assert(IsMapActorBlocked(&palace, (Vector2){836,180}));
    assert(IsMapActorBlocked(&palace, (Vector2){20,470}));
    Encounter final;
    RunProgress finalProgress=NewRunProgress(2);
    finalProgress.map=NECROMANCER_MAP;
    finalProgress.stage=CLEARING_BOSS_STAGE;
    finalProgress.angelSceneCompleted=true;
    finalProgress.angelBlessingUses=1;
    int finalHealth=100;
    BeginEncounter(&final,&enemies,&palace,finalProgress,palace.spawn);
    Enemy *necromancer=&enemies.list[0];
    assert(necromancer->necromancerForm==1 && !EnemyCanBeHit(necromancer));
    assert(final.presentation.areaEntry && final.presentation.wave==0);
    final.presentation=(StagePresentation){0};
    int seenSkills=0;
    for (int form=1;form<=3;form++) {
        assert(necromancer->necromancerForm==form);
        for (int tick=0;tick<530;tick++) {
            UpdateEnemies(&enemies,&palace,palace.pedestal,.035f,true,&finalHealth,100);
            if (necromancer->state==ENEMY_ATTACK) seenSkills|=1<<necromancer->bossAttack;
        }
        assert(finalHealth==100);
        DamageEnemy(necromancer,necromancer->maxHealth*2);
        if (form<3) {
            assert(necromancer->health==necromancer->maxHealth*(form==1?70:35)/100);
            assert(!EnemyCanBeHit(necromancer));
            enemies.necromancer.shots[0].life=1;
        }
        UpdateEnemies(&enemies,&palace,palace.pedestal,1.3f,true,&finalHealth,100);
        assert(enemies.necromancer.shots[0].life==0);
    }
    assert(seenSkills==(1<<10)-1 && necromancer->state==ENEMY_FALLEN);
    for (int miss=0;miss<3;miss++) {
        float power=final.progress.bossPower;
        necromancer->state=ENEMY_FALLEN;
        necromancer->position=palace.pedestal;
        assert(OpenEncounterSeal(&final,&enemies,palace.pedestal));
        assert(AnswerEncounterSeal(&final,&enemies,&finalHealth,(final.answer+1)%3));
        RunProgress savedFinal;
        assert(LoadClearingCheckpoint(&savedFinal) && savedFinal.map==NECROMANCER_MAP);
        if(miss<2) {
            assert(final.phase==ENCOUNTER_FIGHT && final.progress.bossPower>power);
            assert(savedFinal.finalSealFailures==miss+1 && necromancer->necromancerForm==1);
        } else {
            assert(final.phase==ENCOUNTER_CLEAR && savedFinal.stage==CLEARING_COMPLETE_STAGE);
            assert(savedFinal.angelBlessingUses==0 && finalHealth==100);
        }
    }
    final.practice=true;
    final.phase=ENCOUNTER_FIGHT;
    final.presentation=(StagePresentation){0};
    final.progress.finalSealFailures=2;
    necromancer->state=ENEMY_FALLEN;
    necromancer->position=palace.pedestal;
    assert(OpenEncounterSeal(&final,&enemies,palace.pedestal));
    assert(AnswerEncounterSeal(&final,&enemies,&finalHealth,(final.answer+1)%3));
    assert(finalHealth==0 && final.phase==ENCOUNTER_DEFEAT);
    UnloadClearing(&palace);


    /* Audit only the test executable's checkpoint, never the player's save. */
    const char *saveFile=TextFormat("%s/partida_pif.sav",GetApplicationDirectory());
    const char *invalidSaves[]={
        "", "PIF-RUN-5 0 1", "PIF-RUN-5 9 1 100 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 99 100 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 100 100 nan 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 100 100 1 inf 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 101 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-5 0 1 100 100 1 240 1 0 0 0 -1 6",
        "PIF-RUN-5 0 1 100 100 1 240 1 6 0 0 -1 0",
        "PIF-RUN-6 0 5 100 100 1 240 1 0 0 0 -1 0 2",
        "PIF-RUN-6 0 5 100 100 1 240 1 0 0 0 -1 0",
        "PIF-RUN-7 0 6 100 100 1 240 1 0 0 0 -1 0 1",
        "PIF-RUN-7 0 6 100 100 1 240 1 0 0 0 -1 0 1 9",
        "PIF-RUN-7 0 6 100 100 1 240 1 0 0 0 -1 1 1 1",
        "PIF-RUN-7 0 5 100 100 1 240 1 0 0 0 -1 0 1 2",
        "PIF-RUN-8 0 1 100 100 1 240 1 0 0 0 -1 0 0 0",
        "PIF-RUN-8 0 1 100 100 1 240 1 0 0 0 -1 0 0 0 -1",
        "PIF-RUN-8 0 1 100 100 1 240 1 6 0 0 -1 0 0 0 0"
    };
    for (unsigned i=0;i<sizeof(invalidSaves)/sizeof(invalidSaves[0]);i++) {
        assert(SaveFileText(saveFile,(char *)invalidSaves[i]));
        RunProgress rejected=NewRunProgress(2);
        assert(!LoadClearingCheckpoint(&rejected));
        assert(rejected.weapon==2); /* Failure must not partly overwrite the caller. */
    }
    assert(SaveFileText(saveFile,"PIF-CLEARING-3 1 7 80 100 1 240 1 0 0 0 -1"));
    RunProgress migrated;
    assert(LoadClearingCheckpoint(&migrated));
    assert(migrated.map==0 && migrated.stage==CLEARING_COMPLETE_STAGE);
    assert(SaveFileText(saveFile,"PIF-CLEARING-3 1 6 80 100 1 240 1 0 0 0 -1"));
    assert(LoadClearingCheckpoint(&migrated) && migrated.stage==CLEARING_BOSS_STAGE);
    assert(SaveFileText(saveFile,"PIF-CLEARING-4 1 4 80 100 1 240 1 0 0 0 -1"));
    assert(LoadClearingCheckpoint(&migrated) && migrated.stage==CLEARING_BOSS_STAGE && migrated.map==0); /* Onda 4 antiga da Clareira -> Guardião */
    assert(SaveFileText(saveFile,"PIF-CLEARING-2 2"));
    assert(LoadClearingCheckpoint(&migrated) && migrated.weapon==2 && migrated.stage==1);
    assert(SaveClearingCheckpoint(&encounter.progress));
    CheckDevTools(&encounter,&enemies,&clearing);
    UnloadClearing(&clearing);
    assert(InitVillage(&clearing));
    health=100;
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(2),clearing.spawn);
    encounter.presentation=(StagePresentation){0};
    enemies.list[0].health=0;
    enemies.list[0].state=ENEMY_DIE;
    UpdateEncounter(&encounter,&enemies,&clearing,clearing.spawn,&health);
    assert(encounter.phase==ENCOUNTER_VILLAGE_CINEMATIC && EncounterPaused(&encounter));
    RunProgress villageSaved;
    assert(LoadClearingCheckpoint(&villageSaved) && villageSaved.villageAftermath==VILLAGE_SCENE_PENDING);
    BeginEncounter(&encounter,&enemies,&clearing,villageSaved,clearing.spawn);
    assert(encounter.phase==ENCOUNTER_VILLAGE_CINEMATIC);
    CompleteVillageCinematic(&encounter);
    assert(LoadClearingCheckpoint(&villageSaved) && villageSaved.villageAftermath==VILLAGE_SCENE_SEEN);
    BeginEncounter(&encounter,&enemies,&clearing,villageSaved,clearing.spawn);
    assert(encounter.phase==ENCOUNTER_LETTER && enemies.list[0].health==0);
    CheckVillageZombies(&encounter,&enemies,&clearing);
    UnloadClearing(&clearing);
    assert(InitChapel(&clearing));
    BeginEncounter(&encounter,&enemies,&clearing,NewRunProgress(2),clearing.spawn);
    assert(!encounter.chapelAltarDone);
    encounter.phase=ENCOUNTER_ANGEL_CINEMATIC;
    assert(EncounterPaused(&encounter));
    CompleteAngelCinematic(&encounter,100);
    assert(encounter.chapelAltarDone && !encounter.basementComplete);
    RunProgress blessed;
    assert(LoadClearingCheckpoint(&blessed) && blessed.angelSceneCompleted && blessed.angelBlessingUses==3);
    RunProgress boneBlessed=blessed;
    boneBlessed.map=BONELAND_MAP;
    boneBlessed.stage=BONELAND_WAVES;
    boneBlessed.angelBlessingUses=1;
    assert(SaveClearingCheckpoint(&boneBlessed));
    RunProgress restoredBoneBlessing;
    assert(LoadClearingCheckpoint(&restoredBoneBlessing));
    assert(restoredBoneBlessing.map==BONELAND_MAP && restoredBoneBlessing.stage==BONELAND_WAVES);
    assert(restoredBoneBlessing.angelSceneCompleted && restoredBoneBlessing.angelBlessingUses==1);
    blessed.angelBlessingUses=0;
    BeginEncounter(&encounter,&enemies,&clearing,blessed,clearing.spawn);
    CompleteAngelCinematic(&encounter,100);
    assert(encounter.chapelAltarDone && encounter.progress.angelBlessingUses==0);
    UnloadEnemies(&enemies);
    UnloadClearing(&clearing);
    UnloadGameFonts();
    CloseWindow();
    puts("Four waves, boss fifth wave, rewards, boss roots, seals and checkpoints passed");
    return 0;
}
