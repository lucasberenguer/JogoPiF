#include "game_text.h"
#include "enemy.h"
#include "run_progress.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define ENEMY_SPEED 98.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12
/* Balanceamento das ondas da Clareira do Selo (não afeta outros mapas). */
#define CLEARING_FIRST_WAVE_ENEMIES 14
#define CLEARING_WAVE_ENEMY_STEP 4
#define CLEARING_ENEMY_HEALTH_SCALE 1.10f
#define CLEARING_ENEMY_DAMAGE_SCALE 1.15f
#define ENEMY_HURT_RADIUS 20.0f
#define CLEARING_MELEE_ATTACK_DURATION .62f
#define CLEARING_MELEE_ATTACK_FRAMES 8
#define CLEARING_MELEE_IMPACT_FRAME 4
#define CLEARING_ARCHER_RELEASE_TIME .32f
#define CULTIST_RELEASE_TIME .58f
#define CULTIST_CAST_DURATION .95f
#define CULTIST_SHOT_SPEED 275.0f
#define SPECIAL_CHARGE_START .5f
#define SPECIAL_CHARGE_END 1.0f
#define SPECIAL_ATTACK_END 1.1f
#define RANGED_MIN_DISTANCE 170.0f
#define RANGED_MAX_DISTANCE 300.0f
#define RANGED_SHOT_SPEED 365.0f
#define ARENA_BOSS_BASE_HEALTH 2200
#define CEMETERY_BOSS_BASE_HEALTH 2420
#define ENEMY_DEFEAT_DURATION .85f
#define GUARDIAN_DEFEAT_FRAMES 8
#define GUARDIAN_SPAWN_DURATION 3.0f
#define GUARDIAN_SPAWN_COLUMNS 6
#define GUARDIAN_SPAWN_RECOIL_FRAME 13 /* Índice na folha, começando em zero. */
#define GUARDIAN_SPAWN_SCALE .72f
#define GUARDIAN_SPRITE_SCALE .65f
#define GUARDIAN_PHASE_TRANSITION_TIME 1.0f
#define GUARDIAN_ROOT_ECHO_TIME .95f
#define GUARDIAN_CURSE_ECHO_TIME .62f
#define GUARDIAN_ROOT_WARNING 0.8f
#define GUARDIAN_ROOT_RADIUS 74.0f
#define GUARDIAN_ROOT_DAMAGE 30
#define GUARDIAN_WAVE_WARNING .65f
#define GUARDIAN_WAVE_RADIUS 60.0f
#define GUARDIAN_WAVE_DAMAGE 36
#define GUARDIAN_WAVE_FPS 12.0f
#define GUARDIAN_WAVE_FRAMES 8
#define GUARDIAN_WAVE_TILE_WIDTH 352
#define GUARDIAN_WAVE_TILE_HEIGHT 288
#define GUARDIAN_CURSE_WARNING 1.0f
#define GUARDIAN_CURSE_FPS 12.0f
#define GUARDIAN_CURSE_CHARGE_FRAMES 8
#define GUARDIAN_CURSE_IMPACT_FRAMES 7
#define GUARDIAN_CURSE_TILE_WIDTH 384
#define GUARDIAN_CURSE_TILE_HEIGHT 320
#define GUARDIAN_MELEE_RANGE 118.0f
#define GUARDIAN_MELEE_DAMAGE 26
#define GUARDIAN_MELEE_COOLDOWN 1.65f
#define GUARDIAN_MELEE_FPS 10.0f
#define GUARDIAN_MELEE_FRAMES 8
#define GUARDIAN_MELEE_HIT_FRAME 4

/* As poses da folha têm larguras diferentes. O pivô acompanha o corpo e a
   linha do chão de cada fileira, independente de folhas e raízes soltas. */
static const struct {
    int top, bottom, ground;
    int edges[GUARDIAN_SPAWN_COLUMNS + 1];
    int pivots[GUARDIAN_SPAWN_COLUMNS];
} guardianSpawnRows[] = {
    {0,   234, 221, {0, 224, 452, 674, 937, 1201, 1536}, {116, 343, 571, 819, 1078, 1348}},
    {234, 474, 462, {0, 242, 481, 737, 984, 1236, 1536}, {118, 362, 619, 868, 1122, 1374}},
    {474, 702, 692, {0, 254, 523, 788, 1055, 1288, 1536}, {140, 397, 674, 938, 1188, 1433}},
    {702, 940, 924, {0, 273, 529, 784, 1049, 1278, 1536}, {155, 407, 680, 930, 1181, 1408}},
};

#define GUARDIAN_SPAWN_FRAMES ((int)(sizeof(guardianSpawnRows) / sizeof(guardianSpawnRows[0])) * GUARDIAN_SPAWN_COLUMNS - 1)

/* Ordem das direções do inimigo: baixo, cima, esquerda e direita.
   O primeiro recorte de cada linha começa depois da legenda da folha. */
static const struct {
    int top, bottom, ground;
    int edges[GUARDIAN_DEFEAT_FRAMES + 1];
    float scale;
} guardianDefeatRows[] = {
    {0,   267, 251, {74, 248, 420, 609, 795, 988, 1176, 1364, 1536}, .66f},
    {267, 523, 508, {73, 248, 420, 608, 795, 988, 1178, 1364, 1536}, .72f},
    {523, 779, 763, {94, 251, 420, 607, 793, 987, 1179, 1366, 1536}, .70f},
    {779, 1024, 1005, {78, 250, 420, 607, 797, 990, 1181, 1363, 1536}, .71f},
};

static const int guardianDefeatPivots[GUARDIAN_DEFEAT_FRAMES] = {
    158, 334, 510, 699, 891, 1084, 1271, 1454
};

static int GuardianDefeatFrame(const Enemy *enemy) {
    if (enemy->state == ENEMY_FALLEN) return GUARDIAN_DEFEAT_FRAMES - 1;
    float progress = enemy->stateTime / ENEMY_DEFEAT_DURATION;
    return (int)fminf(progress * GUARDIAN_DEFEAT_FRAMES, GUARDIAN_DEFEAT_FRAMES - 1);
}

static bool IsGuardianSpawn(const EnemyGroup *group, const Enemy *enemy) {
    return !group->boneland && !group->cemetery && !group->village && enemy->templarRole != TEMPLAR_MARSHAL &&
           enemy->type == ENEMY_BOSS && enemy->state == ENEMY_SPAWNING;
}

static int GuardianSpawnFrame(const Enemy *enemy) {
    float progress = enemy->stateTime / GUARDIAN_SPAWN_DURATION;
    return (int)Clamp(progress * GUARDIAN_SPAWN_FRAMES, 0, GUARDIAN_SPAWN_FRAMES - 1);
}

typedef struct GuardianHazard {
    float warning, hitStart, hitEnd, duration, radius;
    int damage;
    Color warningColor;
} GuardianHazard;

static const GuardianHazard guardianHazards[GUARDIAN_ATTACK_COUNT] = {
    [GUARDIAN_ROOTS] = {GUARDIAN_ROOT_WARNING, GUARDIAN_ROOT_WARNING, 1.3f, 1.6f,
                        GUARDIAN_ROOT_RADIUS, GUARDIAN_ROOT_DAMAGE, ORANGE},
    [GUARDIAN_ROOT_WAVE] = {GUARDIAN_WAVE_WARNING,
        GUARDIAN_WAVE_WARNING + 2.0f / GUARDIAN_WAVE_FPS,
        GUARDIAN_WAVE_WARNING + 5.0f / GUARDIAN_WAVE_FPS,
        GUARDIAN_WAVE_WARNING + GUARDIAN_WAVE_FRAMES / GUARDIAN_WAVE_FPS,
        GUARDIAN_WAVE_RADIUS, GUARDIAN_WAVE_DAMAGE, GOLD},
    [GUARDIAN_CURSE] = {GUARDIAN_CURSE_WARNING,
        GUARDIAN_CURSE_WARNING + 1.0f / GUARDIAN_CURSE_FPS,
        GUARDIAN_CURSE_WARNING + 6.0f / GUARDIAN_CURSE_FPS,
        GUARDIAN_CURSE_WARNING + GUARDIAN_CURSE_IMPACT_FRAMES / GUARDIAN_CURSE_FPS,
        110.0f, 42, VIOLET},
};

int EnemyOptionalDropChance(EnemyType type) {
    switch (type) {
        case ENEMY_COMMON_MELEE: return 20;
        case ENEMY_COMMON_RANGED: return 25;
        case ENEMY_SPECIAL: return 45;
        case ENEMY_ELITE: return 65;
        case ENEMY_BOSS: return 100;
        default: return 0;
    }
}

bool EnemyShouldDrop(EnemyType type, int roll) {
    return roll >= 0 && roll < 10000 && roll < EnemyOptionalDropChance(type)*100;
}

static bool GuardianEnraged(const Enemy *enemy);

static int FacingToward(Vector2 direction) {
    return fabsf(direction.x) > fabsf(direction.y)*0.65f
        ? (direction.x < 0 ? 2 : 3) : (direction.y < 0 ? 1 : 0);
}

static int ClearingEnemyFrame(const Enemy *enemy, bool boneland) {
    bool archer = enemy->type == ENEMY_COMMON_RANGED;
    if (enemy->type == ENEMY_SPECIAL) {
        if (enemy->state == ENEMY_WALK) return (int)(enemy->walkTime * 8.0f) % 4;
        if (enemy->state != ENEMY_ATTACK) return 0;
        int impact = enemy->facing == 2 ? 2 : 3;
        int count = enemy->facing == 1 ? 7 : 6;
        if (enemy->stateTime < SPECIAL_CHARGE_START)
            return (int)(enemy->stateTime / SPECIAL_CHARGE_START * impact);
        float recovery = (enemy->stateTime - SPECIAL_CHARGE_START) / (SPECIAL_ATTACK_END - SPECIAL_CHARGE_START);
        int frame = (int)fminf(impact + recovery * (count - impact), count - 1);
        // Pula o quadro em que o perfil esquerdo vira para o outro lado.
        return enemy->facing == 2 && frame == 5 ? 6 : frame;
    }
    switch (enemy->state) {
        case ENEMY_IDLE: return (int)(enemy->stateTime * 7.5f) % (archer ? 4 : 5);
        case ENEMY_WALK: return (int)(enemy->walkTime * 11.0f) % 8;
        case ENEMY_ATTACK: {
            if (archer) {
                // A flecha para a esquerda já aparece no quarto quadro da folha.
                int releaseFrame = enemy->facing == 2 ? 3 : 4;
                float release = boneland ? CULTIST_RELEASE_TIME : CLEARING_ARCHER_RELEASE_TIME;
                float duration = boneland ? CULTIST_CAST_DURATION : CLEARING_MELEE_ATTACK_DURATION;
                if (enemy->stateTime < release)
                    return (int)(enemy->stateTime / release * releaseFrame);
                float recovery = (enemy->stateTime - release) / (duration - release);
                return (int)fminf(releaseFrame + recovery * (8 - releaseFrame), 7);
            }
            return (int)fminf(enemy->stateTime / CLEARING_MELEE_ATTACK_DURATION *
                             CLEARING_MELEE_ATTACK_FRAMES, CLEARING_MELEE_ATTACK_FRAMES - 1);
        }
        default: return 0;
    }
}

int EnemyAnimationFrame(const Enemy *enemy) {
    if (enemy->type == ENEMY_BOSS) {
        if (enemy->state == ENEMY_ATTACK && enemy->guardianMelee)
            return (int)fminf(enemy->stateTime * GUARDIAN_MELEE_FPS, GUARDIAN_MELEE_FRAMES - 1);
        if (enemy->state == ENEMY_ATTACK && enemy->bossAttack != GUARDIAN_ROOTS)
            return 0;
       if (enemy->state == ENEMY_WALK) {
            float v = enemy->speed * (GuardianEnraged(enemy) ? 1.5f : 1.0f);
            float fps = 4.0f * v / 30.0f;
            return (int)(enemy->walkTime * fps) % 8;
        }
        int count = enemy->state == ENEMY_ATTACK ? 9 : 6;
        int frame = (int)(enemy->stateTime * (enemy->state == ENEMY_ATTACK ? 6.0f : 7.0f));
        return enemy->state == ENEMY_ATTACK ? (frame < count ? frame : count-1) : frame % count;
    }
    int count = enemy->type == ENEMY_COMMON_RANGED ? 4 :
                enemy->state == ENEMY_ATTACK || enemy->state == ENEMY_DIE ? 5 : 4;
    float speed = enemy->state == ENEMY_DIE ? 5.9f :
                  enemy->state == ENEMY_ATTACK ? 8.0f :
                  enemy->state == ENEMY_HURT ? 12.0f : 7.5f;
    float elapsed = enemy->state == ENEMY_WALK ? enemy->walkTime : enemy->stateTime;
    int frame = (int)(elapsed*speed);
    return enemy->state == ENEMY_IDLE || enemy->state == ENEMY_WALK
        ? frame % count : (frame < count ? frame : count-1);
}

static Texture2D LoadClearingSpriteWithMatte(const char *path, Shader shader) {
    Texture2D source = LoadTexture(path);
    if (!source.id) return source;
    RenderTexture2D target = LoadRenderTexture(source.width, source.height);
    Texture2D result = {0};
    if (shader.id && target.id) {
        SetTextureFilter(source, TEXTURE_FILTER_POINT);
        BeginTextureMode(target);
        ClearBackground(BLANK);
        // GL_ONE sobre o alvo vazio preserva RGB e alfa produzidos pelo shader.
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        BeginShaderMode(shader);
        DrawTexture(source, 0, 0, WHITE);
        EndShaderMode();
        EndBlendMode();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        if (image.data) {
            ImageFlipVertical(&image);
            result = LoadTextureFromImage(image);
            UnloadImage(image);
        }
    }
    if (target.id) UnloadRenderTexture(target);
    UnloadTexture(source);
    return result;
}

static bool IsBonelandMatte(Color color) {
    return color.r >= 175 && color.b >= 140 && color.g <= 100 &&
           color.b > color.g + 85 && color.b + 70 >= color.r;
}

static Texture2D LoadBonelandSprite(const char *path) {
    Image image = LoadImage(path);
    if (!image.data) return (Texture2D){0};
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *original = LoadImageColors(image);
    if (!original) {
        UnloadImage(image);
        return (Texture2D){0};
    }
    Color *pixels = image.data;
    int width = image.width, height = image.height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int i = y * width + x;
            Color color = original[i];
            // Limpa o contorno misturado com o fundo sem remover os tons roxos internos.
            bool fringe = color.b + 8 >= color.r && color.b > color.g + 28 &&
                ((x > 0 && IsBonelandMatte(original[i - 1])) ||
                 (x + 1 < width && IsBonelandMatte(original[i + 1])) ||
                 (y > 0 && IsBonelandMatte(original[i - width])) ||
                 (y + 1 < height && IsBonelandMatte(original[i + width])));
            if (IsBonelandMatte(color) || fringe) pixels[i] = BLANK;
        }
    }
    UnloadImageColors(original);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static Texture2D LoadBoneLordSprite(const char *path) {
    Image image = LoadImage(path);
    if (!image.data) return (Texture2D){0};
    ImageAlphaClear(&image, BLANK, .24f);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

static const float templarArrivalTime = 2.05f;
static const float templarPlantTime = .48f;
static const float shieldImpact = .42f;
static const float executionerImpact = .76f;
static const float crossbowRelease = .52f;
static const struct {
    float impact, activeEnd, duration, reach, arcDot;
    int damage;
} marshalSkills[] = {
    [MARSHAL_SLASH] = {.48f, .64f, .94f, 155, .34f, 20},
    [MARSHAL_CHARGE] = {.55f, 1.12f, 1.38f, 63, 0, 22},
    [MARSHAL_RETURN] = {.50f, .68f, 1.07f, 168, -.14f, 22},
    [MARSHAL_RUPTURE] = {.71f, .93f, 1.39f, 209, .50f, 33},
    [MARSHAL_SPIN] = {.56f, .79f, 1.13f, 165, -1, 28},
    [MARSHAL_SHOCKWAVE] = {.84f, 1.46f, 1.79f, 295, -1, 33},
};

bool InitEnemies(EnemyGroup *group) {
    *group = (EnemyGroup){0};
    if (!LoadNecromancerSprites(group)) { UnloadNecromancerSprites(group); return false; }
    group->sprites = LoadTexture("assets/inimigo_encapuzado.png");
    group->backSprites = LoadTexture("assets/inimigo_encapuzado_costas.png");
    group->frontSprites = LoadTexture("assets/inimigo_encapuzado_frente.png");
    const char *zombieSheets[]={
        "assets/zumbi_cemiterio_idle.png", "assets/zumbi_cemiterio_walk.png",
        "assets/zumbi_cemiterio_attack.png"
    };
    const char *lanternSheets[]={
        "assets/zumbi_lanterna_idle.png", "assets/zumbi_lanterna_walk.png",
        "assets/zumbi_lanterna_attack.png"
    };
    const char *specialSheets[]={
        "assets/cemiterio_especial_idle.png", "assets/cemiterio_especial_walk.png",
        "assets/cemiterio_especial_attack.png"
    };
    for (int i=0;i<3;i++) {
        group->cemeteryZombie[i]=LoadTexture(zombieSheets[i]);
        if (group->cemeteryZombie[i].id) SetTextureFilter(group->cemeteryZombie[i],TEXTURE_FILTER_POINT);
        group->cemeteryLantern[i]=LoadTexture(lanternSheets[i]);
        if (group->cemeteryLantern[i].id) SetTextureFilter(group->cemeteryLantern[i],TEXTURE_FILTER_POINT);
        group->cemeterySpecial[i]=LoadTexture(specialSheets[i]);
        if (group->cemeterySpecial[i].id) SetTextureFilter(group->cemeterySpecial[i],TEXTURE_FILTER_POINT);
    }
    group->coveiroIdle=LoadTexture("assets/coveiro_novo_idle.png");
    group->coveiroWalk=LoadTexture("assets/coveiro_novo_run.png");
    group->coveiroEmerge=LoadTexture("assets/coveiro_novo_emerge.png");
    group->coveiroFall=LoadTexture("assets/coveiro_novo_fall.png");
    group->coveiroMelee=LoadTexture("assets/coveiro_novo_melee.png");
    group->cemeterySpell=LoadTexture("assets/zumbi_lanterna_projetil.png");
    group->boss[0] = LoadTexture("assets/coveiro_novo_pull.png");
    group->boss[1] = LoadTexture("assets/coveiro_novo_spin.png");
    group->boss[2] = LoadTexture("assets/coveiro_novo_charge.png");
    group->effects[0]=LoadTexture("assets/coveiro_novo_pull_effect.png");
    group->effects[1]=LoadTexture("assets/coveiro_pa_vento_v4.png");
    group->effects[2]=LoadTexture("assets/coveiro_efeito_investida.png");
    if (!group->cemeteryZombie[0].id || !group->cemeteryZombie[1].id || !group->cemeteryZombie[2].id ||
        !group->cemeteryLantern[0].id || !group->cemeteryLantern[1].id || !group->cemeteryLantern[2].id ||
        !group->cemeterySpecial[0].id || !group->cemeterySpecial[1].id || !group->cemeterySpecial[2].id ||
        !group->coveiroIdle.id || !group->coveiroEmerge.id || !group->coveiroFall.id ||
        !group->coveiroWalk.id || !group->coveiroMelee.id || !group->cemeterySpell.id || !group->boss[0].id ||
        !group->boss[1].id || !group->boss[2].id || !group->effects[0].id ||
        !group->effects[1].id || !group->effects[2].id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->coveiroIdle,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroEmerge,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroFall,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroWalk,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroMelee,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->cemeterySpell,TEXTURE_FILTER_POINT);
    for (int i=0;i<3;i++) SetTextureFilter(group->boss[i], TEXTURE_FILTER_POINT);
    for (int i=0;i<3;i++) SetTextureFilter(group->effects[i], TEXTURE_FILTER_POINT);
    group->clearingMelee[0] = LoadTexture("assets/clareira_melee_idle.png");
    group->clearingMelee[1] = LoadTexture("assets/clareira_melee_andar.png");
    Shader matte = LoadShader(NULL, "assets/shaders/clareira_melee.fs");
    group->clearingMelee[2] = LoadClearingSpriteWithMatte("assets/clareira_melee_ataque.png", matte);
    group->clearingArcher[0] = LoadClearingSpriteWithMatte("assets/clareira_arqueiro_idle.png", matte);
    group->clearingArcher[1] = LoadClearingSpriteWithMatte("assets/clareira_arqueiro_andar.png", matte);
    group->clearingArcher[2] = LoadClearingSpriteWithMatte("assets/clareira_arqueiro_ataque.png", matte);
    int darkRedMatte = 1;
    if (matte.id) SetShaderValue(matte, GetShaderLocation(matte, "darkRedMatte"),
                                &darkRedMatte, SHADER_UNIFORM_INT);
    group->clearingSpecial[0] = LoadClearingSpriteWithMatte("assets/clareira_especial_idle.png", matte);
    group->clearingSpecial[1] = LoadClearingSpriteWithMatte("assets/clareira_especial_andar.png", matte);
    group->clearingSpecial[2] = LoadTexture("assets/clareira_especial_ataque.png");
    if (matte.id) UnloadShader(matte);
    group->bonelandGhoul = LoadBonelandSprite("assets/terra_dos_ossos_carnical.png");
    group->bonelandCultist = LoadBonelandSprite("assets/terra_dos_ossos_cultista.png");
    group->boneLordWalk = LoadBoneLordSprite("assets/terra_dos_ossos_chefe_marcha.png");
    group->boneLordPounce = LoadBoneLordSprite("assets/terra_dos_ossos_chefe_bote.png");
    group->boneLordClaw = LoadBoneLordSprite("assets/terra_dos_ossos_chefe_garra.png");
    group->boneLordDash = LoadBoneLordSprite("assets/terra_dos_ossos_chefe_dash.png");
    group->boneLordBite = LoadBoneLordSprite("assets/terra_dos_ossos_chefe_mordida.png");
    static const char *const templarFiles[] = {
        [TEMPLAR_SHIELD] = "assets/templario_escudo.png",
        [TEMPLAR_EXECUTIONER] = "assets/templario_carrasco.png",
        [TEMPLAR_CROSSBOW] = "assets/templario_besteiro.png",
        [TEMPLAR_STANDARD] = "assets/templario_estandarte.png",
        [TEMPLAR_MARSHAL] = "assets/templario_marechal.png",
    };
    static const char *const attackFiles[] = {
        [TEMPLAR_SHIELD] = "assets/templario_escudo_ataque.png",
        [TEMPLAR_EXECUTIONER] = "assets/templario_carrasco_ataque.png",
        [TEMPLAR_CROSSBOW] = "assets/templario_besteiro_ataque.png",
        [TEMPLAR_STANDARD] = "assets/templario_estandarte_ataque.png",
        [TEMPLAR_MARSHAL] = "assets/templario_marechal_corte_v2.png",
    };
    for (int role = TEMPLAR_SHIELD; role <= TEMPLAR_MARSHAL; role++) {
        group->templarWalk[role] = LoadTexture(templarFiles[role]);
        group->templarAttack[role] = LoadTexture(attackFiles[role]);
        if (!group->templarWalk[role].id || !group->templarAttack[role].id) {
            UnloadEnemies(group);
            return false;
        }
        SetTextureFilter(group->templarWalk[role], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->templarAttack[role], TEXTURE_FILTER_POINT);
    }
    group->sealMarker = LoadTexture("assets/inimigo_selo.png");
    group->bossSprites = LoadTexture("assets/boss_clareira.png");
    group->guardianSpawn = LoadTexture("assets/boss_clareira_spawn.png");
    group->guardianDefeat = LoadTexture("assets/boss_clareira_derrota.png");
    group->bossWalk = LoadTexture("assets/boss_clareira_andar.png");
    group->bossPortrait = LoadTexture("assets/guardiao_intro_pixel_v2.png");
    group->cemeteryPortrait = LoadTexture("assets/coveiro_intro_pixel.png");
    group->boneLordPortrait = LoadTexture("assets/terra_dos_ossos_chefe_retrato.png");
    group->marshalSlam = LoadTexture("assets/templario_marechal_ruptura_v2.png");
    group->marshalCharge = LoadTexture("assets/templario_marechal_investida_v2.png");
    group->templarDust = LoadTexture("assets/templario_poeira.png");
    if (!group->marshalSlam.id || !group->marshalCharge.id || !group->templarDust.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->marshalSlam, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->marshalCharge, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->templarDust, TEXTURE_FILTER_POINT);
    if (FileExists("assets/templario_efeitos.png")) {
        group->templarEffects = LoadTexture("assets/templario_efeitos.png");
        if (group->templarEffects.id) SetTextureFilter(group->templarEffects, TEXTURE_FILTER_POINT);
    }
    group->marshalPortrait = LoadTexture("assets/templario_marechal_retrato.png");
    group->bossAttack = LoadTexture("assets/boss_clareira_especial_1.png");
    group->roots = LoadTexture("assets/boss_clareira_raizes_area.png");
    group->rootWave = LoadTexture("assets/boss_clareira_onda.png");
    group->curse = LoadTexture("assets/boss_clareira_maldicao.png");
    group->guardianMelee = LoadTexture("assets/boss_clareira_corpo_a_corpo.png");
    group->villageBossWalk = LoadTexture("assets/lider_vila_walk.png");
    group->villageBossAttack = LoadTexture("assets/lider_vila_ataque.png");
    group->villageBossCharge = LoadTexture("assets/lider_vila_investida.png");
    const char *villageZombieSheets[VILLAGE_ZOMBIE_ATLAS_COUNT] = {
        [VILLAGE_ZOMBIE_IDLE] = "assets/zumbi_vila_idle.png",
        [VILLAGE_ZOMBIE_EMERGE] = "assets/zumbi_vila_emerge.png",
        [VILLAGE_ZOMBIE_WALK] = "assets/zumbi_vila_walk.png",
        [VILLAGE_ZOMBIE_ATTACK] = "assets/zumbi_vila_attack.png"
    };
    for (int i = 0; i < VILLAGE_ZOMBIE_ATLAS_COUNT; i++) {
        group->villageZombie[i] = LoadTexture(villageZombieSheets[i]);
        if (!group->villageZombie[i].id) {
            UnloadEnemies(group);
            return false;
        }
        SetTextureFilter(group->villageZombie[i], TEXTURE_FILTER_POINT);
    }
    if (!group->sprites.id || !group->backSprites.id || !group->frontSprites.id ||
        !group->bossSprites.id || !group->bossWalk.id || !group->bossAttack.id ||
        !group->guardianSpawn.id || !group->guardianDefeat.id ||
        !group->clearingMelee[0].id || !group->clearingMelee[1].id || !group->clearingMelee[2].id ||
        !group->clearingArcher[0].id || !group->clearingArcher[1].id || !group->clearingArcher[2].id ||
        !group->clearingSpecial[0].id || !group->clearingSpecial[1].id || !group->clearingSpecial[2].id ||
        !group->bonelandGhoul.id || !group->bonelandCultist.id ||
        !group->boneLordWalk.id || !group->boneLordPounce.id || !group->boneLordClaw.id || !group->boneLordDash.id || !group->boneLordBite.id ||
        !group->sealMarker.id ||
        !group->roots.id || !group->rootWave.id || !group->curse.id || !group->guardianMelee.id ||
        !group->bossPortrait.id || !group->cemeteryPortrait.id || !group->boneLordPortrait.id || !group->marshalPortrait.id ||
        !group->villageBossWalk.id || !group->villageBossAttack.id ||
        !group->villageBossCharge.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->backSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->frontSprites, TEXTURE_FILTER_POINT);
    for (int i=0; i<3; i++) {
        SetTextureFilter(group->clearingMelee[i], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->clearingArcher[i], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->clearingSpecial[i], TEXTURE_FILTER_POINT);
    }
    SetTextureFilter(group->sealMarker, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bonelandGhoul, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bonelandCultist, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianSpawn, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianDefeat, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->cemeteryPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->boneLordPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->marshalPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->roots, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rootWave, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->curse, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianMelee, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->villageBossWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->villageBossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->villageBossCharge, TEXTURE_FILTER_POINT);
    group->villageLetter=LoadTexture("assets/cutscenes/village_aftermath/images/carta.png");
    if (group->villageLetter.id) SetTextureFilter(group->villageLetter,TEXTURE_FILTER_BILINEAR);
    return true;
}

void UnloadEnemies(EnemyGroup *group) {
    UnloadNecromancerSprites(group);
    for (int i=0;i<3;i++) if (group->cemeteryZombie[i].id) UnloadTexture(group->cemeteryZombie[i]);
    for (int i=0;i<3;i++) if (group->cemeteryLantern[i].id) UnloadTexture(group->cemeteryLantern[i]);
    for (int i=0;i<3;i++) if (group->cemeterySpecial[i].id) UnloadTexture(group->cemeterySpecial[i]);
    if (group->coveiroIdle.id) UnloadTexture(group->coveiroIdle);
    if (group->coveiroEmerge.id) UnloadTexture(group->coveiroEmerge);
    if (group->coveiroFall.id) UnloadTexture(group->coveiroFall);
    if (group->coveiroWalk.id) UnloadTexture(group->coveiroWalk);
    if (group->coveiroMelee.id) UnloadTexture(group->coveiroMelee);
    if (group->cemeterySpell.id) UnloadTexture(group->cemeterySpell);
    for (int i=0;i<3;i++) if (group->boss[i].id) UnloadTexture(group->boss[i]);
    for (int i=0;i<3;i++) if (group->effects[i].id) UnloadTexture(group->effects[i]);

    if (group->sprites.id) UnloadTexture(group->sprites);
    if (group->backSprites.id) UnloadTexture(group->backSprites);
    if (group->frontSprites.id) UnloadTexture(group->frontSprites);
    for (int i=0; i<3; i++) {
        if (group->clearingMelee[i].id) UnloadTexture(group->clearingMelee[i]);
        if (group->clearingArcher[i].id) UnloadTexture(group->clearingArcher[i]);
        if (group->clearingSpecial[i].id) UnloadTexture(group->clearingSpecial[i]);
    }
    if (group->sealMarker.id) UnloadTexture(group->sealMarker);
    for (int role = TEMPLAR_SHIELD; role <= TEMPLAR_MARSHAL; role++) {
        if (group->templarWalk[role].id) UnloadTexture(group->templarWalk[role]);
        if (group->templarAttack[role].id) UnloadTexture(group->templarAttack[role]);
    }
    if (group->bonelandGhoul.id) UnloadTexture(group->bonelandGhoul);
    if (group->bonelandCultist.id) UnloadTexture(group->bonelandCultist);
    if (group->boneLordWalk.id) UnloadTexture(group->boneLordWalk);
    if (group->boneLordPounce.id) UnloadTexture(group->boneLordPounce);
    if (group->boneLordClaw.id) UnloadTexture(group->boneLordClaw);
    if (group->boneLordDash.id) UnloadTexture(group->boneLordDash);
    if (group->boneLordBite.id) UnloadTexture(group->boneLordBite);
    if (group->bossSprites.id) UnloadTexture(group->bossSprites);
    if (group->guardianSpawn.id) UnloadTexture(group->guardianSpawn);
    if (group->guardianDefeat.id) UnloadTexture(group->guardianDefeat);
    if (group->bossWalk.id) UnloadTexture(group->bossWalk);
    if (group->bossPortrait.id) UnloadTexture(group->bossPortrait);
    if (group->cemeteryPortrait.id) UnloadTexture(group->cemeteryPortrait);
    if (group->boneLordPortrait.id) UnloadTexture(group->boneLordPortrait);
    if (group->templarEffects.id) UnloadTexture(group->templarEffects);
    if (group->marshalSlam.id) UnloadTexture(group->marshalSlam);
    if (group->marshalCharge.id) UnloadTexture(group->marshalCharge);
    if (group->templarDust.id) UnloadTexture(group->templarDust);
    if (group->marshalPortrait.id) UnloadTexture(group->marshalPortrait);
    if (group->bossAttack.id) UnloadTexture(group->bossAttack);
    if (group->roots.id) UnloadTexture(group->roots);
    if (group->rootWave.id) UnloadTexture(group->rootWave);
    if (group->curse.id) UnloadTexture(group->curse);
    if (group->guardianMelee.id) UnloadTexture(group->guardianMelee);
    if (group->villageBossWalk.id) UnloadTexture(group->villageBossWalk);
    if (group->villageBossAttack.id) UnloadTexture(group->villageBossAttack);
    if (group->villageBossCharge.id) UnloadTexture(group->villageBossCharge);
    for (int i = 0; i < VILLAGE_ZOMBIE_ATLAS_COUNT; i++)
        if (group->villageZombie[i].id) UnloadTexture(group->villageZombie[i]);
    if (group->villageLetter.id) UnloadTexture(group->villageLetter);
    *group = (EnemyGroup){0};
}

void ResetEnemies(EnemyGroup *group, const Clearing *clearing) {
    StartEnemyWave(group, clearing, 1, clearing->spawn, 1.0f);
}

void ClearEnemyAttacks(EnemyGroup *group) {
    group->templarBlood.remaining = 0;
    memset(group->necromancer.shots,0,sizeof(group->necromancer.shots));
    for (int i=1; i<=NECROMANCER_WRAITHS; i++)
        if (group->list[i].necromancerWraith) group->list[i].state=ENEMY_GONE;
    memset(group->projectiles, 0, sizeof(group->projectiles));
    memset(group->cemeteryProjectiles,0,sizeof(group->cemeteryProjectiles));
    memset(group->rootAttacks, 0, sizeof(group->rootAttacks));
    memset(group->impacts,0,sizeof(group->impacts));
}

static Vector2 SpawnPosition(const Clearing *clearing, Vector2 player) {
    Rectangle bounds = clearing->playableBounds;
    Vector2 fallback = clearing->spawn;
    bool hasFallback = false;
    for (int attempt = 0; attempt < 800; attempt++) {
        float x = bounds.x + GetRandomValue(35, (int)bounds.width - 35);
        float y = bounds.y + GetRandomValue(35, (int)bounds.height - 35);
        // With the whole arena visible, entry points stay near its edges.
        if (attempt < 500 && x > bounds.x + bounds.width*.25f &&
            x < bounds.x + bounds.width*.75f && y > bounds.y + bounds.height*.25f &&
            y < bounds.y + bounds.height*.75f) continue;
        Vector2 point = {x, y};
        if (Vector2Distance(point, player) <= 230.0f || IsMapActorBlocked(clearing, point)) continue;
        if (!hasFallback) {
            fallback = point;
            hasFallback = true;
        }

        // Prefer a clear approach so the enemy can start chasing immediately.
        if (MapHasClearPath(clearing, point, player)) return point;
    }
    if (!hasFallback) FindFreeMapPosition(clearing, fallback, &fallback);
    return fallback;
}

static float WaveSpawnDelay(int wave, int index) {
    return index * (.30f - (wave - 1) * .025f);
}

static void StartBonelandWave(EnemyGroup *group, const Clearing *map, int wave,
                              Vector2 player, float playerPower) {
    static const struct {
        int total, cultists, ghoulHealth, cultistHealth;
    } waves[] = {
        {20, 5, 49, 36}, {26, 7, 53, 39}, {30, 9, 56, 41}
    };
    int index = (int)Clamp((float)wave, 1, sizeof(waves) / sizeof(waves[0])) - 1;
    int total = waves[index].total;
    int cultists = waves[index].cultists;
    float adjustment = 1.0f + fminf(.15f, fmaxf(0, playerPower - 1.0f) * .05f);
    for (int i = 0; i < ENEMY_COUNT; i++) {
        group->list[i] = (Enemy){.state = ENEMY_GONE};
        if (i >= total) continue;
        bool cultist = (i + 1) * cultists / total > i * cultists / total;
        int baseHealth = cultist ? waves[index].cultistHealth : waves[index].ghoulHealth;
        int health = (int)ceilf(baseHealth * adjustment);
        group->list[i] = (Enemy){
            .type = cultist ? ENEMY_COMMON_RANGED : ENEMY_COMMON_MELEE,
            .position = SpawnPosition(map, player), .state = ENEMY_SPAWNING,
            .health = health, .maxHealth = health, .stateTime = -WaveSpawnDelay(index + 1, i),
            .speed = cultist ? 80 : index > 0 ? 125 : 118,
            .power = index > 0 ? 1.09f : 1.0f, .attackCooldown = .8f,
            .facing = 0, .lastKatanaSwing = -1
        };
    }
}

static void StartTemplarWave(EnemyGroup *group, int wave) {
    static const struct {
        TemplarRole role;
        Vector2 position;
    } formations[][6] = {
        {{TEMPLAR_SHIELD, {750, 445}}, {TEMPLAR_SHIELD, {835, 470}},
         {TEMPLAR_SHIELD, {920, 445}}, {TEMPLAR_CROSSBOW, {470, 365}},
         {TEMPLAR_CROSSBOW, {1200, 365}}},
        {{TEMPLAR_STANDARD, {835, 323}}, {TEMPLAR_EXECUTIONER, {670, 473}},
         {TEMPLAR_EXECUTIONER, {1000, 473}}, {TEMPLAR_SHIELD, {755, 402}},
         {TEMPLAR_SHIELD, {915, 402}}, {TEMPLAR_CROSSBOW, {1090, 350}}},
    };
    static const struct {
        EnemyType type;
        int health;
        float speed, power;
    } stats[] = {
        [TEMPLAR_SHIELD] = {ENEMY_COMMON_MELEE, 105, 100, 1.15f},
        [TEMPLAR_EXECUTIONER] = {ENEMY_SPECIAL, 135, 115, 1.25f},
        [TEMPLAR_CROSSBOW] = {ENEMY_COMMON_RANGED, 75, 115, 1.25f},
        [TEMPLAR_STANDARD] = {ENEMY_COMMON_MELEE, 90, 84, .9f},
    };
    int formation = wave == 1 ? 0 : 1;
    int count = wave == 1 ? 5 : 6;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        group->list[i] = (Enemy){.state = ENEMY_GONE};
        if (i >= count) continue;
        TemplarRole role = formations[formation][i].role;
        group->list[i] = (Enemy){
            .templarRole = role, .type = stats[role].type, .state = ENEMY_SPAWNING,
            .position = formations[formation][i].position,
            .health = 2 * stats[role].health, .maxHealth = 2 * stats[role].health,
            .speed = stats[role].speed, .power = stats[role].power,
            .attackCooldown = role == TEMPLAR_CROSSBOW ? .65f + .1f * i : .8f,
            .stateTime = -.12f * i, .lastKatanaSwing = -1
        };
    }
}

void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower) {
    group->boneland=clearing->boneland;
    group->cemetery=clearing->cemetery;
    group->village=clearing->village;
    group->villageSpawnRemaining = 0;
    group->villageSpawnTimer = 0.0f;
    group->villageBossDeathWeapon = -1;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    if (clearing->templarField) {
        StartTemplarWave(group, wave);
        return;
    }
    if (clearing->boneland) {
        StartBonelandWave(group, clearing, wave, player, playerPower);
        return;
    }
    if (clearing->chapel || clearing->basement || clearing->invertedChapel) {
        for (int i = 0; i < ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
        return;
    }
    bool clearingWave = ClearingMapId(clearing) == CLEARING_MAP;
    int total = clearingWave ? CLEARING_FIRST_WAVE_ENEMIES + (wave - 1) * CLEARING_WAVE_ENEMY_STEP
                             : 7 + (wave - 1) * 4;
    if (total > ENEMY_COUNT) total = ENEMY_COUNT;
    int minimumSpecials = 1 + (wave - 1) / 2;
    int specials = GetRandomValue(minimumSpecials, minimumSpecials + (wave >= 3));
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        *enemy = (Enemy){.state = ENEMY_GONE};
        if (i >= total) continue;
        enemy->position = SpawnPosition(clearing, player);
        enemy->type = i < specials ? ENEMY_SPECIAL : i%3 == 0 ? ENEMY_COMMON_RANGED : ENEMY_COMMON_MELEE;
        float adjustment = 1.0f + fminf(.15f, fmaxf(0, playerPower - 1.0f) * .05f);
        enemy->maxHealth = (int)((enemy->type == ENEMY_SPECIAL ? 100 : 45) *
                                (1.0f + (wave-1)*.12f) * adjustment);
        if (clearing->cemetery) enemy->maxHealth=(int)ceilf(enemy->maxHealth*1.10f);
        else if (clearingWave) enemy->maxHealth=(int)ceilf(enemy->maxHealth*CLEARING_ENEMY_HEALTH_SCALE);
        enemy->health = enemy->maxHealth;
        enemy->speed = ENEMY_SPEED * (1.0f + (wave-1)*.045f);
        enemy->power = (1.0f + (wave-1)*.09f) * (clearing->cemetery ? 1.10f : 1.0f);
        if (clearingWave) enemy->damageScale = CLEARING_ENEMY_DAMAGE_SCALE;
        enemy->state = ENEMY_SPAWNING;
        enemy->stateTime = -WaveSpawnDelay(wave, i);
        enemy->facing = i%2 ? 2 : 3;
        enemy->lastKatanaSwing = -1;
        enemy->attackCooldown = 0.8f;
    }
}

void StartArenaBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player, float power) {
    group->boneland=clearing->boneland;
    group->cemetery=clearing->cemetery;
    group->village=clearing->village;
    group->villageSpawnRemaining = 0;
    group->villageSpawnTimer = 0.0f;
    group->villageBossDeathWeapon = -1;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    if (clearing->chapel || clearing->basement || clearing->invertedChapel) {
        for (int i = 0; i < ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
        return;
    }
    for (int i=0; i<ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
    Enemy *boss = &group->list[0];
    if (clearing->necromancerArena) { StartNecromancer(group,power); return; }
    int baseHealth = clearing->templarField ? 3000 : clearing->boneland ? 2800
        : clearing->village ? 500 : (clearing->cemetery ? CEMETERY_BOSS_BASE_HEALTH : ARENA_BOSS_BASE_HEALTH);
    Vector2 bossPos = clearing->templarField ? (Vector2){835, 290} : clearing->village
        ? (Vector2){clearing->background.width * 0.62f, clearing->background.height * 0.54f}
        : SpawnPosition(clearing, player);
    *boss = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_SPAWNING,
        .position = bossPos, .maxHealth = (int)(baseHealth*fmaxf(1, power)),
        .speed = clearing->village || clearing->templarField ? 185 : (clearing->cemetery ? 160 : 100),
        .power = power, .attackCooldown = 1.25f, .lastKatanaSwing = -1,
        .bossAttack = (clearing->cemetery ? COVEIRO_ATTACK_COUNT : GUARDIAN_ATTACK_COUNT) - 1};
    boss->health = boss->maxHealth;
    if (clearing->boneland) {
        boss->bossAttack = BONE_DASH;
        boss->speed = 132;
    }
    if (clearing->templarField) {
        boss->templarRole = TEMPLAR_MARSHAL;
        boss->bossAttack = MARSHAL_SLASH;
        boss->attackCooldown = .25f;
    }
}

int EnemiesRemaining(const EnemyGroup *group) {
    int count = 0;
    for (int i=0; i<ENEMY_COUNT; i++) if (group->list[i].state != ENEMY_GONE) count++;
    return count + (group->village ? group->villageSpawnRemaining : 0);
}

float EnemyHurtRadius(const Enemy *enemy) { return enemy->type == ENEMY_BOSS ? 48.0f : ENEMY_HURT_RADIUS; }

bool EnemyCanBeHit(const Enemy *enemy) {
    if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) return false;
    return enemy->state != ENEMY_DIE && enemy->state != ENEMY_GONE &&
           enemy->state != ENEMY_SPAWNING && enemy->state != ENEMY_FALLEN && enemy->health > 0;
}

Vector2 EnemyCenter(const Enemy *enemy) {
    return (Vector2){enemy->position.x, enemy->position.y-(enemy->type == ENEMY_BOSS ? 75.0f : 36.0f)};
}

void DamageEnemy(Enemy *enemy, int damage) {
    DamageEnemyFrom(enemy, damage, enemy->position);
}

void DamageEnemyFrom(Enemy *enemy, int damage, Vector2 source) {
    if (!EnemyCanBeHit(enemy) || damage<=0) return;
    if (enemy->necromancerForm) { DamageNecromancer(enemy,damage); return; }
    static const Vector2 guardDirection[] = {{0,1}, {0,-1}, {-1,0}, {1,0}};
    Vector2 incoming = Vector2Subtract(source, enemy->position);
    if (enemy->templarRole == TEMPLAR_SHIELD &&
        (enemy->state == ENEMY_IDLE || enemy->state == ENEMY_WALK) &&
        Vector2DotProduct(incoming, guardDirection[enemy->facing]) > Vector2Length(incoming) * .55f) {
        damage = damage / 2 + damage % 2;
        if (enemy->health > damage) {
            enemy->health -= damage;
            return;
        }
    }
    enemy->health -= damage;
    if (enemy->type == ENEMY_BOSS && enemy->guardianPhase == GUARDIAN_PHASE_ONE &&
        enemy->health <= enemy->maxHealth / 2) {
        enemy->health = enemy->maxHealth / 2;
        enemy->guardianPhase = GUARDIAN_PHASE_TRANSITION;
        enemy->guardianMelee = false;
        enemy->state = ENEMY_IDLE;
        enemy->stateTime = 0.0f;
    } else if (enemy->health <= 0) {
        enemy->stateTime = 0.0f;
        enemy->health = 0;
        enemy->state = ENEMY_DIE;
        if (!enemy->dropRolled) {
            enemy->dropRolled = true;
            enemy->pickupAvailable = EnemyShouldDrop(enemy->type, GetRandomValue(0, 9999));
        }
    } else if (enemy->type != ENEMY_BOSS) {
        enemy->stateTime = 0.0f;
        enemy->state = ENEMY_HURT;
    }
}

void MoveEnemy(const Clearing *clearing, Enemy *enemy, Vector2 delta) {
    MoveMapActor(clearing, &enemy->position, delta);
}

void KnockbackEnemy(Enemy *enemy, const Clearing *clearing,
                    Vector2 source, float distance) {
    if (!EnemyCanBeHit(enemy)) return;
    if (enemy->templarRole == TEMPLAR_STANDARD ||
        (enemy->templarRole == TEMPLAR_SHIELD && enemy->state != ENEMY_HURT)) return;
    if (enemy->type == ENEMY_BOSS) distance *= .15f;
    Vector2 away = Vector2Subtract(enemy->position, source);
    if (Vector2LengthSqr(away) < 0.001f) return;
    MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(away), distance));
}

static float EnemyAttackPower(const EnemyGroup *group, const Enemy *enemy) {
    if (enemy->templarRole != TEMPLAR_NONE && enemy->templarRole != TEMPLAR_STANDARD) {
        for (int i = 0; i < ENEMY_COUNT; i++) {
            const Enemy *standard = &group->list[i];
            if (standard->templarRole == TEMPLAR_STANDARD && EnemyCanBeHit(standard) &&
                Vector2DistanceSqr(standard->position, enemy->position) <= 220.0f * 220.0f)
                return enemy->power * 1.25f;
        }
    }
    return enemy->power;
}

/* Dano de um acerto. Inimigos com ajuste de mapa (damageScale) arredondam para o
   inteiro mais próximo; os demais mantêm o cálculo antigo, truncado. */
static int EnemyHitDamage(const EnemyGroup *group, const Enemy *enemy, float base) {
    float damage = base * EnemyAttackPower(group, enemy);
    if (enemy->damageScale > 0.0f) return (int)roundf(damage * enemy->damageScale);
    return (int)damage;
}

#define CEMETERY_SPELL_LIFETIME 4.0f

static void FireRangedProjectile(EnemyGroup *group, const Enemy *enemy, Vector2 player) {
    Vector2 origin = EnemyCenter(enemy);
    Vector2 toward = Vector2Subtract((Vector2){player.x,player.y-35},origin);
    if ((group->cemetery || group->boneland || enemy->templarRole == TEMPLAR_CROSSBOW) &&
        Vector2LengthSqr(enemy->attackDirection)>.001f) toward=enemy->attackDirection;
    if (Vector2LengthSqr(toward) < 0.001f) return;
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        if (group->projectiles[i].remaining <= 0.0f) {
            group->projectiles[i] = (EnemyProjectile){origin, Vector2Normalize(toward),
                group->cemetery ? CEMETERY_SPELL_LIFETIME : 1.5f,
                EnemyHitDamage(group, enemy, ENEMY_ATTACK_DAMAGE)};
            return;
        }
    }
}

static bool GuardianEnraged(const Enemy *enemy) {
    return enemy->guardianPhase == GUARDIAN_PHASE_TWO;
}

static void PlaceGuardianRoot(EnemyGroup *group, Vector2 position, GuardianAttack attack, float delay) {
    for (int i = 0; i < ENEMY_ROOT_COUNT; i++) {
        if (group->rootAttacks[i].active) continue;
        group->rootAttacks[i] = (EnemyRoot){.position = position, .age = -delay,
                                           .active = true, .attack = attack};
        return;
    }
}

static void StartGuardianCast(EnemyGroup *group, const Clearing *clearing,
                              Enemy *enemy, Vector2 player) {
    enemy->attackCooldown = GuardianEnraged(enemy) ? 1.95f : 3.0f;
    enemy->bossAttack = (enemy->bossAttack + 1) % GUARDIAN_ATTACK_COUNT;
    if (enemy->bossAttack == GUARDIAN_ROOTS) {
        Vector2 flank = Vector2Scale(Vector2Rotate(enemy->attackDirection, PI / 2), 120);
        const Vector2 targets[] = {player, Vector2Add(player, flank), Vector2Subtract(player, flank)};
        for (int i = 0; i < 3; i++) {
            if (!IsClearingBlocked(clearing, targets[i])) PlaceGuardianRoot(group, targets[i], GUARDIAN_ROOTS, 0);
        }
        return;
    }

    if (enemy->bossAttack == GUARDIAN_CURSE) {
        if (!IsClearingBlocked(clearing, player))
            PlaceGuardianRoot(group, player, GUARDIAN_CURSE, 0);
        return;
    }

    int segments = GuardianEnraged(enemy) ? 6 : 5;
    float stagger = GuardianEnraged(enemy) ? .08f : .10f;
    for (int i = 0; i < segments; i++) {
        float distance = 85 + i * 105;
        Vector2 position = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, distance));
        // Probe between eruptions too, so the wave ends at walls and rocks.
        for (float sample = (i == 0 ? 12 : distance - 96); sample < distance; sample += 9) {
            Vector2 point = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, sample));
            if (IsClearingBlocked(clearing, point)) return;
        }
        if (IsClearingBlocked(clearing, position)) return;
        PlaceGuardianRoot(group, position, GUARDIAN_ROOT_WAVE, i * stagger);
    }
}

static void UpdateGuardianMelee(EnemyGroup *group, const Clearing *clearing, Enemy *enemy, Vector2 player,
                                bool playerDodging, int *playerHealth) {
    if (!enemy->attackConnected && EnemyAnimationFrame(enemy) >= GUARDIAN_MELEE_HIT_FRAME) {
        enemy->attackConnected = true;
        Vector2 offset = Vector2Subtract(player, enemy->position);
        float distance = Vector2Length(offset);
        if (!playerDodging && *playerHealth > 0 && group->playerHitCooldown <= 0 &&
            distance <= GUARDIAN_MELEE_RANGE &&
            Vector2DotProduct(offset, enemy->attackDirection) >= distance * .48f &&
            MapHasClearPath(clearing, enemy->position, player)) {
            *playerHealth = (int)fmaxf(0, *playerHealth - GUARDIAN_MELEE_DAMAGE * enemy->power);
            group->playerHitCooldown = .65f;
        }
    }
    if (enemy->stateTime >= GUARDIAN_MELEE_FRAMES / GUARDIAN_MELEE_FPS) {
        enemy->state = ENEMY_IDLE;
        enemy->stateTime = 0;
        enemy->guardianMelee = false;
    }
}

static void AddEnemyImpact(EnemyGroup *group,Vector2 position,Vector2 direction) {
    for(int i=0;i<16;i++) if(group->impacts[i].remaining<=0) {
        group->impacts[i]=(EnemyImpact){position,.32f,direction};return;
    }
}

static const struct {
    float minRange, range, windup, impactEnd, duration;
    int damage;
} boneAttacks[BONE_ATTACK_COUNT] = {
    [BONE_CHARGE] = {110, 395, .65f, 1.25f, 1.65f, 25},
    [BONE_CLAW] = {0, 115, .32f, .42f, .84f, 19},
    [BONE_DASH] = {0, 245, .80f, 1.00f, 1.46f, 27},
};

static void MoveEnemyStraight(const Clearing *map, Enemy *boss, float distance) {
    int steps = (int)ceilf(distance / 4.0f);
    if (steps <= 0) return;
    Vector2 step = Vector2Scale(boss->attackDirection, distance / steps);
    for (int i = 0; i < steps; i++) {
        Vector2 next = Vector2Add(boss->position, step);
        if (IsMapActorBlocked(map, next)) break;
        boss->position = next;
    }
}

static void BoneLordHit(EnemyGroup *group, Enemy *boss, bool dodging, int *health) {
    bool lockedDash = boss->bossAttack == BONE_DASH;
    if (*health <= 0 || group->practiceInvincible ||
        (!lockedDash && (dodging || group->playerHitCooldown > 0))) return;
    int damage = (int)(boneAttacks[boss->bossAttack].damage * boss->power);
    *health = damage < *health ? *health - damage : 0;
    group->playerHitCooldown = .65f;
}

static void UpdateBoneLord(EnemyGroup *group, const Clearing *map, Enemy *boss,
                           Vector2 player, float dt, bool dodging, int *health) {
    Vector2 toPlayer = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(toPlayer);
    bool enraged = GuardianEnraged(boss);
    if (boss->state != ENEMY_ATTACK) {
        if (*health <= 0) return;
        boss->facing = FacingToward(toPlayer);
        if (boss->attackCooldown <= 0 && distance <= boneAttacks[BONE_CHARGE].range &&
            MapHasClearPath(map, boss->position, player)) {
            for (int offset = 1; offset <= BONE_ATTACK_COUNT; offset++) {
                int attack = (boss->bossAttack + offset) % BONE_ATTACK_COUNT;
                if (distance < boneAttacks[attack].minRange || distance > boneAttacks[attack].range)
                    continue;
                boss->bossAttack = attack;
                boss->attackDirection = distance > .001f ? Vector2Scale(toPlayer, 1 / distance) : (Vector2){0, 1};
                boss->state = ENEMY_ATTACK;
                boss->stateTime = 0;
                boss->attackConnected = false;
                boss->attackBurst = 0;
                return;
            }
        }
        boss->state = distance > 70 ? ENEMY_WALK : ENEMY_IDLE;
        if (boss->state == ENEMY_WALK) {
            boss->walkTime += dt;
            MoveEnemy(map, boss, Vector2Scale(toPlayer, boss->speed * (enraged ? 1.22f : 1.0f) * dt / distance));
        }
        return;
    }

    int attack = boss->bossAttack;
    float time = boss->stateTime;
    float previous = time - dt;
    float windup = boneAttacks[attack].windup;
    float impactEnd = boneAttacks[attack].impactEnd;
    float active = fmaxf(0, fminf(time, impactEnd) - fmaxf(previous, windup));
    if (attack == BONE_CHARGE && active > 0) {
        Vector2 origin = boss->position;
        MoveEnemyStraight(map, boss, active * 560);
        Vector2 segment = Vector2Subtract(boss->position, origin);
        float lengthSquared = Vector2LengthSqr(segment);
        float along = lengthSquared > 0 ? Clamp(Vector2DotProduct(toPlayer, segment) / lengthSquared, 0, 1) : 0;
        Vector2 nearest = Vector2Add(origin, Vector2Scale(segment, along));
        if (!boss->attackConnected && Vector2DistanceSqr(nearest, player) <= 63 * 63 &&
            MapHasClearPath(map, nearest, player)) {
            boss->attackConnected = true;
            BoneLordHit(group, boss, dodging, health);
        }
    } else if (attack == BONE_CLAW && !boss->attackConnected && time >= windup) {
        if (distance <= boneAttacks[attack].range &&
            Vector2DotProduct(boss->attackDirection, toPlayer) >= distance * .45f &&
            MapHasClearPath(map, boss->position, player))
            BoneLordHit(group, boss, dodging, health);
        boss->attackConnected = true;
    } else if (attack == BONE_DASH) {
        if (boss->attackBurst == 0 && time >= windup) {
            boss->attackBurst = 1;
            bool locked = distance <= boneAttacks[attack].range && MapHasClearPath(map, boss->position, player);
            boss->attackConnected = !locked;
            if (locked && distance > .001f) boss->attackDirection = Vector2Scale(toPlayer, 1 / distance);
            boss->facing = FacingToward(boss->attackDirection);
            float travel = locked ? fmaxf(0, distance - 36) : boneAttacks[attack].range;
            boss->attackTarget = Vector2Add(boss->position, Vector2Scale(boss->attackDirection, travel));
        }
        if (active > 0) {
            float remaining = Vector2Distance(boss->position, boss->attackTarget);
            float fraction = active / (impactEnd - fmaxf(previous, windup));
            MoveEnemyStraight(map, boss, remaining * fraction);
        }
        if (previous < impactEnd && time >= impactEnd && !boss->attackConnected) {
            BoneLordHit(group, boss, dodging, health);
            boss->attackConnected = true;
        }
    }
    if (time >= boneAttacks[attack].duration) {
        boss->state = ENEMY_IDLE;
        boss->stateTime = 0;
        boss->attackCooldown = enraged ? .43f : .72f;
    }
}

static void HurtByTemplar(EnemyGroup *group, Vector2 player, int *health, bool dodging, int damage) {
    if (dodging || group->practiceInvincible || group->playerHitCooldown > 0 || *health <= 0 || damage <= 0)
        return;
    *health = damage < *health ? *health - damage : 0;
    group->playerHitCooldown = .65f;
    group->templarBlood = (EnemyImpact){.position={player.x, player.y - 36}, .remaining=.36f};
}

static void StartMarshalSkill(Enemy *boss, int skill, Vector2 direction) {
    boss->state = ENEMY_ATTACK;
    boss->bossAttack = skill;
    boss->stateTime = 0;
    boss->attackConnected = false;
    boss->attackDirection = direction;
    boss->facing = FacingToward(direction);
}

static void UpdateMarshal(EnemyGroup *group, const Clearing *map, Enemy *boss, Vector2 player,
                          float dt, bool dodging, int *health) {
    Vector2 delta = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(delta);
    Vector2 toward = distance > .001f ? Vector2Scale(delta, 1 / distance) : (Vector2){0,1};
    bool enraged = GuardianEnraged(boss);
    if (boss->state != ENEMY_ATTACK) {
        if (*health <= 0) return;
        boss->facing = FacingToward(toward);
        if (boss->attackCooldown <= 0 && (boss->attackBurst == 0 || distance < 415) &&
            MapHasClearPath(map, boss->position, player)) {
            static const int opening[] = {MARSHAL_CHARGE, MARSHAL_SLASH, MARSHAL_SPIN};
            static const int enragedPattern[] = {
                MARSHAL_CHARGE, MARSHAL_SHOCKWAVE, MARSHAL_SPIN, MARSHAL_RUPTURE,
            };
            int skill = enraged ? enragedPattern[boss->attackBurst % 4] : opening[boss->attackBurst % 3];
            boss->attackBurst = boss->attackBurst % 12 + 1;
            if (distance > 220 && skill != MARSHAL_SHOCKWAVE) skill = MARSHAL_CHARGE;
            StartMarshalSkill(boss, skill, toward);
            return;
        }
        boss->state = boss->attackBurst != 0 && distance > 93 ? ENEMY_WALK : ENEMY_IDLE;
        if (boss->state == ENEMY_WALK) {
            boss->walkTime += dt;
            MoveEnemy(map, boss, Vector2Scale(toward, boss->speed * dt * (enraged ? 1.13f : 1)));
        }
        return;
    }
    int skill = boss->bossAttack;
    float time = boss->stateTime, impact = marshalSkills[skill].impact;
    float active = fmaxf(0, fminf(time, marshalSkills[skill].activeEnd) - fmaxf(time - dt, impact));
    float reach = marshalSkills[skill].reach;
    if (skill == MARSHAL_CHARGE && active > 0) {
        Vector2 origin = boss->position;
        MoveEnemyStraight(map, boss, active * (enraged ? 620 : 555));
        Vector2 segment = Vector2Subtract(boss->position, origin);
        float lengthSquared = Vector2LengthSqr(segment);
        float along = lengthSquared > 0 ? Clamp(Vector2DotProduct(delta, segment) / lengthSquared, 0, 1) : 0;
        Vector2 nearest = Vector2Add(origin, Vector2Scale(segment, along));
        if (!boss->attackConnected && Vector2DistanceSqr(nearest, player) <= reach * reach &&
            MapHasClearPath(map, nearest, player)) {
            boss->attackConnected = true;
            HurtByTemplar(group, player, health, dodging, (int)(marshalSkills[skill].damage * boss->power));
        }
    } else if (skill == MARSHAL_SHOCKWAVE) {
        float travel = marshalSkills[skill].activeEnd - impact;
        float inner = reach * Clamp((time - dt - impact) / travel, 0, 1) - 22;
        float outer = reach * Clamp((time - impact) / travel, 0, 1) + 22;
        if (!boss->attackConnected && time >= impact && time - dt <= marshalSkills[skill].activeEnd &&
            distance >= inner && distance <= outer && MapHasClearPath(map, boss->position, player)) {
            boss->attackConnected = true;
            HurtByTemplar(group, player, health, dodging, (int)(marshalSkills[skill].damage * boss->power));
        }
    } else if (skill != MARSHAL_CHARGE && !boss->attackConnected && time >= impact) {
        boss->attackConnected = true;
        if (distance <= reach && Vector2DotProduct(toward, boss->attackDirection) >= marshalSkills[skill].arcDot &&
            MapHasClearPath(map, boss->position, player)) {
            HurtByTemplar(group, player, health, dodging, (int)(marshalSkills[skill].damage * boss->power));
        }
    }
    if (time < marshalSkills[skill].duration) return;
    if (enraged && skill == MARSHAL_CHARGE && *health > 0) {
        StartMarshalSkill(boss, MARSHAL_RETURN, toward);
        return;
    }
    boss->state = ENEMY_IDLE;
    boss->stateTime = 0;
    boss->attackCooldown = enraged ? .22f : .43f;
}

void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float dt, bool playerDodging,
                   int *playerHealth, int playerMaxHealth) {
    if (clearing->necromancerArena) {
        UpdateNecromancer(group,clearing,player,dt,playerDodging,playerHealth);
        return;
    }
    group->playerHitCooldown = fmaxf(0.0f, group->playerHitCooldown-dt);
    group->templarBlood.remaining = fmaxf(0, group->templarBlood.remaining - dt);
    for (int i=0;i<16;i++) group->impacts[i].remaining=fmaxf(0,group->impacts[i].remaining-dt);
    if (group->cemetery && group->list[0].type == ENEMY_BOSS &&
        (group->list[0].guardianPhase == GUARDIAN_PHASE_TRANSITION || group->list[0].state == ENEMY_DIE))
        ClearEnemyAttacks(group);
    if (group->cemetery)
        CemeteryUpdateProjectiles(group, clearing, player, dt, playerDodging, playerHealth);
    if (group->village)
        VillageUpdateHorde(group, clearing, player, dt);
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE && enemy->pickupAvailable &&
            *playerHealth > 0 && *playerHealth < playerMaxHealth &&
            Vector2Distance(enemy->position, player) < 26.0f) {
            *playerHealth += 15;
            if (*playerHealth > playerMaxHealth) *playerHealth = playerMaxHealth;
            enemy->pickupAvailable = false;
        }
        if (enemy->state == ENEMY_GONE) continue;
        if (enemy->state == ENEMY_FALLEN) continue;
        enemy->stateTime += dt;
        if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
            ClearEnemyAttacks(group);
            if (enemy->stateTime >= GUARDIAN_PHASE_TRANSITION_TIME) {
                enemy->guardianPhase = GUARDIAN_PHASE_TWO;
                enemy->stateTime = 0;
                /* The Coveiro opens with a charge and the Guardian with a curse. */
                enemy->bossAttack = enemy->templarRole == TEMPLAR_MARSHAL ? MARSHAL_SLASH :
                    group->boneland ? BONE_CLAW : group->cemetery ? COVEIRO_SHOVEL : GUARDIAN_ROOT_WAVE;
                enemy->attackCooldown = .20f;
                enemy->guardianMeleeCooldown = .90f;
            }
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            float duration = IsGuardianSpawn(group, enemy)
                ? GUARDIAN_SPAWN_DURATION : enemy->templarRole == TEMPLAR_STANDARD ? templarArrivalTime + templarPlantTime
                    : enemy->templarRole != TEMPLAR_NONE ? templarArrivalTime
                    : group->village && enemy->type == ENEMY_COMMON_MELEE ? VILLAGE_ZOMBIE_EMERGE_TIME : 1.0f;
            if (enemy->stateTime >= duration) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        enemy->attackCooldown = fmaxf(0.0f, enemy->attackCooldown-dt);
        if (enemy->type == ENEMY_BOSS)
            enemy->guardianMeleeCooldown = fmaxf(0.0f, enemy->guardianMeleeCooldown - dt);
        enemy->bibleHitCooldown = fmaxf(0.0f, enemy->bibleHitCooldown-dt);
        if (enemy->state == ENEMY_DIE) {
            if (enemy->type == ENEMY_BOSS) ClearEnemyAttacks(group);
            bool humanBoss = enemy->type == ENEMY_BOSS &&
                             (group->village || enemy->templarRole == TEMPLAR_MARSHAL);
            float dieDuration = humanBoss ? 1.35f : ENEMY_DEFEAT_DURATION;
            if (enemy->stateTime >= dieDuration) {
                bool requiresSeal = enemy->type == ENEMY_BOSS ||
                    (enemy->templarRole == TEMPLAR_NONE && enemy->type == ENEMY_SPECIAL);
                enemy->state = requiresSeal ? ENEMY_FALLEN : ENEMY_GONE;
            }
            continue;
        }
        if (enemy->state == ENEMY_HURT) {
            if (enemy->stateTime >= 0.32f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (group->boneland && enemy->type == ENEMY_BOSS) {
            UpdateBoneLord(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (group->cemetery && enemy->type == ENEMY_BOSS) {
            CemeteryUpdateBoss(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (group->cemetery && enemy->type == ENEMY_SPECIAL) {
            CemeteryUpdateSpecial(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (enemy->templarRole == TEMPLAR_MARSHAL) {
            UpdateMarshal(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (group->village && enemy->type == ENEMY_BOSS) {
            VillageUpdateBoss(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (enemy->templarRole == TEMPLAR_STANDARD) continue; /* Estabelece a posição ao pé da escadaria. */
        if (enemy->state == ENEMY_ATTACK) {
            if (enemy->templarRole == TEMPLAR_EXECUTIONER) {
                float t = enemy->stateTime;
                float charge = fmaxf(0, fminf(t, executionerImpact) - fmaxf(t - dt, .45f));
                MoveEnemy(clearing, enemy, Vector2Scale(enemy->attackDirection, 390 * charge));
                if (t >= executionerImpact && !enemy->attackConnected) {
                    enemy->attackConnected = true;
                    Vector2 toward = Vector2Subtract(player, enemy->position);
                    if (Vector2Length(toward) <= 106 &&
                        Vector2DotProduct(toward, enemy->attackDirection) >= -.15f * Vector2Length(toward) &&
                        MapHasClearPath(clearing, enemy->position, player))
                        HurtByTemplar(group, player, playerHealth, playerDodging,
                                      (int)(18 * EnemyAttackPower(group, enemy)));
                }
                if (t >= 1.45f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            if (enemy->type == ENEMY_BOSS) {
                if (enemy->guardianMelee) {
                    UpdateGuardianMelee(group, clearing, enemy, player, playerDodging, playerHealth);
                    continue;
                }
                bool roots = enemy->bossAttack == GUARDIAN_ROOTS;
                bool curse = enemy->bossAttack == GUARDIAN_CURSE;
                float echoTime = roots ? GUARDIAN_ROOT_ECHO_TIME : GUARDIAN_CURSE_ECHO_TIME;
                if (GuardianEnraged(enemy) && (roots || curse) &&
                    !enemy->attackConnected && enemy->stateTime >= echoTime) {
                    enemy->attackConnected = true;
                    if (!IsClearingBlocked(clearing, player))
                        PlaceGuardianRoot(group, player, roots ? GUARDIAN_ROOTS : GUARDIAN_CURSE, 0);
                }
                if (enemy->stateTime >= 1.5f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            if (enemy->type == ENEMY_SPECIAL) {
                if (enemy->stateTime >= SPECIAL_CHARGE_START && enemy->stateTime < SPECIAL_CHARGE_END) {
                    MoveEnemy(clearing, enemy, Vector2Scale(enemy->attackDirection, 360 * dt));
                    if (!enemy->attackConnected && !playerDodging && group->playerHitCooldown <= 0 &&
                        Vector2Distance(player, enemy->position) < 65 && *playerHealth > 0 &&
                        MapHasClearPath(clearing, enemy->position, player)) {
                        float hit = enemy->damageScale > 0.0f ? (float)EnemyHitDamage(group, enemy, 18)
                                                              : 18*EnemyAttackPower(group, enemy);
                        *playerHealth = (int)fmaxf(0, *playerHealth - hit);
                        group->playerHitCooldown = .65f;
                        enemy->attackConnected = true;
                    }
                }
                if (enemy->stateTime >= SPECIAL_ATTACK_END) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            bool ranged = enemy->type == ENEMY_COMMON_RANGED;
            bool mage = group->cemetery && ranged;
            bool cultist = group->boneland && ranged;
            bool villageZombie = group->village && enemy->type == ENEMY_COMMON_MELEE;
            bool clearingMelee = !group->cemetery && !villageZombie && enemy->type == ENEMY_COMMON_MELEE;
            bool crossbow = enemy->templarRole == TEMPLAR_CROSSBOW;
            bool shield = enemy->templarRole == TEMPLAR_SHIELD;
            if (shield) {
                float step = fmaxf(0, fminf(enemy->stateTime, shieldImpact) -
                                      fmaxf(enemy->stateTime - dt, shieldImpact - .13f));
                MoveEnemy(clearing, enemy, Vector2Scale(enemy->attackDirection, step * 180));
            }
            float release = crossbow ? crossbowRelease : shield ? shieldImpact :
                cultist ? CULTIST_RELEASE_TIME : mage ? CEM_LANTERN_RELEASE_TIME :
                villageZombie ? VILLAGE_ZOMBIE_HIT_TIME :
                group->cemetery && enemy->type==ENEMY_COMMON_MELEE ? CEM_ZOMBIE_HIT_TIME : CLEARING_ARCHER_RELEASE_TIME;
            bool impact = clearingMelee && !shield ? ClearingEnemyFrame(enemy, group->boneland) >= CLEARING_MELEE_IMPACT_FRAME :
                          enemy->stateTime >= release;
            if (!enemy->attackConnected && impact) {
                enemy->attackConnected = true;
                if (enemy->type == ENEMY_COMMON_RANGED) {
                    if (!mage && !cultist && enemy->templarRole != TEMPLAR_CROSSBOW) {
                        Vector2 aim = Vector2Subtract((Vector2){player.x, player.y-35}, EnemyCenter(enemy));
                        if (Vector2LengthSqr(aim) > .001f) enemy->facing = FacingToward(aim);
                    }
                    FireRangedProjectile(group, enemy, player);
                } else if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0 &&
                    Vector2Distance(enemy->position, player) <= ENEMY_ATTACK_RANGE+8.0f &&
                    (!shield || Vector2DotProduct(Vector2Subtract(player, enemy->position),
                                                 enemy->attackDirection) > 0) &&
                    MapHasClearPath(clearing, enemy->position, player)) {
                    int damage = EnemyHitDamage(group, enemy, ENEMY_ATTACK_DAMAGE);
                    if (shield) HurtByTemplar(group, player, playerHealth, playerDodging, damage);
                    else {
                        *playerHealth = damage < *playerHealth ? *playerHealth - damage : 0;
                        group->playerHitCooldown = .65f;
                    }
                }
            }
            if (enemy->stateTime >= (crossbow ? 1.18f : shield ? .85f : cultist ? CULTIST_CAST_DURATION : mage ? .95f : CLEARING_MELEE_ATTACK_DURATION)) {
                enemy->state = ENEMY_IDLE;
                enemy->stateTime = 0;
            }
            continue;
        }
        if (*playerHealth <= 0) { enemy->state = ENEMY_IDLE; continue; }
        Vector2 toPlayer = Vector2Subtract(player, enemy->position);
        float distance = Vector2Length(toPlayer);
        if (distance > 0.001f) enemy->facing = FacingToward(toPlayer);
        if (enemy->type == ENEMY_BOSS || enemy->type == ENEMY_SPECIAL) {
            float range = enemy->type == ENEMY_BOSS ? 560 : 280;
            bool melee = enemy->type == ENEMY_BOSS && distance <= GUARDIAN_MELEE_RANGE &&
                         enemy->guardianMeleeCooldown <= 0;
            if (melee || (distance <= range && enemy->attackCooldown <= 0)) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0;
                enemy->attackConnected = false;
                enemy->attackDirection = distance > .01f ? Vector2Normalize(toPlayer) : (Vector2){0,1};
                if (melee) {
                    const Vector2 directions[] = {{0,1}, {0,-1}, {-1,0}, {1,0}};
                    enemy->guardianMelee = true;
                    enemy->guardianMeleeCooldown = GuardianEnraged(enemy) ? 1.25f : GUARDIAN_MELEE_COOLDOWN;
                    enemy->attackDirection = directions[enemy->facing];
                } else if (enemy->type == ENEMY_BOSS) {
                    StartGuardianCast(group, clearing, enemy, player);
                } else {
                    enemy->attackCooldown = 2.75f / fminf(enemy->power, 1.36f);
                }
            } else if (distance > (enemy->type == ENEMY_BOSS ? 110 : 100)) {
                enemy->state = ENEMY_WALK;
                enemy->walkTime += dt;
                float speed = enemy->speed * (GuardianEnraged(enemy) ? 1.5f : 1.0f);
                MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), speed*dt));
            } else enemy->state = ENEMY_IDLE;
        } else if (enemy->type == ENEMY_COMMON_RANGED) {
            bool canSeePlayer = (!group->boneland && enemy->templarRole != TEMPLAR_CROSSBOW) ||
                                MapHasClearPath(clearing, enemy->position, player);
            float maxDistance = group->boneland ? 350.0f : RANGED_MAX_DISTANCE;
            if (distance <= maxDistance && enemy->attackCooldown <= 0.0f && canSeePlayer) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0.0f;
                enemy->attackConnected = false;
                enemy->attackDirection = Vector2Normalize(toPlayer);
                enemy->attackCooldown = enemy->templarRole == TEMPLAR_CROSSBOW ? 2.05f :
                    group->boneland ? 2.15f : 1.5f / fminf(enemy->power, 1.36f);
            } else if (distance < RANGED_MIN_DISTANCE || distance > maxDistance || !canSeePlayer) {
                enemy->state = ENEMY_WALK;
                enemy->walkTime += dt;
                Vector2 step = Vector2Scale(Vector2Normalize(toPlayer), enemy->speed * dt);
                MoveEnemy(clearing, enemy, canSeePlayer && distance < RANGED_MIN_DISTANCE ?
                          Vector2Negate(step) : step);
            } else {
                enemy->state = ENEMY_IDLE;
            }
        } else if (distance <= ENEMY_ATTACK_RANGE && enemy->attackCooldown <= 0.0f) {
            enemy->state = ENEMY_ATTACK;
            enemy->stateTime = 0.0f;
            enemy->attackConnected = false;
            enemy->attackDirection = Vector2Normalize(toPlayer);
            enemy->attackCooldown = 1.25f / fminf(enemy->power, 1.36f);
        } else if (distance > ENEMY_ATTACK_RANGE*0.85f) {
            enemy->state = ENEMY_WALK;
            enemy->walkTime += dt;
            MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(toPlayer), enemy->speed*dt));
        } else {
            enemy->state = ENEMY_IDLE;
        }
    }
    for (int i=0; i<ENEMY_ROOT_COUNT; i++) {
        EnemyRoot *root = &group->rootAttacks[i];
        if (!root->active) continue;
        root->age += dt;
        const GuardianHazard *hazard = &guardianHazards[root->attack];
        if (root->age >= hazard->hitStart && root->age < hazard->hitEnd && !root->hit && !playerDodging &&
            group->playerHitCooldown <= 0 && *playerHealth > 0 &&
            Vector2Distance(player, root->position) < hazard->radius) {
            *playerHealth = (int)fmaxf(0, *playerHealth - hazard->damage * group->list[0].power);
            group->playerHitCooldown = .65f;
            root->hit = true;
        }
        if (root->age >= hazard->duration) root->active = false;
    }
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        float speed = group->cemetery ? 273.0f : group->boneland ? CULTIST_SHOT_SPEED : RANGED_SHOT_SPEED;
        Vector2 movement = Vector2Scale(shot->direction, speed * dt);
        int steps = (int)(Vector2Length(movement) / 6.0f) + 1;
        Vector2 step = Vector2Scale(movement, 1.0f / steps);
        for (int j = 0; j < steps && shot->remaining > 0.0f; j++) {
            Vector2 next = Vector2Add(shot->position, step);
            Vector2 collisionPoint=next;
            if (group->cemetery || group->boneland) collisionPoint.y+=35;
            if (IsClearingBlocked(clearing, collisionPoint)) {
                if (group->cemetery || group->boneland) AddEnemyImpact(group,next,shot->direction);
                shot->remaining = 0.0f;
            } else {
                shot->position = next;
                Vector2 playerCenter = {player.x, player.y - 35.0f};
                float radius = group->cemetery ? 30 : 24;
                if (Vector2DistanceSqr(next, playerCenter) <= radius * radius) {
                    if (group->cemetery || group->boneland) AddEnemyImpact(group,next,shot->direction);
                    if (clearing->templarField)
                        HurtByTemplar(group, player, playerHealth, playerDodging, shot->damage);
                    else if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0) {
                        *playerHealth -= shot->damage;
                        if (*playerHealth < 0) *playerHealth = 0;
                        group->playerHitCooldown = 0.65f;
                    }
                    shot->remaining = 0.0f;
                }
            }
        }
        if (shot->remaining > 0.0f) {
            shot->remaining = fmaxf(0.0f, shot->remaining - dt);
            if (group->cemetery && shot->remaining == 0.0f)
                AddEnemyImpact(group,shot->position,shot->direction);
        }
    }
}

static void DrawGuardianWaveFrame(Texture2D texture, Vector2 position, float elapsed,
                                  bool eruption, bool flip) {
    int frame = (int)(elapsed * GUARDIAN_WAVE_FPS);
    if (frame >= GUARDIAN_WAVE_FRAMES) return;
    const float scale = .50f;
    const float width = GUARDIAN_WAVE_TILE_WIDTH * scale;
    const float height = GUARDIAN_WAVE_TILE_HEIGHT * scale;
    float ground = (eruption ? 217 : 252) * scale;
    DrawTexturePro(texture,
        (Rectangle){frame * GUARDIAN_WAVE_TILE_WIDTH, eruption ? GUARDIAN_WAVE_TILE_HEIGHT : 0,
                    flip ? -GUARDIAN_WAVE_TILE_WIDTH : GUARDIAN_WAVE_TILE_WIDTH, GUARDIAN_WAVE_TILE_HEIGHT},
        (Rectangle){position.x - width / 2, position.y - ground, width, height},
        (Vector2){0}, 0, WHITE);
}

static void DrawGuardianCurse(Texture2D texture, const EnemyRoot *root, const GuardianHazard *hazard) {
    bool charging = root->age < hazard->warning;
    if (root->age < hazard->hitEnd) {
        DrawCircleV(root->position, hazard->radius, Fade(PURPLE, charging ? .23f : .36f));
        DrawCircleLines((int)root->position.x, (int)root->position.y, hazard->radius, hazard->warningColor);
    }
    float elapsed = root->age - hazard->warning;
    if (charging) elapsed += GUARDIAN_CURSE_CHARGE_FRAMES / GUARDIAN_CURSE_FPS;
    if (elapsed < 0) return;
    int count = charging ? GUARDIAN_CURSE_CHARGE_FRAMES : GUARDIAN_CURSE_IMPACT_FRAMES;
    int frame = (int)(elapsed * GUARDIAN_CURSE_FPS);
    if (frame >= count) frame = count - 1;
    const float scale = .80f;
    DrawTexturePro(texture,
        (Rectangle){frame * GUARDIAN_CURSE_TILE_WIDTH, charging ? 0 : GUARDIAN_CURSE_TILE_HEIGHT,
                    GUARDIAN_CURSE_TILE_WIDTH, GUARDIAN_CURSE_TILE_HEIGHT},
        (Rectangle){root->position.x - GUARDIAN_CURSE_TILE_WIDTH * scale / 2,
                    root->position.y - 250 * scale,
                    GUARDIAN_CURSE_TILE_WIDTH * scale, GUARDIAN_CURSE_TILE_HEIGHT * scale},
        (Vector2){0}, 0, WHITE);
}

static void DrawEmberBurst(Vector2 point, float progress) {
    float alpha = 1 - progress;
    for (int i = 0; i < 8; i++) {
        float angle = i * (PI / 4) + .6f;
        float reach = (7 + 22 * progress) * (i % 2 ? .72f : 1);
        Vector2 spark = Vector2Add(point, (Vector2){cosf(angle) * reach, sinf(angle) * reach});
        DrawRectangleV(spark, (Vector2){4 * alpha, 4 * alpha},
                       Fade(i % 2 ? GOLD : ORANGE, alpha));
    }
    DrawCircleV(point, 9 * alpha, Fade((Color){255, 224, 151, 255}, alpha * .85f));
}

static void DrawCemeterySpell(Texture2D texture, Vector2 position, Vector2 direction, int frame, Color tint) {
    float cellWidth=texture.width/8.0f;
    float width=108.0f, height=width*texture.height/cellWidth;
    float angle=atan2f(direction.y,direction.x)*RAD2DEG;
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(texture,(Rectangle){frame*cellWidth,0,cellWidth,texture.height},
        (Rectangle){position.x,position.y,width,height},
        (Vector2){width*.75f,height*.53f},angle,tint);
    EndBlendMode();
}

static void DrawRangedProjectiles(const EnemyGroup *group) {
    if (group->boneland) for (int i = 0; i < 16; i++) {
        const EnemyImpact *impact = &group->impacts[i];
        if (impact->remaining > 0)
            DrawEmberBurst(impact->position, 1 - impact->remaining / .32f);
    }
    if (group->cemetery) for(int i=0;i<16;i++) {
        const EnemyImpact *impact=&group->impacts[i];
        if (impact->remaining<=0) continue;
        int frame=impact->remaining>.21f?6:7;
        DrawCemeterySpell(group->cemeterySpell,impact->position,impact->direction,frame,
            Fade(WHITE,fminf(1,impact->remaining/.13f)));
    }
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        const EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        if (group->cemetery) {
            int step=(int)((CEMETERY_SPELL_LIFETIME-shot->remaining)*16);
            int frame=step<2?step:2+(step-2)%4;
            DrawCemeterySpell(group->cemeterySpell,shot->position,shot->direction,frame,WHITE);
            continue;
        }
        if (group->boneland) {
            float age = 1.5f - shot->remaining;
            Vector2 normal = {-shot->direction.y, shot->direction.x};
            DrawCircleV(shot->position, 14 + 2 * sinf(age * 19), Fade(MAROON, .72f));
            for (int i = 5; i > 0; i--) {
                float phase = age * 34 + i * 2;
                Vector2 tail = Vector2Subtract(shot->position, Vector2Scale(shot->direction, (float)i * 5));
                tail = Vector2Add(tail, Vector2Scale(normal, sinf(phase) * i));
                float size = 8 - i + sinf(phase);
                DrawRectangleV(Vector2Subtract(tail, (Vector2){size / 2, size / 2}),
                               (Vector2){size,size}, Fade(i < 3 ? GOLD : ORANGE, 1 - i * .12f));
            }
            DrawCircleV(shot->position, 8, ORANGE);
            DrawCircleV(shot->position, 5 + 1.4f * cosf(age * 21), (Color){255, 234, 171, 255});
            continue;
        }
        Vector2 tail = Vector2Subtract(shot->position, Vector2Scale(shot->direction, 16.0f));
        DrawLineEx(tail, shot->position, 3.0f, GOLD);
        DrawCircleV(shot->position, 3.0f, RAYWHITE);
    }
}

static void DrawGuardianSpawn(Texture2D texture, const Enemy *enemy) {
    int frame = GuardianSpawnFrame(enemy);
    // Pula a pose em que o Guardião afunda no meio da subida.
    if (frame >= GUARDIAN_SPAWN_RECOIL_FRAME) frame++;
    int row = frame / GUARDIAN_SPAWN_COLUMNS;
    int column = frame % GUARDIAN_SPAWN_COLUMNS;
    int left = guardianSpawnRows[row].edges[column];
    int top = guardianSpawnRows[row].top;
    Rectangle source = {left, top, guardianSpawnRows[row].edges[column + 1] - left,
                        guardianSpawnRows[row].bottom - top};
    Vector2 pivot = {(guardianSpawnRows[row].pivots[column] - left) * GUARDIAN_SPAWN_SCALE,
                     (guardianSpawnRows[row].ground - top) * GUARDIAN_SPAWN_SCALE};
    Rectangle destination = {enemy->position.x, enemy->position.y,
                             source.width * GUARDIAN_SPAWN_SCALE, source.height * GUARDIAN_SPAWN_SCALE};
    DrawTexturePro(texture, source, destination, pivot, 0, WHITE);
}

static void DrawGuardianDefeat(Texture2D texture, const Enemy *enemy) {
    int row = enemy->facing;
    int frame = GuardianDefeatFrame(enemy);
    int left = guardianDefeatRows[row].edges[frame];
    int right = guardianDefeatRows[row].edges[frame + 1];
    int top = guardianDefeatRows[row].top;
    int ground = guardianDefeatRows[row].ground;
    int width = right - left;
    int height = guardianDefeatRows[row].bottom - top;
    float scale = guardianDefeatRows[row].scale;
    // As duas linhas laterais da arte original estão voltadas para a esquerda.
    bool flip = row == 3;
    int anchor = guardianDefeatPivots[frame];
    Rectangle source = {left, top, flip ? -width : width, height};
    Vector2 pivot = {(flip ? right - anchor : anchor - left) * scale, (ground - top) * scale};
    Rectangle destination = {enemy->position.x, enemy->position.y, width * scale, height * scale};
    DrawTexturePro(texture, source, destination, pivot, 0, WHITE);
    if (enemy->state == ENEMY_FALLEN)
        DrawGameText("E: selar", (int)enemy->position.x - 30, (int)enemy->position.y + 10, 18, GOLD);
}

typedef struct ClearingEnemyRow {
    int top, bottom, ground;
    float scale;
    int edges[9];
    int pivots[8];
} ClearingEnemyRow;

// Os pivôs acompanham o corpo e a linha do chão, mesmo quando o arco se estende para o lado.
static const ClearingEnemyRow clearingMeleeIdleRows[] = {
    {747, 995, 961, .36f, {160,406,657,906,1155,1430}, {278,529,781,1034,1285}},
    {0,   254, 238, .35f, {160,406,657,906,1155,1430}, {279,530,785,1036,1289}},
    {254, 502, 471, .39f, {160,406,657,906,1155,1430}, {277,532,785,1037,1289}},
    {502, 747, 710, .39f, {160,406,657,906,1155,1430}, {280,531,780,1034,1286}},
};

static const ClearingEnemyRow clearingMeleeWalkRows[] = {
    {270, 480, 449, .46f, {157,340,500,660,820,980,1140,1300,1480}, {257,419,579,741,899,1058,1219,1378}},
    {0,   245, 212, .43f, {157,340,500,660,820,980,1140,1300,1480}, {256,417,577,736,895,1056,1215,1376}},
    {515, 730, 698, .44f, {157,340,500,660,820,980,1140,1300,1480}, {265,429,592,750,909,1068,1230,1388}},
    {770, 985, 951, .46f, {157,340,500,660,820,980,1140,1300,1480}, {258,414,575,732,893,1050,1216,1376}},
};

static const ClearingEnemyRow clearingMeleeAttackRows[] = {
    {18,  257, 240, .53f, {150,317,480,629,796,981,1152,1329,1490}, {235,395,550,713,877,1049,1212,1415}},
    {272, 510, 483, .49f, {151,319,485,644,790,990,1144,1327,1490}, {236,397,557,715,881,1056,1224,1415}},
    {751, 960, 936, .49f, {149,319,484,642,799,997,1151,1327,1490}, {237,402,560,718,880,1053,1223,1414}},
};

static const ClearingEnemyRow clearingArcherIdleRows[] = {
    {52,  252, 231, .45f, {416,595,773,950,1123}, {511,688,863,1038}},
    {514, 727, 705, .44f, {416,595,773,950,1123}, {507,686,860,1039}},
    {769, 966, 943, .46f, {416,595,773,950,1123}, {514,692,868,1044}},
    {285, 487, 465, .46f, {416,595,773,950,1123}, {502,684,859,1035}},
};

static const ClearingEnemyRow clearingArcherWalkRows[] = {
    {71,  252, 238, .48f, {98,282,462,645,805,974,1132,1290,1454}, {178,382,558,728,898,1061,1219,1374}},
    {517, 705, 690, .46f, {98,282,462,645,805,974,1132,1290,1454}, {182,379,565,732,899,1068,1221,1380}},
    {765, 940, 921, .50f, {98,282,462,645,805,974,1132,1290,1454}, {176,378,562,730,888,1051,1214,1374}},
    {302, 483, 462, .50f, {98,282,462,645,805,974,1132,1290,1454}, {177,381,562,734,899,1065,1220,1373}},
};

static const ClearingEnemyRow clearingArcherAttackRows[] = {
    {305, 516, 461, .54f, {171,324,486,647,813,1030,1190,1360,1514}, {249,410,571,734,923,1099,1285,1447}},
    {29,  277, 254, .57f, {174,324,486,647,823,1007,1190,1360,1514}, {250,411,571,732,921,1095,1284,1450}},
    {529, 716, 689, .53f, {171,325,486,633,822,1003,1190,1365,1514}, {248,407,570,750,925,1103,1288,1453}},
    {751, 944, 917, .54f, {171,324,488,649,818,1034,1190,1362,1514}, {249,409,573,741,926,1098,1283,1449}},
};

static const ClearingEnemyRow clearingSpecialIdleRows[] = {
    {0,   482, 449, .22f, {150,762},  {472}},
    {490, 974, 936, .22f, {128,759},  {472}},
    {490, 974, 936, .22f, {840,1400}, {1108}},
    {0,   482, 449, .22f, {870,1405}, {1113}},
};

static const ClearingEnemyRow clearingSpecialWalkRows[] = {
    {8,   273, 260, .39f, {162,488,796,1122,1451}, {313,638,966,1275}},
    {280, 514, 502, .44f, {162,481,796,1119,1451}, {314,642,964,1280}},
    {514, 745, 729, .45f, {161,479,791,1118,1451}, {296,617,929,1271}},
    {749, 993, 972, .44f, {152,469,790,1111,1451}, {299,624,945,1270}},
};

static const ClearingEnemyRow clearingSpecialAttackRows[] = {
    {0,   258, 253,  .51f, {0,260,458,673,949,1290,1536},         {139,338,563,801,1086,1415}},
    {258, 520, 506,  .49f, {0,261,459,655,928,1151,1326,1536},    {139,346,561,774,1054,1242,1425}},
    {520, 765, 745,  .49f, {0,258,434,694,939,1138,1330,1536},    {127,329,587,784,1034,1237,1425}},
    {775, 1024, 995, .48f, {0,250,462,691,947,1137,1334},         {123,363,574,818,1039,1251}},
};

static void DrawBonelandEnemy(const EnemyGroup *group, const Enemy *enemy) {
    // Cada folha tem frente, esquerda, direita e costas, com espera, dois passos e ataque.
    static const int rowForFacing[] = {0, 3, 1, 2};
    static const Vector2 ghoulFeet[4][4] = {
        {{155, 254}, {460, 256}, {779, 262}, {1092, 235}},
        {{152, 566}, {457, 572}, {791, 569}, {1144, 574}},
        {{162, 881}, {463, 882}, {784, 886}, {1065, 885}},
        {{155, 1206}, {461, 1206}, {786, 1207}, {1076, 1209}},
    };
    static const Vector2 cultistFeet[4][4] = {
        {{176, 292}, {472, 292}, {780, 292}, {1099, 289}},
        {{168, 602}, {467, 600}, {785, 601}, {1096, 599}},
        {{151, 908}, {461, 906}, {777, 906}, {1092, 908}},
        {{159, 1225}, {470, 1224}, {784, 1222}, {1100, 1226}},
    };
    bool cultist = enemy->type == ENEMY_COMMON_RANGED;
    bool attacking = enemy->state == ENEMY_ATTACK;
    float impact = cultist ? CULTIST_RELEASE_TIME : CLEARING_MELEE_ATTACK_DURATION *
        CLEARING_MELEE_IMPACT_FRAME / CLEARING_MELEE_ATTACK_FRAMES;
    int frame = 0;
    if (enemy->state == ENEMY_WALK) frame = 1 + (int)(enemy->walkTime * 8) % 2;
    else if (attacking && enemy->stateTime >= impact - (cultist ? .18f : 0) &&
             enemy->stateTime < impact + .20f) frame = 3;
    Texture2D texture = cultist ? group->bonelandCultist : group->bonelandGhoul;
    int row = rowForFacing[enemy->facing];
    int left = frame * texture.width / 4;
    int top = row * texture.height / 4;
    int width = (frame + 1) * texture.width / 4 - left;
    int height = (row + 1) * texture.height / 4 - top;
    Vector2 feet = cultist ? cultistFeet[row][frame] : ghoulFeet[row][frame];
    float scale = cultist ? .31f : .34f;
    Vector2 pivot = {(feet.x - left) * scale, (feet.y - top) * scale};
    float fall = enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / ENEMY_DEFEAT_DURATION, 0, 1) : 0;
    Color tint = enemy->state == ENEMY_HURT ? (Color){255, 160, 160, 255} : Fade(WHITE, 1 - fall);
    DrawTexturePro(texture, (Rectangle){left, top, width, height},
        (Rectangle){enemy->position.x, enemy->position.y, width * scale, height * scale},
        pivot, (enemy->facing == 2 ? -1 : 1) * 75 * fall, tint);
    if (cultist && attacking && !enemy->attackConnected) {
        Vector2 origin = EnemyCenter(enemy);
        float charge = Clamp(enemy->stateTime / CULTIST_RELEASE_TIME, 0, 1);
        origin = Vector2Add(origin, Vector2Scale(enemy->attackDirection, 12));
        DrawCircleV(origin, 8 + 7 * charge, Fade(MAROON, .32f + .32f * charge));
        DrawCircleV(origin, 3 + 6 * charge, GOLD);
        for (int i = 0; i < 5; i++) {
            float angle = i * 2 * PI / 5 + charge * 8;
            Vector2 spark = Vector2Add(origin, (Vector2){cosf(angle) * (19 - 9 * charge),
                                                        sinf(angle) * (19 - 9 * charge)});
            DrawRectangleV(spark, (Vector2){3, 3}, Fade(ORANGE, charge));
        }
    }
}

static void DrawClearingEnemy(const EnemyGroup *group, const Enemy *enemy) {
    bool archer = enemy->type == ENEMY_COMMON_RANGED;
    bool special = enemy->type == ENEMY_SPECIAL;
    bool melee = enemy->type == ENEMY_COMMON_MELEE;
    bool attack = enemy->state == ENEMY_ATTACK;
    bool walk = enemy->state == ENEMY_WALK;
    // A fileira esquerda do ataque muda de lado no meio do golpe.
    bool flip = melee && attack && enemy->facing == 2;
    const ClearingEnemyRow *rows;
    if (special) rows = attack ? clearingSpecialAttackRows : walk ? clearingSpecialWalkRows : clearingSpecialIdleRows;
    else if (archer) rows = attack ? clearingArcherAttackRows : walk ? clearingArcherWalkRows : clearingArcherIdleRows;
    else rows = attack ? clearingMeleeAttackRows : walk ? clearingMeleeWalkRows : clearingMeleeIdleRows;
    int facing = melee && attack && enemy->facing == 3 ? 2 : enemy->facing;
    const ClearingEnemyRow *row = &rows[facing];
    int frame = ClearingEnemyFrame(enemy, group->boneland);
    int left = row->edges[frame];
    int right = row->edges[frame + 1];
    float width = right - left;
    float height = row->bottom - row->top;
    Vector2 pivot = {(flip ? right - row->pivots[frame] : row->pivots[frame] - left) * row->scale,
                     (row->ground - row->top) * row->scale};
    Rectangle source = {left, row->top, flip ? -width : width, height};
    Rectangle destination = {enemy->position.x, enemy->position.y, width * row->scale, height * row->scale};
    float fall = enemy->state == ENEMY_FALLEN ? 1 :
                 enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / ENEMY_DEFEAT_DURATION, 0, 1) : 0;
    Color tint = enemy->state == ENEMY_HURT ? (Color){255,160,160,255} :
                 Fade(WHITE, 1 - fall * (special ? .25f : 1.0f));
    const Texture2D *textures = special ? group->clearingSpecial :
                                archer ? group->clearingArcher : group->clearingMelee;
    DrawTexturePro(textures[attack ? 2 : walk ? 1 : 0], source, destination, pivot,
                   (enemy->facing == 2 ? -1 : 1) * 75.0f * fall, tint);
}

static const float templarSizes[] = {
    [TEMPLAR_SHIELD] = 124, [TEMPLAR_EXECUTIONER] = 146, [TEMPLAR_CROSSBOW] = 116,
    [TEMPLAR_STANDARD] = 159, [TEMPLAR_MARSHAL] = 190,
};
static const int templarRows[] = {0, 3, 1, 2};

static void DrawTemplarDust(Vector2 center, Vector2 direction, float progress, float scale) {
    if (progress < 0 || progress >= 1) return;
    Vector2 normal = {-direction.y, direction.x};
    for (int i = 0; i < 9; i++) {
        float side = (float)(i - 4) / 4;
        Vector2 p = Vector2Add(center, Vector2Scale(normal, side * scale * (11 + 29 * progress)));
        p = Vector2Subtract(p, Vector2Scale(direction, scale * (5 + (20 + 4 * (i % 3)) * progress)));
        p.y -= sinf(PI * progress) * (i % 3) * 8 * scale;
        float radius = (3.0f + i % 3 + 4 * progress) * scale;
        DrawEllipse((int)p.x, (int)p.y, radius * 1.45f, radius,
                    Fade(i % 2 ? (Color){215, 166, 119, 255} : (Color){137, 84, 66, 255},
                         .52f * (1 - progress)));
    }
}

static void DrawTemplarEffect(Texture2D texture, int row, Vector2 center,
                              Vector2 direction, float age, float size) {
    if (!texture.id || age < 0 || age >= 1) return;
    float tileW = texture.width / 6.0f, tileH = texture.height / 4.0f;
    float angle = atan2f(direction.y, direction.x) * RAD2DEG;
    DrawTexturePro(texture, (Rectangle){(int)(age * 6) * tileW, row * tileH, tileW, tileH},
                   (Rectangle){center.x, center.y, size, size}, (Vector2){size * .5f, size * .5f},
                   angle, Fade(WHITE, 1 - age * .45f));
}

static void DrawTemplarBlood(const EnemyGroup *group) {
    const EnemyImpact *blood = &group->templarBlood;
    if (blood->remaining <= 0) return;
    float time = 1 - blood->remaining / .36f, alpha = 1 - time;
    for (int i = 0; i < 13; i++) {
        float a = i * 2.39996f;
        float reach = (8 + 35 * time) * (.58f + .15f * (i % 4));
        Vector2 p = {blood->position.x + cosf(a) * reach,
                     blood->position.y + sinf(a) * reach * .65f - 12 * time + 34 * time * time};
        DrawRectangleV(p, (Vector2){(i % 3 + 3) * alpha, (i % 3 + 2) * alpha},
                       Fade(i % 3 == 0 ? (Color){232, 42, 52, 255} : (Color){138, 9, 35, 255}, alpha));
    }
}

static void DrawMarshalAttackPose(const EnemyGroup *group, const Enemy *enemy, Color tint) {
    static const int clockwise[] = {0, 2, 1, 3};
    static const Vector2 anchors[] = {{128, 244}, {130, 242}, {112, 242}, {150, 240}};
    int skill = enemy->bossAttack;
    int facing = enemy->facing;
    bool slam = skill == MARSHAL_RUPTURE || skill == MARSHAL_SHOCKWAVE;
    Texture2D texture = skill == MARSHAL_CHARGE ? group->marshalCharge :
                        slam ? group->marshalSlam : group->templarAttack[TEMPLAR_MARSHAL];
    float time = enemy->stateTime;
    float impact = marshalSkills[skill].impact;
    float activeEnd = marshalSkills[skill].activeEnd;
    float recovery = marshalSkills[skill].duration - activeEnd;
    int frame;
    if (time < impact) {
        frame = (int)Clamp(3 * time / impact, 0, 2);
    } else if (time < activeEnd) {
        float progress = Clamp((time - impact) / (activeEnd - impact), 0, .999f);
        frame = progress < .55f ? 3 : 4;
        if (skill == MARSHAL_SPIN) {
            int turn = (int)(4 * progress);
            facing = clockwise[(clockwise[facing] + turn) % 4];
        }
    } else {
        frame = time < activeEnd + .36f * recovery ? 4 : 5;
    }
    // Use the fully contained overhead pose with either impact animation.
    if (slam && frame == 2) texture = group->templarAttack[TEMPLAR_MARSHAL];
    const float tile = 256;
    float size = templarSizes[TEMPLAR_MARSHAL];
    Vector2 anchor = Vector2Scale(anchors[facing], size / tile);
    DrawTexturePro(texture, (Rectangle){frame * tile, templarRows[facing] * tile, tile, tile},
                   (Rectangle){enemy->position.x, enemy->position.y, size, size}, anchor, 0, tint);
}

static void DrawTemplarAttackPose(const EnemyGroup *group, const Enemy *enemy, Vector2 feet,
                                 int facing, int frame, float tilt, Color tint) {
    Texture2D texture = group->templarAttack[enemy->templarRole];
    float tileW = texture.width / 4.0f, tileH = texture.height / 4.0f;
    float size = templarSizes[enemy->templarRole] * (enemy->templarRole == TEMPLAR_EXECUTIONER ? 1.08f : 1);
    Rectangle source = {frame * tileW, templarRows[facing] * tileH, tileW, tileH};
    DrawTexturePro(texture, source, (Rectangle){feet.x, feet.y, size, size},
                   (Vector2){size * .5f, size * .93f}, tilt, tint);
}

static void DrawTemplarSprite(const EnemyGroup *group, const Enemy *enemy,
                              Vector2 position, int facing, int frame, float tilt, Color tint) {
    // A parte mais larga da arma ou bandeira não coincide com a posição dos pés.
    static const float anchors[][4] = {
        [TEMPLAR_SHIELD] = {64, 64, 57, 65},
        [TEMPLAR_EXECUTIONER] = {66, 63, 73, 56},
        [TEMPLAR_CROSSBOW] = {63, 64, 61, 69},
        [TEMPLAR_STANDARD] = {63, 63, 62, 66},
        [TEMPLAR_MARSHAL] = {66, 64, 59, 80},
    };
    float size = templarSizes[enemy->templarRole];
    DrawTexturePro(group->templarWalk[enemy->templarRole],
        (Rectangle){frame * 128, templarRows[facing] * 128, 128, 128},
        (Rectangle){position.x, position.y, size, size},
        (Vector2){anchors[enemy->templarRole][facing] * size / 128, size * .96f}, tilt, tint);
}

static void DrawTemplar(const EnemyGroup *group, const Enemy *enemy) {
    TemplarRole role = enemy->templarRole;
    if (enemy->state == ENEMY_SPAWNING) {
        if (enemy->stateTime < 0) return;
        if (role == TEMPLAR_STANDARD && enemy->stateTime >= templarArrivalTime) {
            float plant = (enemy->stateTime - templarArrivalTime) / templarPlantTime;
            int frame = plant < .38f ? 0 : plant < .68f ? 1 : 2;
            DrawTemplarEffect(group->templarDust, 2, enemy->position, (Vector2){1,0},
                              (plant - .38f) / .62f, 110);
            DrawTemplarAttackPose(group, enemy, enemy->position, 0, frame, 0, WHITE);
            DrawTemplarEffect(group->templarEffects, 3, enemy->position, (Vector2){1,0}, (plant - .38f) / .62f, 205);
            return;
        }
        float t = Clamp(enemy->stateTime / templarArrivalTime, 0, 1);
        float descent = role == TEMPLAR_MARSHAL ? .76f : .52f;
        Vector2 landing = {835 + Clamp((enemy->position.x - 835) * .26f, -83, 83), 284};
        Vector2 origin = {landing.x, 70};
        Vector2 position = t < descent ? Vector2Lerp(origin, landing, t / descent) :
            Vector2Lerp(landing, enemy->position, (t - descent) / (1 - descent));
        int facing = t < descent ? 0 : FacingToward(Vector2Subtract(enemy->position, landing));
        int frame = (int)(enemy->stateTime * 10) % 6;
        if (role == TEMPLAR_MARSHAL) {
            Enemy escort = {.templarRole = TEMPLAR_SHIELD};
            float exit = Clamp((t - .53f) / .47f, 0, 1);
            for (int side = -1; side <= 1; side += 2) {
                Vector2 feet = {position.x + side * (57 + 130 * exit), position.y + 22 + 42 * exit};
                DrawTemplarSprite(group, &escort, feet, exit > 0 ? (side < 0 ? 2 : 3) : 0,
                                 frame, 0, Fade(WHITE, 1 - exit));
            }
            facing = 0;
            if (t > .83f) {
                frame = 0;
                float trail = Clamp((t - .83f) / .17f, 0, 1);
                DrawLineEx((Vector2){position.x - 44, position.y - 4},
                    (Vector2){position.x - 44 + 29 * trail, position.y - 7}, 3, Fade(ORANGE, 1 - trail));
            }
        }
        DrawTemplarSprite(group, enemy, position, facing, frame, 0, WHITE);
        return;
    }
    float fall = enemy->state == ENEMY_FALLEN ? 1.0f :
                 enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / 1.2f, 0, 1) : 0;
    int frame = enemy->state == ENEMY_WALK ? (int)(enemy->walkTime * 9) % 6 : 0;
    float tilt = (enemy->facing == 2 ? -1.0f : 1.0f) * 72 * fall;
    Color tint = enemy->state == ENEMY_HURT ? (Color){255, 173, 173, 255} :
                 role == TEMPLAR_MARSHAL && fall > 0 ? Fade((Color){215, 185, 195, 255}, .80f) :
                 Fade(WHITE, 1 - fall);
    if (role == TEMPLAR_MARSHAL && enemy->state == ENEMY_FALLEN)
        DrawBossGroundSeal(group->sealMarker, enemy->position);
    if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
        float pulse = .55f + .25f * sinf(17 * enemy->stateTime);
        DrawCircleV(enemy->position, 47, Fade(MAROON, pulse));
        tint = (Color){255, 174, 226, 255};
    }
    if (role == TEMPLAR_STANDARD && enemy->state != ENEMY_DIE) {
        int sway = (int)(enemy->stateTime * 3.5f) % 2;
        DrawTemplarAttackPose(group, enemy, enemy->position, enemy->facing, 2 + sway, 0, tint);
        return;
    }
    if (enemy->state == ENEMY_ATTACK) {
        float t = enemy->stateTime;
        bool marshal = role == TEMPLAR_MARSHAL;
        int skill = marshal ? enemy->bossAttack : 0;
        bool charge = marshal && skill == MARSHAL_CHARGE;
        float impact = marshal ? marshalSkills[skill].impact : role == TEMPLAR_EXECUTIONER ?
            executionerImpact : role == TEMPLAR_CROSSBOW ? crossbowRelease : shieldImpact;
        float activeEnd = marshal ? marshalSkills[skill].activeEnd : impact + .17f;
        int pose = t < impact * .46f ? 0 : t < impact ? 1 : t < activeEnd ? 2 : 3;
        Vector2 feet = enemy->position;
        float recoil = t >= impact && t < activeEnd ? sinf(PI * (t - impact) / (activeEnd - impact)) : 0;
        if (role == TEMPLAR_CROSSBOW) {
            feet = Vector2Subtract(feet, Vector2Scale(enemy->attackDirection, recoil * 8));
            if (recoil > 0)
                DrawEmberBurst(Vector2Add(EnemyCenter(enemy), Vector2Scale(enemy->attackDirection, 25)),
                               (t - impact) / (activeEnd - impact));
        } else {
            bool ring = marshal && (skill == MARSHAL_SHOCKWAVE || skill == MARSHAL_RUPTURE);
            if (charge) {
                float elapsed = t - impact;
                for (int puff = 0; puff < 4; puff++) {
                    float delay = (float)puff * .12f;
                    Vector2 origin = Vector2Subtract(feet, Vector2Scale(enemy->attackDirection, puff * 26));
                    DrawTemplarEffect(group->templarDust, 1, origin, enemy->attackDirection,
                                      (elapsed - delay) / .42f, 126);
                }
                DrawTemplarEffect(group->templarDust, 2, feet, (Vector2){1,0},
                                  (t - activeEnd) / .38f, 124);
            } else {
                float size = ring ? 265 : marshal ? 151 : 98;
                DrawTemplarEffect(group->templarDust, ring ? 3 : 2, feet,
                                  (Vector2){1,0}, (t - impact) / .43f, size);
            }
        }
        float lean = enemy->facing == 2 ? -recoil : recoil;
        if (marshal)
            DrawMarshalAttackPose(group, enemy, tint);
        else
            DrawTemplarAttackPose(group, enemy, feet, enemy->facing, pose, 4 * lean, tint);
        int effect = role == TEMPLAR_SHIELD ? 0 : role == TEMPLAR_CROSSBOW ? 2 :
            marshal && (skill == MARSHAL_SPIN || skill == MARSHAL_SHOCKWAVE) ? 3 : 1;
        Vector2 origin = Vector2Add((Vector2){feet.x, feet.y - 24},
                                    Vector2Scale(enemy->attackDirection, effect == 3 ? 0 : 35));
        DrawTemplarEffect(group->templarEffects, effect, origin, enemy->attackDirection, (t - impact) / .32f,
                          marshal ? 190 : role == TEMPLAR_EXECUTIONER ? 142 : 91);
        return;
    }
    if (enemy->state == ENEMY_WALK) {
        float step = fmodf(enemy->walkTime * 3.6f, 1);
        static const Vector2 facingDirection[] = {{0,1},{0,-1},{-1,0},{1,0}};
        Vector2 direction = facingDirection[enemy->facing];
        Vector2 heel = Vector2Subtract(enemy->position, Vector2Scale(direction, 10));
        DrawTemplarEffect(group->templarDust, 0, heel, direction, step,
                          role == TEMPLAR_MARSHAL ? 76 : 52);
    }
    if (role == TEMPLAR_MARSHAL && fall > 0)
        DrawEllipse((int)enemy->position.x, (int)enemy->position.y - 8, 48.0f, 16.0f, Fade(BLACK, .38f));
    DrawTemplarSprite(group, enemy, enemy->position, enemy->facing, frame, tilt, tint);
    if (role == TEMPLAR_MARSHAL && enemy->state == ENEMY_FALLEN)
        DrawGameText("E: selar", (int)enemy->position.x - 30, (int)enemy->position.y + 12, 18, GOLD);
}

void DrawBossGroundSeal(Texture2D sealMarker, Vector2 position) {
    float width = 186.0f;
    float height = 148.0f;
    Vector2 center = {position.x, position.y - 16.0f};
    DrawTexturePro(sealMarker, (Rectangle){128, 36, 1056, 1064},
        (Rectangle){center.x - width * 0.5f, center.y - height * 0.5f, width, height},
        (Vector2){0}, 0, Fade(WHITE, 0.52f));
}

static void DrawSealMarkers(const EnemyGroup *group) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->state != ENEMY_FALLEN || enemy->type == ENEMY_BOSS) continue;
        float size = 42.0f;
        float height = 55.0f;
        DrawTexturePro(group->sealMarker, (Rectangle){128,36,1056,1064},
            (Rectangle){enemy->position.x-size*.5f, enemy->position.y-height-size*.5f, size, size},
            (Vector2){0},0,WHITE);
    }
}

static void DrawTemplarWarnings(const EnemyGroup *group) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->templarRole == TEMPLAR_NONE || !EnemyCanBeHit(enemy)) continue;
        if (enemy->templarRole == TEMPLAR_STANDARD) {
            DrawCircleV(enemy->position, 220, Fade(GOLD, .08f));
            DrawCircleLines((int)enemy->position.x, (int)enemy->position.y, 220, Fade(GOLD, .5f));
        } else if (enemy->templarRole == TEMPLAR_CROSSBOW &&
                   enemy->state == ENEMY_ATTACK && !enemy->attackConnected) {
            Vector2 origin = EnemyCenter(enemy);
            Vector2 end = Vector2Add(origin, Vector2Scale(enemy->attackDirection, 300));
            DrawLineEx(origin, end, 3, Fade(GOLD, .65f));
        } else if (enemy->state == ENEMY_ATTACK) {
            bool shield = enemy->templarRole == TEMPLAR_SHIELD;
            bool marshal = enemy->templarRole == TEMPLAR_MARSHAL;
            int skill = marshal ? enemy->bossAttack : 0;
            bool charge = marshal && skill == MARSHAL_CHARGE;
            bool rupture = marshal && skill == MARSHAL_RUPTURE;
            float t = enemy->stateTime;
            float heading = atan2f(enemy->attackDirection.y, enemy->attackDirection.x) * RAD2DEG;
            float windup = shield ? shieldImpact : marshal ? marshalSkills[skill].impact : executionerImpact;
            if (enemy->templarRole == TEMPLAR_CROSSBOW) continue;
            if (marshal && skill == MARSHAL_SHOCKWAVE) {
                float radius = marshalSkills[skill].reach;
                if (t < windup) {
                    DrawCircleV(enemy->position, radius, Fade(MAROON, .12f + .19f * t / windup));
                    DrawCircleLinesV(enemy->position, radius, Fade(ORANGE, .65f));
                } else if (t < marshalSkills[skill].activeEnd) {
                    float age = (t - windup) / (marshalSkills[skill].activeEnd - windup);
                    float edge = radius * age;
                    DrawRing(enemy->position, fmaxf(0, edge - 18), edge + 6, 0, 360, 64, Fade(MAROON, .75f));
                    DrawRing(enemy->position, edge, edge + 5, 0, 360, 64, Fade(RAYWHITE, 1 - .55f * age));
                }
                continue;
            }
            if (t < windup && (charge || enemy->templarRole == TEMPLAR_EXECUTIONER)) {
                Vector2 end = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, charge ? 280 : 120));
                DrawLineEx(enemy->position, end, 4, Fade(ORANGE, .68f));
            }
            if (charge) {
                if (t >= windup && t < marshalSkills[skill].activeEnd) {
                    Vector2 tail = Vector2Subtract(enemy->position, Vector2Scale(enemy->attackDirection, 130));
                    Vector2 normal = {-enemy->attackDirection.y, enemy->attackDirection.x};
                    for (int side = -1; side <= 1; side += 2) {
                        Vector2 offset = Vector2Scale(normal, side * (22 + 7 * sinf(t * 35)));
                        DrawLineEx(Vector2Add(tail, offset), Vector2Add(enemy->position, offset),
                                   3, Fade(RAYWHITE, .46f));
                    }
                }
                continue;
            }
            float reach = shield ? ENEMY_ATTACK_RANGE + 8 : marshal ? marshalSkills[skill].reach : 106;
            float spread = shield ? 90 : marshal ? acosf(marshalSkills[skill].arcDot) * RAD2DEG : 99;
            if (t < windup)
                DrawCircleSector(enemy->position, reach, heading - spread, heading + spread, 24,
                                 Fade(ORANGE, .12f + .22f * t / windup));
            else if (t < windup + (rupture ? .36f : .20f)) {
                float age = (t - windup) / (rupture ? .36f : .20f);
                float radius = reach * (.54f + .46f * age);
                float sweep = heading - spread + 2 * spread * age;
                bool returning = marshal && skill == MARSHAL_RETURN;
                if (returning) sweep = heading + spread - 2 * spread * age;
                float start = rupture ? heading - spread : fmaxf(heading - spread, sweep - spread * .64f);
                float end = rupture ? heading + spread : fminf(heading + spread, sweep + spread * .64f);
                DrawRing(enemy->position, radius * .72f, radius, start, end, 24, Fade(ORANGE, .68f * (1 - age)));
                DrawRing(enemy->position, radius - 4, radius, start, end, 24, Fade(RAYWHITE, .86f * (1 - age)));
                if (rupture) {
                    Vector2 center = Vector2Add(enemy->position, Vector2Scale(enemy->attackDirection, 72));
                    DrawTemplarDust(center, enemy->attackDirection, age, 2.3f);
                    DrawEmberBurst(center, age);
                }
            }
        }
    }
}

static void DrawHeartShape(Vector2 center, float scale, Color color) {
    float r = 4.6f * scale;
    Vector2 leftLobe = {center.x - 3.6f * scale, center.y - 2.2f * scale};
    Vector2 rightLobe = {center.x + 3.6f * scale, center.y - 2.2f * scale};
    Vector2 bottomTip = {center.x, center.y + 7.2f * scale};
    Vector2 leftSide = {center.x - 7.8f * scale, center.y - 0.5f * scale};
    Vector2 rightSide = {center.x + 7.8f * scale, center.y - 0.5f * scale};
    Vector2 mid = {center.x, center.y - 2.2f * scale};
    DrawCircleV(leftLobe, r, color);
    DrawCircleV(rightLobe, r, color);
    /* Em raylib (sentido anti-horário com Y para baixo): */
    DrawTriangle(leftSide, bottomTip, rightSide, color);
    DrawTriangle(leftSide, rightSide, mid, color);
}

void DrawEnemyHealthBar(const Enemy *enemy, float top) {
    if (!EnemyCanBeHit(enemy) || enemy->health <= 0 || enemy->maxHealth <= 0) return;
    int x = (int)enemy->position.x - 24;
    int y = (int)top;
    DrawRectangle(x, y, 48, 5, DARKGRAY);
    DrawRectangle(x, y, 48 * enemy->health / enemy->maxHealth, 5, RED);
}

void DrawHealthPickup(Vector2 position) {
    Vector2 drop = {position.x, position.y - 9.0f};
    DrawHeartShape(drop, 1.25f, Fade(GOLD, 0.65f));
    DrawHeartShape(drop, 0.90f, RED);
}

static void DrawBoneLordWarning(const Enemy *boss) {
    if (boss->type != ENEMY_BOSS || boss->state != ENEMY_ATTACK) return;
    int attack = boss->bossAttack;
    float remaining = boneAttacks[attack].windup - boss->stateTime;
    if (remaining <= 0) return;

    float strength = .35f + .40f * (1 - remaining / boneAttacks[attack].windup);
    if (attack == BONE_CHARGE) {
        Vector2 end = Vector2Add(boss->position, Vector2Scale(boss->attackDirection, 336));
        DrawLineEx(boss->position, end, 126, Fade(ORANGE, strength * .29f));
        DrawLineEx(boss->position, end, 4, Fade(GOLD, strength));
    } else if (attack == BONE_CLAW) {
        float facing = atan2f(boss->attackDirection.y, boss->attackDirection.x) * RAD2DEG;
        float angle = acosf(.45f) * RAD2DEG;
        DrawCircleSector(boss->position, 115, facing - angle, facing + angle, 28, Fade(ORANGE, strength * .38f));
        DrawCircleSectorLines(boss->position, 115, facing - angle, facing + angle, 28, Fade(GOLD, strength));
    } else {
        float range = boneAttacks[attack].range;
        DrawCircleV(boss->position, range, Fade(RED, strength * .19f));
        DrawRing(boss->position, range - 4, range, 0, 360, 64, Fade(ORANGE, strength));
    }
}

static void DrawBoneLord(const EnemyGroup *group, const Enemy *boss) {
    static const int rowForFacing[] = {0, 3, 1, 2};
    static const int walkGround[] = {234, 482, 729, 991};
    static const int pounceRows[] = {0, 284, 538, 781, 1122};
    static const int pounceColumns[][6] = {
        {0, 275, 553, 846, 1135, 1402},
        {0, 270, 529, 843, 1130, 1402},
        {0, 279, 530, 843, 1134, 1402},
        {0, 280, 557, 834, 1135, 1402},
    };
    static const int pounceGround[] = {249, 502, 746, 1080};
    static const int clawRows[] = {0, 282, 544, 814, 1122};
    static const int clawGround[] = {249, 507, 785, 1088};
    static const int altRows[] = {0, 279, 553, 825, 1122};
    static const int dashGround[] = {270, 525, 797, 1108};
    static const int biteGround[] = {246, 507, 785, 1092};

    int row = rowForFacing[boss->facing];
    Texture2D texture = group->boneLordWalk;
    int frame = boss->state == ENEMY_WALK ? (int)(boss->walkTime * 9) % 6 : 0;
    int left = frame * 256, top = row * 256, right = left + 256, bottom = top + 256;
    int ground = walkGround[row];
    float scale = .62f;
    float lift = 0;
    if (boss->state == ENEMY_ATTACK) {
        int attack = boss->bossAttack;
        float t = boss->stateTime;
        float windup = boneAttacks[attack].windup;
        float impactEnd = boneAttacks[attack].impactEnd;
        float duration = boneAttacks[attack].duration;
        frame = t < windup ? (t < windup * .43f ? 0 : 1)
            : t < impactEnd ? 2 : t < (impactEnd + duration) * .5f ? 3 : 4;
        bool enraged = GuardianEnraged(boss);
        if (attack == BONE_DASH || (attack == BONE_CLAW && enraged)) {
            bool dash = attack == BONE_DASH;
            texture = dash ? group->boneLordDash : group->boneLordBite;
            left = frame * texture.width / 5;
            right = (frame + 1) * texture.width / 5;
            top = altRows[row];
            bottom = altRows[row + 1];
            ground = dash ? dashGround[row] : biteGround[row];
            if (dash && frame == 2)
                lift = (enraged ? 39 : 24) * sinf(PI * (t - windup) / (impactEnd - windup));
        } else if (attack == BONE_CLAW) {
            texture = group->boneLordClaw;
            left = frame * texture.width / 5;
            right = (frame + 1) * texture.width / 5;
            top = clawRows[row];
            bottom = clawRows[row + 1];
            ground = clawGround[row];
        } else {
            texture = group->boneLordPounce;
            left = pounceColumns[row][frame];
            right = pounceColumns[row][frame + 1];
            top = pounceRows[row];
            bottom = pounceRows[row + 1];
            ground = pounceGround[row];
            if (frame == 2) lift = (attack == BONE_DASH ? 18 : 10) * sinf(PI * (t - windup) / (impactEnd - windup));
        }
    }

    float collapse = boss->state == ENEMY_FALLEN ? 1 :
        boss->state == ENEMY_DIE ? Clamp(boss->stateTime / ENEMY_DEFEAT_DURATION, 0, 1) : 0;
    float reveal = boss->state == ENEMY_SPAWNING ? Clamp(boss->stateTime, 0, 1) : 1;
    float width = (right - left) * scale;
    float heightScale = scale * (1 - .48f * collapse);
    float height = (bottom - top) * heightScale;
    Color tint = Fade(collapse > 0 ? (Color){211, 171, 163, 255} : WHITE, reveal);
    if (boss->state == ENEMY_FALLEN) DrawBossGroundSeal(group->sealMarker, boss->position);
    DrawEllipse((int)boss->position.x, (int)boss->position.y - 9, 54, 19, Fade(BLACK, .38f * reveal));
    if (boss->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
        float flare = Clamp(boss->stateTime / GUARDIAN_PHASE_TRANSITION_TIME, 0, 1);
        DrawRing(boss->position, 45 + 72 * flare, 50 + 72 * flare, 0, 360, 36,
                 Fade(ORANGE, 1 - flare));
        DrawEmberBurst(boss->position, flare);
        tint = ColorLerp((Color){255, 165, 120, 255}, WHITE, flare);
    } else if (GuardianEnraged(boss) && boss->state == ENEMY_ATTACK) {
        int attack = boss->bossAttack;
        float impact = boss->stateTime - (attack == BONE_DASH ? boneAttacks[attack].impactEnd :
                                                                   boneAttacks[attack].windup);
        if (impact >= 0 && impact < .29f) {
            Vector2 contact = Vector2Add(boss->position, Vector2Scale(boss->attackDirection, 54));
            DrawEmberBurst(contact, impact / .29f);
        }
    }
    DrawTexturePro(texture, (Rectangle){left, top, right - left, bottom - top},
        (Rectangle){boss->position.x, boss->position.y + 28 * (1 - reveal) - lift, width, height},
        (Vector2){width * .5f, (ground - top) * heightScale},
        (boss->facing == 2 ? -1 : 1) * 14 * collapse, tint);
    if (boss->state == ENEMY_FALLEN)
        DrawGameText("E: selar", (int)boss->position.x - 30, (int)boss->position.y + 12, 18, GOLD);
}

void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    if (group->list[0].necromancerForm) {
        DrawNecromancer(group,player,foreground);
        return;
    }
    if (group->village) {
        VillageDrawEnemies(group, player, foreground);
        return;
    }
    if (group->cemetery) {
        CemeteryDrawEnemies(group,player,foreground);
        if (foreground) {
            DrawRangedProjectiles(group);
            DrawSealMarkers(group);
        }
        return;
    }
    if (!foreground) {
        DrawTemplarWarnings(group);
        if (group->boneland) DrawBoneLordWarning(&group->list[0]);
    }
    if (!foreground) for (int i=0; i<ENEMY_ROOT_COUNT; i++) {
        const EnemyRoot *root = &group->rootAttacks[i];
        if (!root->active) continue;
        const GuardianHazard *hazard = &guardianHazards[root->attack];
        if (root->attack == GUARDIAN_CURSE) {
            DrawGuardianCurse(group->curse, root, hazard);
        } else if (root->age < hazard->warning) {
            DrawCircleV(root->position, hazard->radius, Fade(RED, .18f));
            DrawCircleLines((int)root->position.x, (int)root->position.y, hazard->radius, hazard->warningColor);
        } else if (root->attack == GUARDIAN_ROOT_WAVE) {
            DrawGuardianWaveFrame(group->rootWave, root->position, root->age - hazard->warning, true,
                                  group->list[0].attackDirection.x < 0);
        } else {
            int frame = (int)((root->age - hazard->warning)*10);
            if (frame > 7) frame = 7;
            DrawTexturePro(group->roots, (Rectangle){frame*288.0f,0,288,256},
                (Rectangle){root->position.x-90,root->position.y-145,180,160}, (Vector2){0},0,WHITE);
        }
    }
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if ((enemy->position.y >= player.y) != foreground) continue;
        if (enemy->state == ENEMY_GONE && enemy->pickupAvailable) {
            DrawHealthPickup(enemy->position);
        }
        if (enemy->state == ENEMY_GONE) continue;
        if (group->boneland && enemy->type == ENEMY_BOSS) {
            DrawBoneLord(group, enemy);
            continue;
        }
        if (enemy->templarRole != TEMPLAR_NONE) {
            DrawTemplar(group, enemy);
            DrawEnemyHealthBar(enemy, enemy->position.y - templarSizes[enemy->templarRole] *
                               (enemy->type == ENEMY_BOSS ? 1.13f : 1.02f));
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            if (IsGuardianSpawn(group, enemy)) DrawGuardianSpawn(group->guardianSpawn, enemy);
            else if (enemy->stateTime >= 0) DrawCircleLines((int)enemy->position.x,
                (int)enemy->position.y, 24 + 10*sinf(enemy->stateTime*8), GOLD);
            continue;
        }
        if (enemy->type == ENEMY_BOSS) {
            if (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN) {
                if (enemy->state == ENEMY_FALLEN) DrawBossGroundSeal(group->sealMarker, enemy->position);
                DrawGuardianDefeat(group->guardianDefeat, enemy);
                continue;
            }
            if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
                float progress = fminf(enemy->stateTime / GUARDIAN_PHASE_TRANSITION_TIME, 1);
                DrawCircleV(enemy->position, 50 + 120 * progress, Fade(PURPLE, .35f * (1 - progress)));
                DrawCircleLines((int)enemy->position.x, (int)enemy->position.y,
                                50 + 120 * progress, Fade(VIOLET, 1 - progress));
            } else if (GuardianEnraged(enemy) && enemy->health > 0) {
                DrawCircleV(enemy->position, 43, Fade(PURPLE, .26f));
            }
            bool melee = enemy->state == ENEMY_ATTACK && enemy->guardianMelee;
            bool castingWave = enemy->state == ENEMY_ATTACK && !melee && enemy->bossAttack == GUARDIAN_ROOT_WAVE;
            bool attack = enemy->state == ENEMY_ATTACK && !melee && enemy->bossAttack == GUARDIAN_ROOTS;
            if (castingWave)
                DrawGuardianWaveFrame(group->rootWave, enemy->position, enemy->stateTime, false,
                                      enemy->attackDirection.x < 0);
            bool widePose = attack || melee;
            float w = widePose ? 384 : 256, h = widePose ? 288 : 272;
            Texture2D texture = melee ? group->guardianMelee : attack ? group->bossAttack :
                    enemy->state == ENEMY_WALK ? group->bossWalk : group->bossSprites;
            DrawTexturePro(texture,
                (Rectangle){EnemyAnimationFrame(enemy)*w, enemy->facing*h,w,h},
                (Rectangle){enemy->position.x-w*.5f*GUARDIAN_SPRITE_SCALE,
                            enemy->position.y-(widePose?280:264)*GUARDIAN_SPRITE_SCALE,
                            w*GUARDIAN_SPRITE_SCALE,h*GUARDIAN_SPRITE_SCALE}, (Vector2){0},0,
                WHITE);
            continue;
        }
        int row = enemy->state == ENEMY_WALK ? 1 : enemy->state == ENEMY_ATTACK ? 2 :
                  enemy->state == ENEMY_HURT ? 3 : enemy->state == ENEMY_DIE ? 4 : 0;
        int frame = EnemyAnimationFrame(enemy);
        if (enemy->state == ENEMY_FALLEN) { row=0; frame=0; }
        Texture2D tex = group->sprites;
        if (group->boneland && (enemy->type == ENEMY_COMMON_MELEE || enemy->type == ENEMY_COMMON_RANGED)) {
            DrawBonelandEnemy(group, enemy);
        } else if (enemy->type == ENEMY_COMMON_MELEE || enemy->type == ENEMY_COMMON_RANGED ||
            enemy->type == ENEMY_SPECIAL) {
            DrawClearingEnemy(group, enemy);
        } else {
        if (enemy->facing < 2 && enemy->state != ENEMY_DIE) {
            tex = enemy->facing == 1 ? group->backSprites : group->frontSprites;
            if (enemy->state == ENEMY_ATTACK) {
                row = 1; /* Sem arte de ataque frontal/traseiro: movimento seguro. */
                frame = (int)(enemy->stateTime*6.5f);
                if (frame > 3) frame = 3;
            } else if (enemy->state == ENEMY_HURT) {
                row = 0;
                frame = 0;
            }
        }
        Rectangle src = {frame*80.0f, row*80.0f,
                         enemy->facing==2 && tex.id==group->sprites.id ? -80.0f : 80.0f, 80.0f};
        Color tint = enemy->state == ENEMY_HURT ? (Color){255,160,160,255} :
                     enemy->type == ENEMY_SPECIAL ? (Color){215,160,255,255} : WHITE;
        DrawTexturePro(tex, src,
            (Rectangle){enemy->position.x-38, enemy->position.y-75, 76, 76},
            (Vector2){0},0,tint);
        }
        if (enemy->state == ENEMY_FALLEN) DrawGameText("E: selar",(int)enemy->position.x-30,(int)enemy->position.y+10,16,GOLD);
        if (enemy->type == ENEMY_SPECIAL && enemy->state == ENEMY_ATTACK && enemy->stateTime < SPECIAL_CHARGE_START)
            DrawCircleLines((int)enemy->position.x,(int)enemy->position.y,55,ORANGE);
        if (EnemyCanBeHit(enemy) && enemy->health < enemy->maxHealth) {
            int healthY = (int)enemy->position.y - (enemy->type == ENEMY_SPECIAL ? 106 : 85);
            DrawRectangle((int)enemy->position.x-24,healthY,48,5,DARKGRAY);
            DrawRectangle((int)enemy->position.x-24,healthY,
                          48*enemy->health/enemy->maxHealth,5,RED);
        }
    }
    if (foreground) {
        DrawRangedProjectiles(group);
        DrawSealMarkers(group);
        DrawTemplarBlood(group);
    }
}

void DrawEnemyDebug(const EnemyGroup *group) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE) continue;
        Vector2 center = EnemyCenter(enemy);
        DrawCircleLines((int)center.x,(int)center.y,EnemyHurtRadius(enemy),GREEN);
        DrawCircleV(enemy->position,3.0f,YELLOW);
        int frame = IsGuardianSpawn(group, enemy) ? GuardianSpawnFrame(enemy) : EnemyAnimationFrame(enemy);
        if (!group->cemetery && (enemy->type == ENEMY_COMMON_MELEE || enemy->type == ENEMY_COMMON_RANGED ||
                                enemy->type == ENEMY_SPECIAL))
            frame = ClearingEnemyFrame(enemy, group->boneland);
        if (!group->cemetery && enemy->type == ENEMY_BOSS &&
            (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN))
            frame = GuardianDefeatFrame(enemy);
        DrawGameText(TextFormat("E%d F%d",enemy->state,frame),
                 (int)enemy->position.x-28,(int)enemy->position.y-97,12,GOLD);
    }
}
