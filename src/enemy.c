#include "game_text.h"
#include "enemy.h"
#include "run_progress.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define ENEMY_SPEED 98.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12
/* Base das ondas da Clareira e referência para o Cemitério. */
#define CLEARING_FIRST_WAVE_ENEMIES 10
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
#define CEMETERY_BOSS_HEALTH_BONUS 400
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
#define GUARDIAN_WAVE_WARNING .65f
#define GUARDIAN_WAVE_RADIUS 60.0f
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
        110.0f, GUARDIAN_CURSE_DAMAGE, VIOLET},
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

static const float templarArrivalTime = 2.05f;
static const float templarPlantTime = .39f;
static const float templarPlantImpact = .18f;
static const float shieldImpact = .42f;
static const float executionerImpact = .76f;
static const float crossbowRelease = .52f;
static const float crossbowAttackDuration = .99f;
static const float shieldAttackDuration = .73f;
static const float executionerAttackDuration = 1.25f;
static const struct {
    float impact, duration, reach, arcDot;
    int damage, art;
} igrisSkills[IGRIS_SKILL_COUNT + 1] = {
    [IGRIS_MELEE]    = {.42f, .86f, 140, .34f, 20, IGRIS_MELEE_ART},
    [IGRIS_ECLIPSE]  = {.62f, 1.19f, 225, .50f, 28, IGRIS_ECLIPSE_ART},
    [IGRIS_ERUPTION] = {.79f, 1.64f, 120, -1, 33, IGRIS_ERUPTION_ART},
    [IGRIS_SPEARS]   = {.59f, 1.26f, 520, -1, 25, IGRIS_SPEARS_ART},
    [IGRIS_VORTEX]   = {.68f, 1.41f, 182, -1, 32, IGRIS_VORTEX_ART},
    [IGRIS_SKILL_COUNT] = {.37f, .79f, 178, .27f, 36, IGRIS_FINAL_MELEE},
};
#define IGRIS_FINAL_CLAW IGRIS_SKILL_COUNT
#define IGRIS_Z_RIP (IGRIS_FINAL_CLAW + 1)
#define IGRIS_ABYSS_GRAB (IGRIS_Z_RIP + 1)
#define IGRIS_ECLIPSE_ECHOES (IGRIS_ABYSS_GRAB + 1)
#define IGRIS_ECHO_PLANT .40f
#define IGRIS_ECHO_WARNING .44f
#define IGRIS_ECHO_GAP .58f
#define IGRIS_ECHO_TRAVEL .40f
#define IGRIS_ECHO_FADE 2.64f
#define IGRIS_ECHO_END 2.90f
#define IGRIS_ECHO_WIDTH 48.0f
#define IGRIS_GRAB_REACH 295.0f
#define IGRIS_GRAB_HALF_WIDTH 48.0f
#define IGRIS_GRAB_IMPACT .56f
#define IGRIS_GRAB_RELEASE 1.01f
#define IGRIS_GRAB_END 1.30f
#define IGRIS_Z_RADIUS 64.0f
static const float igrisZFrames[] = {0,.50f,.58f,.72f,.84f,.91f,1.00f,1.16f,1.28f,1.35f,1.51f,1.73f,1.90f};
static const int igrisZLegStart[] = {1,4,8};
static const int igrisZLegEnd[] = {3,7,10};
#define UMBRA_SPEAR_SPEED 540.0f
#define UMBRA_SPEAR_LIFETIME 1.25f

static void UnloadTexIfLoaded(Texture2D *texture) {
    if (texture->id) {
        UnloadTexture(*texture);
        *texture = (Texture2D){0};
    }
}

static void UnloadEnemyMapAssets(EnemyGroup *group) {
    UnloadNecromancerSprites(group);
    for (int i = 0; i < 3; i++) {
        UnloadTexIfLoaded(&group->cemeteryZombie[i]);
        UnloadTexIfLoaded(&group->cemeteryLantern[i]);
        UnloadTexIfLoaded(&group->cemeterySpecial[i]);
        UnloadTexIfLoaded(&group->boss[i]);
        UnloadTexIfLoaded(&group->effects[i]);
        UnloadTexIfLoaded(&group->clearingMelee[i]);
        UnloadTexIfLoaded(&group->clearingArcher[i]);
        UnloadTexIfLoaded(&group->clearingSpecial[i]);
    }
    UnloadTexIfLoaded(&group->coveiroIdle);
    UnloadTexIfLoaded(&group->coveiroEmerge);
    UnloadTexIfLoaded(&group->coveiroFall);
    UnloadTexIfLoaded(&group->coveiroWalk);
    UnloadTexIfLoaded(&group->coveiroMelee);
    UnloadTexIfLoaded(&group->cemeterySpell);
    UnloadTexIfLoaded(&group->sprites);
    UnloadTexIfLoaded(&group->backSprites);
    UnloadTexIfLoaded(&group->frontSprites);
    for (int role = TEMPLAR_SHIELD; role < TEMPLAR_MARSHAL; role++) {
        UnloadTexIfLoaded(&group->templarIdle[role]);
        UnloadTexIfLoaded(&group->templarWalk[role]);
        UnloadTexIfLoaded(&group->templarAttack[role]);
    }
    UnloadTexIfLoaded(&group->bonelandGhoul);
    UnloadTexIfLoaded(&group->bonelandCultist);
    for (int i = 0; i < BONE_SHEET_COUNT; i++)
        UnloadTexIfLoaded(&group->boneLordSheets[i]);
    UnloadTexIfLoaded(&group->bossSprites);
    UnloadTexIfLoaded(&group->guardianSpawn);
    UnloadTexIfLoaded(&group->guardianDefeat);
    UnloadTexIfLoaded(&group->bossWalk);
    UnloadTexIfLoaded(&group->bossPortrait);
    UnloadTexIfLoaded(&group->cemeteryPortrait);
    UnloadTexIfLoaded(&group->boneLordPortrait);
    UnloadTexIfLoaded(&group->templarEffects);
    for (int i = 0; i < IGRIS_ART_COUNT; i++)
        UnloadTexIfLoaded(&group->igrisArt[i]);
    UnloadTexIfLoaded(&group->templarDust);
    UnloadTexIfLoaded(&group->bossAttack);
    UnloadTexIfLoaded(&group->roots);
    UnloadTexIfLoaded(&group->rootWave);
    UnloadTexIfLoaded(&group->curse);
    UnloadTexIfLoaded(&group->guardianMelee);
    UnloadTexIfLoaded(&group->villageBruteWalk);
    UnloadTexIfLoaded(&group->villageBruteAttack);
    UnloadTexIfLoaded(&group->villageBossWalk);
    UnloadTexIfLoaded(&group->villageBossAttack);
    UnloadTexIfLoaded(&group->villageBossCharge);
    for (int i = 0; i < VILLAGE_ZOMBIE_ATLAS_COUNT; i++)
        UnloadTexIfLoaded(&group->villageZombie[i]);
    UnloadTexIfLoaded(&group->villageLetter);
    group->loadedMap = -1;
}

static bool LoadClearingEnemyAssets(EnemyGroup *group) {
    group->sprites = LoadTexture("assets/inimigo_encapuzado.png");
    group->backSprites = LoadTexture("assets/inimigo_encapuzado_costas.png");
    group->frontSprites = LoadTexture("assets/inimigo_encapuzado_frente.png");
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
    group->bossSprites = LoadTexture("assets/boss_clareira.png");
    group->guardianSpawn = LoadTexture("assets/boss_clareira_spawn.png");
    group->guardianDefeat = LoadTexture("assets/boss_clareira_derrota.png");
    group->bossWalk = LoadTexture("assets/boss_clareira_andar.png");
    group->bossPortrait = LoadTexture("assets/guardiao_intro_pixel_v2.png");
    group->bossAttack = LoadTexture("assets/boss_clareira_especial_1.png");
    group->roots = LoadTexture("assets/boss_clareira_raizes_area.png");
    group->rootWave = LoadTexture("assets/boss_clareira_onda.png");
    group->curse = LoadTexture("assets/boss_clareira_maldicao.png");
    group->guardianMelee = LoadTexture("assets/boss_clareira_corpo_a_corpo.png");
    if (!group->sprites.id || !group->backSprites.id || !group->frontSprites.id ||
        !group->clearingMelee[0].id || !group->clearingMelee[1].id || !group->clearingMelee[2].id ||
        !group->clearingArcher[0].id || !group->clearingArcher[1].id || !group->clearingArcher[2].id ||
        !group->clearingSpecial[0].id || !group->clearingSpecial[1].id || !group->clearingSpecial[2].id ||
        !group->bossSprites.id || !group->guardianSpawn.id || !group->guardianDefeat.id ||
        !group->bossWalk.id || !group->bossPortrait.id || !group->bossAttack.id ||
        !group->roots.id || !group->rootWave.id || !group->curse.id || !group->guardianMelee.id)
        return false;
    SetTextureFilter(group->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->backSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->frontSprites, TEXTURE_FILTER_POINT);
    for (int i = 0; i < 3; i++) {
        SetTextureFilter(group->clearingMelee[i], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->clearingArcher[i], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->clearingSpecial[i], TEXTURE_FILTER_POINT);
    }
    SetTextureFilter(group->bossSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianSpawn, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianDefeat, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->roots, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rootWave, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->curse, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianMelee, TEXTURE_FILTER_POINT);
    return true;
}

static bool LoadCemeteryEnemyAssets(EnemyGroup *group) {
    const char *zombieSheets[] = {
        "assets/zumbi_cemiterio_idle.png", "assets/zumbi_cemiterio_walk.png",
        "assets/zumbi_cemiterio_attack.png"
    };
    const char *lanternSheets[] = {
        "assets/zumbi_lanterna_idle.png", "assets/zumbi_lanterna_walk.png",
        "assets/zumbi_lanterna_attack.png"
    };
    const char *specialSheets[] = {
        "assets/cemiterio_especial_idle.png", "assets/cemiterio_especial_walk.png",
        "assets/cemiterio_especial_attack.png"
    };
    for (int i = 0; i < 3; i++) {
        group->cemeteryZombie[i] = LoadTexture(zombieSheets[i]);
        if (group->cemeteryZombie[i].id) SetTextureFilter(group->cemeteryZombie[i], TEXTURE_FILTER_POINT);
        group->cemeteryLantern[i] = LoadTexture(lanternSheets[i]);
        if (group->cemeteryLantern[i].id) SetTextureFilter(group->cemeteryLantern[i], TEXTURE_FILTER_POINT);
        group->cemeterySpecial[i] = LoadTexture(specialSheets[i]);
        if (group->cemeterySpecial[i].id) SetTextureFilter(group->cemeterySpecial[i], TEXTURE_FILTER_POINT);
    }
    group->coveiroIdle = LoadTexture("assets/coveiro_novo_idle.png");
    group->coveiroWalk = LoadTexture("assets/coveiro_novo_run.png");
    group->coveiroEmerge = LoadTexture("assets/coveiro_novo_emerge.png");
    group->coveiroFall = LoadTexture("assets/coveiro_novo_fall.png");
    group->coveiroMelee = LoadTexture("assets/coveiro_novo_melee.png");
    group->cemeterySpell = LoadTexture("assets/zumbi_lanterna_projetil.png");
    group->boss[0] = LoadTexture("assets/coveiro_novo_pull.png");
    group->boss[1] = LoadTexture("assets/coveiro_novo_spin.png");
    group->boss[2] = LoadTexture("assets/coveiro_novo_charge.png");
    group->effects[0] = LoadTexture("assets/coveiro_novo_pull_effect.png");
    group->effects[1] = LoadTexture("assets/coveiro_pa_vento_v4.png");
    group->effects[2] = LoadTexture("assets/coveiro_efeito_investida.png");
    group->cemeteryPortrait = LoadTexture("assets/coveiro_intro_pixel.png");
    if (!group->cemeteryZombie[0].id || !group->cemeteryZombie[1].id || !group->cemeteryZombie[2].id ||
        !group->cemeteryLantern[0].id || !group->cemeteryLantern[1].id || !group->cemeteryLantern[2].id ||
        !group->cemeterySpecial[0].id || !group->cemeterySpecial[1].id || !group->cemeterySpecial[2].id ||
        !group->coveiroIdle.id || !group->coveiroEmerge.id || !group->coveiroFall.id ||
        !group->coveiroWalk.id || !group->coveiroMelee.id || !group->cemeterySpell.id || !group->boss[0].id ||
        !group->boss[1].id || !group->boss[2].id || !group->effects[0].id ||
        !group->effects[1].id || !group->effects[2].id || !group->cemeteryPortrait.id)
        return false;
    SetTextureFilter(group->coveiroIdle, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroEmerge, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroFall, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroMelee, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->cemeterySpell, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->cemeteryPortrait, TEXTURE_FILTER_POINT);
    for (int i = 0; i < 3; i++) SetTextureFilter(group->boss[i], TEXTURE_FILTER_POINT);
    for (int i = 0; i < 3; i++) SetTextureFilter(group->effects[i], TEXTURE_FILTER_POINT);
    return true;
}

static bool LoadVillageEnemyAssets(EnemyGroup *group) {
    group->villageBruteWalk = LoadTexture("assets/zumbi_vila_forte_marcha_v2.png");
    group->villageBruteAttack = LoadTexture("assets/zumbi_vila_forte_pancada_v2.png");
    if (!group->villageBruteWalk.id || !group->villageBruteAttack.id) return false;
    SetTextureFilter(group->villageBruteWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->villageBruteAttack, TEXTURE_FILTER_POINT);
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
        if (!group->villageZombie[i].id) return false;
        SetTextureFilter(group->villageZombie[i], TEXTURE_FILTER_POINT);
    }
    if (!group->villageBossWalk.id || !group->villageBossAttack.id || !group->villageBossCharge.id)
        return false;
    SetTextureFilter(group->villageBossWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->villageBossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->villageBossCharge, TEXTURE_FILTER_POINT);
    group->villageLetter = LoadTexture("assets/cutscenes/village_aftermath/images/carta.png");
    if (group->villageLetter.id) SetTextureFilter(group->villageLetter, TEXTURE_FILTER_BILINEAR);
    return true;
}

static bool LoadBonelandEnemyAssets(EnemyGroup *group) {
    group->bonelandGhoul = LoadBonelandSprite("assets/terra_dos_ossos_carnical.png");
    group->bonelandCultist = LoadBonelandSprite("assets/terra_dos_ossos_cultista.png");
    const char *boneSheets[BONE_SHEET_COUNT] = {
        "assets/rasga_ossos/idle.png", "assets/rasga_ossos/marcha.png",
        "assets/rasga_ossos/garra.png", "assets/rasga_ossos/disparo.png",
        "assets/rasga_ossos/chuva.png", "assets/rasga_ossos/chuva_fx.png",
        "assets/rasga_ossos/dash.png", "assets/rasga_ossos/explosao.png",
        "assets/rasga_ossos/explosao_fx.png",
    };
    for (int i = 0; i < BONE_SHEET_COUNT; i++) {
        group->boneLordSheets[i] = LoadTexture(boneSheets[i]);
        if (!group->boneLordSheets[i].id) return false;
        SetTextureFilter(group->boneLordSheets[i], TEXTURE_FILTER_POINT);
    }
    group->boneLordPortrait = LoadTexture("assets/terra_dos_ossos_chefe_retrato.png");
    if (!group->bonelandGhoul.id || !group->bonelandCultist.id || !group->boneLordPortrait.id)
        return false;
    SetTextureFilter(group->bonelandGhoul, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bonelandCultist, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->boneLordPortrait, TEXTURE_FILTER_POINT);
    return true;
}

static bool LoadTemplarEnemyAssets(EnemyGroup *group) {
    static const char *const idleFiles[TEMPLAR_MARSHAL] = {
        [TEMPLAR_SHIELD] = "assets/templarios/espadachim_idle.png",
        [TEMPLAR_CROSSBOW] = "assets/templarios/arqueiro_idle.png",
    };
    static const char *const templarFiles[] = {
        [TEMPLAR_SHIELD] = "assets/templarios/espadachim_andar.png",
        [TEMPLAR_EXECUTIONER] = "assets/templario_carrasco.png",
        [TEMPLAR_CROSSBOW] = "assets/templarios/arqueiro_andar.png",
        [TEMPLAR_STANDARD] = "assets/templario_estandarte.png",
    };
    static const char *const attackFiles[] = {
        [TEMPLAR_SHIELD] = "assets/templarios/espadachim_ataque.png",
        [TEMPLAR_EXECUTIONER] = "assets/templario_carrasco_ataque_v2.png",
        [TEMPLAR_CROSSBOW] = "assets/templarios/arqueiro_ataque.png",
        [TEMPLAR_STANDARD] = "assets/templario_estandarte_ataque_v2.png",
    };
    for (int role = TEMPLAR_SHIELD; role < TEMPLAR_MARSHAL; role++) {
        if (idleFiles[role]) {
            group->templarIdle[role] = LoadTexture(idleFiles[role]);
            if (!group->templarIdle[role].id) return false;
            SetTextureFilter(group->templarIdle[role], TEXTURE_FILTER_POINT);
        }
        group->templarWalk[role] = LoadTexture(templarFiles[role]);
        group->templarAttack[role] = LoadTexture(attackFiles[role]);
        if (!group->templarWalk[role].id || !group->templarAttack[role].id) return false;
        SetTextureFilter(group->templarWalk[role], TEXTURE_FILTER_POINT);
        SetTextureFilter(group->templarAttack[role], TEXTURE_FILTER_POINT);
    }
    static const char *const igrisFiles[IGRIS_ART_COUNT] = {
        "idle", "walk", "melee", "eclipse", "eclipse_fx", "eruption", "eruption_fx",
        "spears", "spears_fx", "vortex", "vortex_fx", "final_idle", "final_walk", "final_melee",
        "z_down", "z_up", "z_left", "z_right", "abyss_grab", "abyss_grab_fx", "echoes", "echoes_fx"
    };
    for (int i = 0; i < IGRIS_ART_COUNT; i++) {
        group->igrisArt[i] = LoadTexture(TextFormat("assets/igris/%s.png", igrisFiles[i]));
        if (!group->igrisArt[i].id) return false;
        SetTextureFilter(group->igrisArt[i], TEXTURE_FILTER_POINT);
    }
    group->templarDust = LoadTexture("assets/templario_poeira.png");
    if (!group->templarDust.id) return false;
    SetTextureFilter(group->templarDust, TEXTURE_FILTER_POINT);
    if (FileExists("assets/templario_efeitos.png")) {
        group->templarEffects = LoadTexture("assets/templario_efeitos.png");
        if (group->templarEffects.id) SetTextureFilter(group->templarEffects, TEXTURE_FILTER_POINT);
    }
    return true;
}

bool EnsureEnemyMapAssets(EnemyGroup *group, int mapId) {
    if (!group) return false;
    if (group->loadedMap == mapId) return true;
    UnloadEnemyMapAssets(group);
    bool ok = true;
    switch (mapId) {
        case 0: ok = LoadClearingEnemyAssets(group); break;
        case 1: ok = LoadCemeteryEnemyAssets(group); break;
        case 2: ok = LoadVillageEnemyAssets(group); break;
        case 3:
        case 4:
        case 5: ok = true; break;
        case 6: ok = LoadBonelandEnemyAssets(group); break;
        case 7: ok = LoadTemplarEnemyAssets(group); break;
        case 8: ok = LoadNecromancerSprites(group); break;
        default: ok = true; break;
    }
    if (!ok) {
        UnloadEnemyMapAssets(group);
        return false;
    }
    group->loadedMap = mapId;
    return true;
}

bool InitEnemies(EnemyGroup *group) {
    *group = (EnemyGroup){0};
    group->loadedMap = -1;
    group->sealMarker = LoadTexture("assets/inimigo_selo.png");
    if (!group->sealMarker.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->sealMarker, TEXTURE_FILTER_POINT);
    return true;
}

void UnloadEnemies(EnemyGroup *group) {
    UnloadEnemyMapAssets(group);
    UnloadTexIfLoaded(&group->sealMarker);
    *group = (EnemyGroup){0};
    group->loadedMap = -1;
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
        [TEMPLAR_SHIELD] = {ENEMY_COMMON_MELEE, 242, 112, 1.38f},
        [TEMPLAR_EXECUTIONER] = {ENEMY_SPECIAL, 311, 130, 1.50f},
        [TEMPLAR_CROSSBOW] = {ENEMY_COMMON_RANGED, 173, 125, 1.50f},
        [TEMPLAR_STANDARD] = {ENEMY_COMMON_MELEE, 207, 84, .9f},
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
            .health = stats[role].health, .maxHealth = stats[role].health,
            .speed = stats[role].speed, .power = stats[role].power,
            .attackCooldown = role == TEMPLAR_CROSSBOW ? .65f + .1f * i : .8f,
            .stateTime = -.12f * i, .lastKatanaSwing = -1
        };
    }
}

void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower) {
    if (IsWindowReady()) EnsureEnemyMapAssets(group, ClearingMapId(clearing));
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
    static const int cemeteryCounts[CEMETERY_WAVES] = {12,18,20};
    int total = clearing->cemetery ? cemeteryCounts[(int)Clamp(wave,1,CEMETERY_WAVES)-1] :
        clearingWave ? CLEARING_FIRST_WAVE_ENEMIES + (wave - 1) * CLEARING_WAVE_ENEMY_STEP :
        7 + (wave - 1) * 4;
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
        if (clearingWave || clearing->cemetery)
            enemy->maxHealth=(int)ceilf(enemy->maxHealth*CLEARING_ENEMY_HEALTH_SCALE);
        if (clearing->cemetery) enemy->maxHealth+=90;
        enemy->health = enemy->maxHealth;
        enemy->speed = ENEMY_SPEED * (1.0f + (wave-1)*.045f);
        enemy->power = 1.0f + (wave-1)*.09f;
        if (clearingWave || clearing->cemetery)
            enemy->damageScale = CLEARING_ENEMY_DAMAGE_SCALE * (clearing->cemetery ? 1.30f : 1.0f);
        enemy->state = ENEMY_SPAWNING;
        enemy->stateTime = -WaveSpawnDelay(wave, i);
        enemy->facing = i%2 ? 2 : 3;
        enemy->lastKatanaSwing = -1;
        enemy->attackCooldown = 0.8f;
    }
}

void StartArenaBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player, float power) {
    if (IsWindowReady()) EnsureEnemyMapAssets(group, ClearingMapId(clearing));
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
        : clearing->village ? 20 : ARENA_BOSS_BASE_HEALTH;
    Vector2 bossPos = clearing->templarField ? (Vector2){835, 290} : clearing->village
        ? (Vector2){clearing->background.width * 0.62f, clearing->background.height * 0.54f}
        : SpawnPosition(clearing, player);
    *boss = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_SPAWNING,
        .position = bossPos, .villageLeader = clearing->village,
        .maxHealth = clearing->village ? baseHealth : (int)(baseHealth*fmaxf(1, power)) +
            (clearing->cemetery ? CEMETERY_BOSS_HEALTH_BONUS : 0),
        .speed = clearing->village || clearing->templarField ? 185 : (clearing->cemetery ? 160 : 100),
        .power = power, .attackCooldown = 1.25f, .lastKatanaSwing = -1,
        .bossAttack = (clearing->cemetery ? COVEIRO_ATTACK_COUNT : GUARDIAN_ATTACK_COUNT) - 1};
    boss->health = boss->maxHealth;
    if (clearing->boneland) {
        boss->bossAttack = BONE_BLAST;
        boss->speed = 170;
    }
    if (clearing->templarField) {
        boss->templarRole = TEMPLAR_MARSHAL;
        boss->bossAttack = IGRIS_MELEE;
        boss->attackCooldown = .25f;
    }
}

int EnemiesRemaining(const EnemyGroup *group) {
    int count = 0;
    for (int i=0; i<ENEMY_COUNT; i++) if (group->list[i].state != ENEMY_GONE) count++;
    return count + (group->village ? group->villageSpawnRemaining : 0);
}

float EnemyHurtRadius(const Enemy *enemy) {
    return enemy->type == ENEMY_BOSS ? 48.0f : enemy->villageBrute ? 26.0f : ENEMY_HURT_RADIUS;
}

bool EnemyCanBeHit(const Enemy *enemy) {
    if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) return false;
    return enemy->state != ENEMY_DIE && enemy->state != ENEMY_GONE &&
           enemy->state != ENEMY_SPAWNING && enemy->state != ENEMY_FALLEN && enemy->health > 0;
}

Vector2 EnemyCenter(const Enemy *enemy) {
    return (Vector2){enemy->position.x, enemy->position.y-(enemy->type == ENEMY_BOSS ? 75.0f : enemy->villageBrute ? 48.0f : 36.0f)};
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
    enemy->health -= enemy->villageLeader ? enemy->health : damage;
    int finalThreshold = enemy->maxHealth * IGRIS_FINAL_HEALTH_PERCENT / 100;
    if (enemy->templarRole == TEMPLAR_MARSHAL && enemy->guardianPhase == GUARDIAN_PHASE_TWO &&
        !enemy->igrisFinalForm && enemy->health <= finalThreshold) {
        enemy->health = finalThreshold;
        enemy->igrisFinalForm = true;
        enemy->attackBurst = 0;
        enemy->guardianPhase = GUARDIAN_PHASE_TRANSITION;
        enemy->state = ENEMY_IDLE;
        enemy->stateTime = 0;
    } else if (enemy->type == ENEMY_BOSS && !enemy->villageLeader && enemy->guardianPhase == GUARDIAN_PHASE_ONE &&
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
                return enemy->power * 1.35f;
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
                EnemyHitDamage(group, enemy, ENEMY_ATTACK_DAMAGE), false, false};
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

#define BONE_DASH_HIDE .26f
#define BONE_DASH_REAPPEAR .63f
#define BONE_SHOT_LIFETIME 1.65f
#define BONE_SHOT_SPEED 680.0f

static const struct {
    float range, windup, impactEnd, duration, radius;
    int damage;
} boneAttacks[BONE_ATTACK_COUNT] = {
    [BONE_CLAW]   = {120, .34f, .44f, .72f, 120, 22},
    [BONE_RANGED] = {550, .70f, .85f, 1.24f,  27, 25},
    [BONE_RAIN]   = {520, .74f, 1.11f, 1.51f, 104, 28},
    [BONE_DASH]   = {440, .83f, 1.10f, 1.37f,  64, 32},
    [BONE_BLAST]  = {220, .72f, 1.03f, 1.40f, 220, 38},
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

static bool BoneLordHit(EnemyGroup *group, const Enemy *boss, bool dodging, int *health) {
    if (*health <= 0 || group->practiceInvincible || dodging || group->playerHitCooldown > 0)
        return false;
    int damage = (int)(boneAttacks[boss->bossAttack].damage * boss->power);
    *health = damage < *health ? *health - damage : 0;
    group->playerHitCooldown = .65f;
    return true;
}

static void FireBoneLordShot(EnemyGroup *group, const Enemy *boss) {
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        if (group->projectiles[i].remaining > 0) continue;
        group->projectiles[i] = (EnemyProjectile){
            .position = {boss->position.x, boss->position.y - 35},
            .direction = boss->attackDirection, .remaining = BONE_SHOT_LIFETIME,
            .damage = (int)(boneAttacks[BONE_RANGED].damage * boss->power), .boneShot = true};
        return;
    }
}

static Vector2 BoneDashFlank(const Clearing *map, Vector2 origin, Vector2 direction, Vector2 player) {
    Vector2 lateral = {-direction.y, direction.x};
    for (int side = 1; side >= -1; side -= 2) {
        Vector2 flank = Vector2Add(Vector2Subtract(player, Vector2Scale(direction, 118)),
                                   Vector2Scale(lateral, side * 122));
        if (!IsMapActorBlocked(map, flank) && MapHasClearPath(map, origin, flank))
            return flank;
    }
    return origin;
}

static void UpdateBoneLord(EnemyGroup *group, const Clearing *map, Enemy *boss,
                           Vector2 player, float dt, bool dodging, int *health) {
    Vector2 toPlayer = Vector2Subtract(player, boss->position);
    float distance = Vector2Length(toPlayer);
    if (boss->state != ENEMY_ATTACK) {
        if (*health <= 0) return;
        boss->facing = FacingToward(toPlayer);
        if (boss->attackCooldown <= 0 && MapHasClearPath(map, boss->position, player)) {
            for (int i = 1; i <= BONE_ATTACK_COUNT; i++) {
                int attack = (boss->bossAttack + i) % BONE_ATTACK_COUNT;
                if (distance > boneAttacks[attack].range) {
                    /* After the dash, pursue the player until the blast is in range. */
                    if (attack == BONE_BLAST && i == 1) break;
                    continue;
                }
                boss->bossAttack = attack;
                boss->attackDirection = distance > .001f ? Vector2Normalize(toPlayer) : (Vector2){0, 1};
                boss->attackTarget = attack == BONE_DASH ?
                    BoneDashFlank(map, boss->position, boss->attackDirection, player) : player;
                boss->state = ENEMY_ATTACK;
                boss->stateTime = 0;
                boss->attackConnected = false;
                return;
            }
        }
        boss->state = distance > 70 ? ENEMY_WALK : ENEMY_IDLE;
        if (boss->state == ENEMY_WALK) {
            boss->walkTime += dt;
            MoveEnemy(map, boss, Vector2Scale(toPlayer, boss->speed *
                      (GuardianEnraged(boss) ? 1.27f : 1.0f) * dt / distance));
        }
        return;
    }

    int attack = boss->bossAttack;
    float t = boss->stateTime, previous = t - dt;
    float windup = boneAttacks[attack].windup;
    float end = boneAttacks[attack].impactEnd;
    float active = fminf(t, end) - fmaxf(previous, windup);
    if (attack == BONE_DASH) {
        float hidden = fminf(t, BONE_DASH_REAPPEAR) - fmaxf(previous, BONE_DASH_HIDE);
        if (hidden > 0) {
            Vector2 remaining = Vector2Subtract(boss->attackTarget, boss->position);
            float fraction = Clamp(hidden / (BONE_DASH_REAPPEAR - fmaxf(previous, BONE_DASH_HIDE)), 0, 1);
            MoveEnemy(map, boss, Vector2Scale(remaining, fraction));
        }
        if (previous < BONE_DASH_REAPPEAR && t >= BONE_DASH_REAPPEAR) {
            Vector2 aim = Vector2Subtract(player, boss->position);
            if (Vector2LengthSqr(aim) > .001f) boss->attackDirection = Vector2Normalize(aim);
            boss->facing = FacingToward(boss->attackDirection);
        }
        if (active > 0) {
            Vector2 origin = boss->position;
            MoveEnemyStraight(map, boss, active * (GuardianEnraged(boss) ? 1000 : 870));
            Vector2 segment = Vector2Subtract(boss->position, origin);
            float lengthSquared = Vector2LengthSqr(segment);
            float along = lengthSquared > 0 ?
                Clamp(Vector2DotProduct(Vector2Subtract(player, origin), segment) / lengthSquared, 0, 1) : 0;
            Vector2 contact = Vector2Add(origin, Vector2Scale(segment, along));
            if (!boss->attackConnected && Vector2Distance(contact, player) <= boneAttacks[attack].radius &&
                MapHasClearPath(map, contact, player))
                boss->attackConnected = BoneLordHit(group, boss, dodging, health);
        }
    } else if (!boss->attackConnected && active > 0) {
        if (attack == BONE_RANGED) {
            FireBoneLordShot(group, boss);
            boss->attackConnected = true;
        } else {
            Vector2 center = attack == BONE_RAIN ? boss->attackTarget : boss->position;
            bool inArc = attack != BONE_CLAW ||
                Vector2DotProduct(boss->attackDirection, toPlayer) >= distance * .45f;
            if (inArc && Vector2Distance(center, player) <= boneAttacks[attack].radius &&
                MapHasClearPath(map, center, player))
                boss->attackConnected = BoneLordHit(group, boss, dodging, health);
        }
    }
    if (t >= boneAttacks[attack].duration) {
        boss->state = ENEMY_IDLE;
        boss->stateTime = 0;
        boss->attackCooldown = GuardianEnraged(boss) ? .24f : .42f;
    }
}

static void HurtByTemplar(EnemyGroup *group, Vector2 player, int *health, bool dodging, int damage) {
    if (dodging || group->practiceInvincible || group->playerHitCooldown > 0 || *health <= 0 || damage <= 0)
        return;
    *health = damage < *health ? *health - damage : 0;
    group->playerHitCooldown = .65f;
    group->templarBlood = (EnemyImpact){.position={player.x, player.y - 36}, .remaining=.36f};
}

static void FireUmbraSpears(EnemyGroup *group, const Enemy *boss, int damage) {
    Vector2 direction = boss->attackDirection;
    Vector2 normal = {-direction.y,direction.x};
    Vector2 origin = Vector2Add(boss->position,Vector2Scale(direction,36));
    origin.y -= 35;
    int lane = -1;
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT && lane <= 1; i++) {
        if (group->projectiles[i].remaining > 0) continue;
        group->projectiles[i] = (EnemyProjectile){
            .position = Vector2Add(origin,Vector2Scale(normal,lane*42)),
            .direction = direction, .remaining = UMBRA_SPEAR_LIFETIME,
            .damage = damage, .umbraSpear = true};
        lane++;
    }
}

static bool PlanIgrisZ(const Clearing *map, Enemy *boss, int facing, float distance) {
    static const Vector2 directions[] = {{0,1},{0,-1},{-1,0},{1,0}};
    Vector2 forward = directions[facing];
    float side = facing < 2 ? -1 : 1;
    Vector2 across = {-forward.y*side,forward.x*side};
    Vector2 start = boss->position;
    for (float scale = 1; scale >= .54f; scale -= .15f) {
        Vector2 along = Vector2Scale(forward,Clamp(distance+55,190,305)*scale);
        Vector2 offset = Vector2Scale(across,172*scale);
        Vector2 points[] = {start,Vector2Add(start,along),Vector2Add(start,offset),
                            Vector2Add(Vector2Add(start,offset),along)};
        bool clear = true;
        for (int i = 0; i < 3 && clear; i++) {
            int steps = (int)ceilf(Vector2Distance(points[i],points[i+1])/4);
            clear = MapHasClearPath(map,points[i],points[i+1]);
            for (int j = 1; j <= steps && clear; j++)
                clear = !IsMapActorBlocked(map,Vector2Lerp(points[i],points[i+1],(float)j/steps));
        }
        if (!clear) continue;
        memcpy(boss->igrisZPath,points,sizeof(points));
        return true;
    }
    return false;
}

static void UpdateIgrisZ(EnemyGroup *group, const Clearing *map, Enemy *boss,
                         Vector2 player, float dt, bool dodging, int *health) {
    float t = boss->stateTime;
    for (int leg = 0; leg < 3; leg++) {
        float start = igrisZFrames[igrisZLegStart[leg]], end = igrisZFrames[igrisZLegEnd[leg]];
        if (t <= start || t-dt >= end) continue;
        float fraction = Clamp((t-start)/(end-start),0,1);
        Vector2 from = boss->position;
        Vector2 next = Vector2Lerp(boss->igrisZPath[leg],boss->igrisZPath[leg+1],fraction);
        MoveEnemy(map,boss,Vector2Subtract(next,from));
        Vector2 traveled = Vector2Subtract(boss->position,from);
        float length = Vector2LengthSqr(traveled);
        float nearest = length > 0 ? Clamp(Vector2DotProduct(Vector2Subtract(player,from),traveled)/length,0,1) : 0;
        Vector2 contact = Vector2Add(from,Vector2Scale(traveled,nearest));
        if (!boss->attackConnected && !dodging && Vector2Distance(player,contact) <= IGRIS_Z_RADIUS &&
            MapHasClearPath(map,contact,player)) {
            boss->attackConnected = true;
            HurtByTemplar(group,player,health,false,(int)(32*boss->power));
        }
    }
    if (!boss->attackConnected && t >= igrisZFrames[10] && t-dt < igrisZFrames[10]) {
        boss->attackConnected = true;
        if (Vector2Distance(boss->position,player) <= 104 && MapHasClearPath(map,boss->position,player))
            HurtByTemplar(group,player,health,dodging,(int)(40*boss->power));
    }
    if (t >= igrisZFrames[12]) {
        boss->state = ENEMY_IDLE;
        boss->stateTime = 0;
        boss->attackCooldown = .24f;
    }
}

static bool IgrisGrabConnected(const Enemy *boss) {
    return boss->templarRole == TEMPLAR_MARSHAL && boss->state == ENEMY_ATTACK &&
           boss->bossAttack == IGRIS_ABYSS_GRAB && boss->attackConnected;
}

bool IgrisHoldingPlayer(const EnemyGroup *group) {
    const Enemy *boss = &group->list[0];
    return IgrisGrabConnected(boss) && boss->stateTime < IGRIS_GRAB_RELEASE;
}

void ApplyIgrisGrab(const EnemyGroup *group, const Clearing *map, Vector2 *player, float dt) {
    const Enemy *boss = &group->list[0];
    if (!IgrisGrabConnected(boss)) return;
    float active = fminf(boss->stateTime,IGRIS_GRAB_RELEASE) -
                   fmaxf(boss->stateTime-dt,IGRIS_GRAB_IMPACT);
    if (active <= 0) return;
    Vector2 delta = Vector2Subtract(boss->position,*player);
    float distance = Vector2Length(delta);
    if (distance > 72)
        MoveMapActor(map,player,Vector2Scale(delta,fminf(distance-72,570*active)/distance));
}

static void UpdateAbyssGrab(EnemyGroup *group, const Clearing *map, Enemy *boss,
                            Vector2 player, float dt, bool dodging, int *health) {
    float t = boss->stateTime;
    if (t >= IGRIS_GRAB_IMPACT && t-dt < IGRIS_GRAB_IMPACT &&
        !dodging && !group->practiceInvincible && group->playerHitCooldown <= 0 && *health > 0) {
        Vector2 delta = Vector2Subtract(player,boss->position);
        float along = Vector2DotProduct(delta,boss->attackDirection);
        Vector2 side = Vector2Subtract(delta,Vector2Scale(boss->attackDirection,along));
        if (along >= 0 && along <= IGRIS_GRAB_REACH && Vector2LengthSqr(side) <= IGRIS_GRAB_HALF_WIDTH*IGRIS_GRAB_HALF_WIDTH &&
            MapHasClearPath(map,boss->position,player)) {
            boss->attackConnected = true;
            HurtByTemplar(group,player,health,false,(int)(24*boss->power));
        }
    }
    if (t >= IGRIS_GRAB_END) {
        boss->state = ENEMY_IDLE;
        boss->stateTime = 0;
        boss->attackCooldown = .27f;
    }
}

static bool PlanIgrisEchoes(const Clearing *map, Enemy *boss, Vector2 target, Vector2 direction) {
    int count = 0;
    for (int attempt = 0; attempt < 18 && count < 3; attempt++) {
        float angle = (attempt%9) * (2*PI/9);
        float radius = attempt < 9 ? 175 : 115;
        Vector2 start = Vector2Add(target,Vector2Scale(Vector2Rotate(direction,angle),radius));
        if (IsMapActorBlocked(map,start) || !MapHasClearPath(map,start,target) ||
            Vector2Distance(start,boss->position) < 76) continue;
        bool separated = true;
        for (int i = 0; i < count; i++)
            if (Vector2Distance(start,boss->igrisEchoFrom[i]) < 94) separated = false;
        if (!separated) continue;
        Vector2 aim = Vector2Normalize(Vector2Subtract(target,start));
        Vector2 end = target;
        for (int j = 1; j <= 12; j++) {
            Vector2 next = Vector2Add(target,Vector2Scale(aim,j*8));
            if (IsMapActorBlocked(map,next) || !MapHasClearPath(map,end,next)) break;
            end = next;
        }
        boss->igrisEchoFrom[count] = start;
        boss->igrisEchoTo[count++] = end;
    }
    boss->igrisEchoHits = 0;
    return count == 3;
}

static float IgrisEchoLaunch(int index) {
    return IGRIS_ECHO_PLANT + IGRIS_ECHO_WARNING + index*IGRIS_ECHO_GAP;
}

static void UpdateIgrisEchoes(EnemyGroup *group, const Clearing *map, Enemy *boss,
                              Vector2 player, float dt, bool dodging, int *health) {
    for (int i = 0; i < 3; i++) {
        float launch = IgrisEchoLaunch(i);
        float age = boss->stateTime-launch;
        unsigned hit = 1u << i;
        if (age < 0 || age-dt >= IGRIS_ECHO_TRAVEL || (boss->igrisEchoHits & hit)) continue;
        Vector2 a = Vector2Lerp(boss->igrisEchoFrom[i],boss->igrisEchoTo[i],Clamp((age-dt)/IGRIS_ECHO_TRAVEL,0,1));
        Vector2 b = Vector2Lerp(boss->igrisEchoFrom[i],boss->igrisEchoTo[i],Clamp(age/IGRIS_ECHO_TRAVEL,0,1));
        Vector2 step = Vector2Subtract(b,a);
        float length = Vector2LengthSqr(step);
        float progress = length > 0 ? Clamp(Vector2DotProduct(Vector2Subtract(player,a),step)/length,0,1) : 0;
        Vector2 contact = Vector2Add(a,Vector2Scale(step,progress));
        if (!dodging && group->playerHitCooldown <= 0 &&
            Vector2Distance(player,contact) <= IGRIS_ECHO_WIDTH && MapHasClearPath(map,contact,player)) {
            boss->igrisEchoHits |= hit;
            HurtByTemplar(group,player,health,false,(int)(28*boss->power));
        }
    }
    if (boss->stateTime >= IGRIS_ECHO_END) {
        boss->state = ENEMY_IDLE;
        boss->stateTime = 0;
        boss->attackCooldown = .30f;
    }
}

static void UpdateIgris(EnemyGroup *group, const Clearing *map, Enemy *boss, Vector2 player,
                        float dt, bool dodging, int *health) {
    Vector2 delta = Vector2Subtract(player,boss->position);
    float distance = Vector2Length(delta);
    Vector2 direction = distance > .001f ? Vector2Scale(delta,1/distance) : (Vector2){0,1};
    bool enraged = GuardianEnraged(boss);
    if (boss->state != ENEMY_ATTACK) {
        if (*health <= 0) return;
        boss->facing = FacingToward(direction);
        if (boss->attackCooldown <= 0 && MapHasClearPath(map,boss->position,player)) {
            static const int pattern[] = {IGRIS_ECLIPSE, IGRIS_SPEARS, IGRIS_VORTEX, IGRIS_ERUPTION, IGRIS_MELEE};
            int choices = boss->igrisFinalForm ? 1 : IGRIS_SKILL_COUNT;
            for (int offset = 0; offset < choices; offset++) {
                int index = (boss->attackBurst+offset)%IGRIS_SKILL_COUNT;
                int skill = boss->igrisFinalForm ? IGRIS_FINAL_CLAW : pattern[index];
                if (boss->igrisFinalForm) {
                    if (boss->attackBurst == 3 && distance <= 420 && PlanIgrisEchoes(map,boss,player,direction))
                        skill = IGRIS_ECLIPSE_ECHOES;
                    else if (boss->attackBurst == 2 && distance <= IGRIS_GRAB_REACH)
                        skill = IGRIS_ABYSS_GRAB;
                    else if ((boss->attackBurst == 1 || distance > 178) &&
                        distance <= 340 && PlanIgrisZ(map,boss,boss->facing,distance))
                        skill = IGRIS_Z_RIP;
                    else if (distance > 178 && distance <= IGRIS_GRAB_REACH)
                        skill = IGRIS_ABYSS_GRAB;
                }
                float reach = skill == IGRIS_ECLIPSE_ECHOES ? 420 : skill == IGRIS_ABYSS_GRAB ? IGRIS_GRAB_REACH :
                    skill == IGRIS_Z_RIP ? 340 : skill == IGRIS_ERUPTION ? 420 : igrisSkills[skill].reach;
                if (distance > reach) continue;
                boss->attackBurst = boss->igrisFinalForm ?
                    (boss->attackBurst == 3 ? 0 : skill == IGRIS_ABYSS_GRAB ? 3 :
                     skill == IGRIS_Z_RIP || boss->attackBurst == 1 ? 2 : 1) :
                    (index+1)%IGRIS_SKILL_COUNT;
                boss->bossAttack = skill;
                boss->attackDirection = direction;
                boss->attackTarget = player;
                boss->state = ENEMY_ATTACK;
                boss->stateTime = 0;
                boss->attackConnected = false;
                return;
            }
        }
        if (distance > 90) {
            boss->state = ENEMY_WALK;
            boss->walkTime += dt;
            float speedScale = boss->igrisFinalForm ? 1.30f : enraged ? 1.13f : 1;
            MoveEnemy(map,boss,Vector2Scale(direction,boss->speed*dt*speedScale));
        } else boss->state = ENEMY_IDLE;
        return;
    }

    int skill = boss->bossAttack;
    if (skill == IGRIS_Z_RIP) {
        UpdateIgrisZ(group,map,boss,player,dt,dodging,health);
        return;
    }
    if (skill == IGRIS_ABYSS_GRAB) {
        UpdateAbyssGrab(group,map,boss,player,dt,dodging,health);
        return;
    }
    if (skill == IGRIS_ECLIPSE_ECHOES) {
        UpdateIgrisEchoes(group,map,boss,player,dt,dodging,health);
        return;
    }
    float time = boss->stateTime;
    if (!boss->attackConnected && time >= igrisSkills[skill].impact) {
        boss->attackConnected = true;
        int damage = (int)(igrisSkills[skill].damage*boss->power);
        if (skill == IGRIS_SPEARS) {
            FireUmbraSpears(group,boss,damage);
        } else {
            Vector2 center = skill == IGRIS_ERUPTION ? boss->attackTarget : boss->position;
            if (Vector2Distance(center,player) <= igrisSkills[skill].reach &&
                Vector2DotProduct(direction,boss->attackDirection) >= igrisSkills[skill].arcDot &&
                MapHasClearPath(map,boss->position,player))
                HurtByTemplar(group,player,health,dodging,damage);
        }
    }
    if (time >= igrisSkills[skill].duration) {
        boss->state = ENEMY_IDLE;
        boss->stateTime = 0;
        boss->attackCooldown = boss->igrisFinalForm ? .16f : enraged ? .22f : .43f;
    }
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
                enemy->bossAttack = enemy->templarRole == TEMPLAR_MARSHAL ? IGRIS_MELEE :
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
            UpdateIgris(group, clearing, enemy, player, dt, playerDodging, playerHealth);
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
                if (t >= executionerAttackDuration) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
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
            if (enemy->villageBrute) {
                VillageUpdateBruteAttack(group,clearing,enemy,player,playerDodging,playerHealth);
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
            if (enemy->stateTime >= (crossbow ? crossbowAttackDuration : shield ? shieldAttackDuration : cultist ? CULTIST_CAST_DURATION : mage ? .95f : CLEARING_MELEE_ATTACK_DURATION)) {
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
                    enemy->attackCooldown = enemy->templarRole == TEMPLAR_EXECUTIONER ? 1.78f :
                        2.75f / fminf(enemy->power, 1.36f);
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
                enemy->attackCooldown = enemy->templarRole == TEMPLAR_CROSSBOW ? 1.59f :
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
            enemy->attackCooldown = enemy->villageBrute ? 1.85f :
                enemy->templarRole == TEMPLAR_SHIELD ? .83f :
                1.25f / fminf(enemy->power, 1.36f);
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
        float speed = shot->umbraSpear ? UMBRA_SPEAR_SPEED : shot->boneShot ? BONE_SHOT_SPEED : group->cemetery ? 273.0f : group->boneland ? CULTIST_SHOT_SPEED : RANGED_SHOT_SPEED;
        Vector2 movement = Vector2Scale(shot->direction, speed * dt);
        int steps = (int)(Vector2Length(movement) / 6.0f) + 1;
        Vector2 step = Vector2Scale(movement, 1.0f / steps);
        for (int j = 0; j < steps && shot->remaining > 0.0f; j++) {
            Vector2 next = Vector2Add(shot->position, step);
            Vector2 collisionPoint=next;
            if (group->cemetery || group->boneland || shot->umbraSpear) collisionPoint.y+=35;
            if (IsClearingBlocked(clearing, collisionPoint)) {
                if (group->cemetery || group->boneland) AddEnemyImpact(group,next,shot->direction);
                shot->remaining = 0.0f;
            } else {
                shot->position = next;
                Vector2 playerCenter = {player.x, player.y - 35.0f};
                float radius = shot->boneShot ? boneAttacks[BONE_RANGED].radius : group->cemetery ? 30 : 24;
                if (Vector2DistanceSqr(next, playerCenter) <= radius * radius) {
                    if (group->cemetery || group->boneland) AddEnemyImpact(group,next,shot->direction);
                    if (clearing->templarField)
                        HurtByTemplar(group, player, playerHealth, playerDodging, shot->damage);
                    else if (!playerDodging && !(shot->boneShot && group->practiceInvincible) &&
                             group->playerHitCooldown <= 0.0f && *playerHealth > 0) {
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
        if (shot->umbraSpear) {
            Texture2D art = group->igrisArt[IGRIS_SPEARS_FX];
            float age = UMBRA_SPEAR_LIFETIME-shot->remaining;
            int frame = age < .14f ? 1 : shot->remaining < .12f ? 5 : 2+(int)(age*16)%3;
            float w = art.width/6.0f, h = art.height/4.0f;
            float angle = atan2f(shot->direction.y,shot->direction.x)*RAD2DEG;
            DrawTexturePro(art,(Rectangle){frame*w,h+h*.425f,w,h*.18f},
                           (Rectangle){shot->position.x,shot->position.y,142,25},(Vector2){104,12.5f},angle,WHITE);
            continue;
        }
        if (shot->boneShot) {
            Texture2D sheet = group->boneLordSheets[BONE_RANGED_SHEET];
            float age = BONE_SHOT_LIFETIME - shot->remaining;
            int width = sheet.width / 8;
            int height = sheet.height / 3;
            int frame = 6 + ((int)(age * 17) % 2);
            float angle = atan2f(shot->direction.y, shot->direction.x) * RAD2DEG;
            DrawTexturePro(sheet, (Rectangle){frame * width, height, width, height},
                (Rectangle){shot->position.x, shot->position.y, 99, 88},
                (Vector2){49.5f, 44}, angle, WHITE);
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

static void DrawIgrisCell(Texture2D art, int columns, int rows, int col, int row,
                          Vector2 feet, float height, float anchorX, float anchorY,
                          bool flip, float tilt, Color tint) {
    float w = (float)art.width/columns, h = (float)art.height/rows;
    float width = height*w/h;
    DrawTexturePro(art,(Rectangle){col*w,row*h,flip ? -w : w,h},
                   (Rectangle){feet.x,feet.y,width,height},
                   (Vector2){width*anchorX,height*anchorY},tilt,tint);
}

static void DrawIgrisEclipsePose(Texture2D art, int frame, int row, Vector2 feet, Color tint) {
    static const int edges[4][9] = {
        {0,182,360,548,751,974,1188,1363,1536},
        {0,181,367,550,745,973,1180,1362,1536},
        {0,197,395,566,766,1012,1203,1371,1536},
        {0,170,352,527,715,929,1155,1354,1536},
    };
    static const int body[4][8] = {
        {100,269,454,639,844,1062,1272,1441},
        {101,270,456,622,814,1030,1259,1447},
        {114,287,474,664,883,1080,1280,1471},
        {96,259,431,611,814,1029,1243,1446},
    };
    int left = edges[row][frame], width = edges[row][frame+1]-left;
    const float scale = .80f;
    DrawTexturePro(art,(Rectangle){left,row*256,width,256},
                   (Rectangle){feet.x,feet.y,width*scale,256*scale},
                   (Vector2){(body[row][frame]-left)*scale,239*scale},0,tint);
}

static void DrawIgrisEruption(Texture2D art, int frame, Vector2 center) {
    static const int edges[4][7] = {
        {0,224,448,686,925,1168,1451},
        {0,231,459,695,936,1193,1536},
        {0,242,477,716,956,1216,1536},
        {0,283,525,776,1020,1280,1536},
    };
    static const int top[] = {0,224,501,740};
    static const int ground[] = {214,490,729,970};
    int row = frame/6, col = frame%6;
    int left = edges[row][col], width = edges[row][col+1]-left;
    int height = ground[row]+9-top[row];
    const float scale = .88f;
    DrawTexturePro(art,(Rectangle){left,top[row],width,height},
                   (Rectangle){center.x,center.y,width*scale,height*scale},
                   (Vector2){width*scale*.5f,(height-9)*scale},0,WHITE);
}

static int IgrisRow(int art, int facing) {
    static const int basicRows[] = {0,2,3,1};
    static const int spellRows[] = {0,3,2,1};
    static const int eruptionRows[] = {0,3,1,2};
    if (art == IGRIS_ERUPTION_ART) return eruptionRows[facing];
    if (art >= IGRIS_SPEARS_ART) return spellRows[facing];
    return basicRows[facing];
}

static void DrawTemplarSprite(const EnemyGroup *group, const Enemy *enemy,
                              Vector2 position, int facing, int frame, float tilt, Color tint);

static void DrawIgris(const EnemyGroup *group, const Enemy *enemy) {
    int art = IGRIS_IDLE_ART, columns = 8, facing = enemy->facing;
    if (enemy->igrisFinalForm) {
        art = IGRIS_FINAL_IDLE;
        columns = 4;
    }
    int frame = (int)(enemy->stateTime*7)%columns;
    Vector2 feet = enemy->position;
    if (enemy->state == ENEMY_SPAWNING) {
        if (enemy->stateTime < 0) return;
        float arrival = Clamp(enemy->stateTime/templarArrivalTime,0,1);
        Vector2 landing = {835 + Clamp((feet.x-835)*.26f,-83,83),284};
        Vector2 origin = {landing.x,70};
        feet = arrival < .76f ? Vector2Lerp(origin,landing,arrival/.76f) :
            Vector2Lerp(landing,feet,(arrival-.76f)/.24f);
        Enemy escort = {.templarRole = TEMPLAR_SHIELD, .state = ENEMY_WALK};
        float exit = Clamp((arrival-.53f)/.47f,0,1);
        for (int side = -1; side <= 1; side += 2) {
            Vector2 escortFeet = {feet.x+side*(57+130*exit),feet.y+22+42*exit};
            DrawTemplarSprite(group,&escort,escortFeet,exit > 0 ? (side < 0 ? 2 : 3) : 0,
                              (int)(enemy->stateTime*10)%6,0,Fade(WHITE,1-exit));
        }
        facing = 0;
    }
    if (enemy->state == ENEMY_WALK || enemy->state == ENEMY_SPAWNING) {
        art = enemy->igrisFinalForm ? IGRIS_FINAL_WALK : IGRIS_WALK_ART;
        columns = enemy->igrisFinalForm ? 4 : 7;
        frame = (int)((enemy->state == ENEMY_WALK ? enemy->walkTime : enemy->stateTime)*11)%columns;
    }
    if (enemy->state == ENEMY_ATTACK) {
        int skill = enemy->bossAttack;
        if (skill == IGRIS_ECLIPSE_ECHOES) {
            float t = enemy->stateTime;
            int cel = t < IGRIS_ECHO_PLANT ? 0 : t >= IGRIS_ECHO_FADE ? 5 :
                t < IgrisEchoLaunch(0) ? 1 : t < IgrisEchoLaunch(1) ? 2 : t < IgrisEchoLaunch(2) ? 3 : 4;
            DrawIgrisCell(group->igrisArt[IGRIS_ECHO_ART],6,4,cel,IgrisRow(IGRIS_FINAL_MELEE,facing),
                          feet,238,.5f,.85f,false,0,WHITE);
            return;
        }
        if (skill == IGRIS_Z_RIP) {
            int cel = 0;
            while (cel < 11 && enemy->stateTime >= igrisZFrames[cel+1]) cel++;
            DrawIgrisCell(group->igrisArt[IGRIS_Z_DOWN+facing],4,3,cel%4,cel/4,
                          feet,248,.5f,.88f,false,0,WHITE);
            return;
        }
        if (skill == IGRIS_ABYSS_GRAB) {
            float t = enemy->stateTime;
            int cel = t < .22f ? 0 : t < .40f ? 1 : t < IGRIS_GRAB_IMPACT ? 2 :
                t < .78f ? 3 : t < IGRIS_GRAB_RELEASE ? 4 : 5;
            DrawIgrisCell(group->igrisArt[IGRIS_GRAB_ART],6,4,cel,IgrisRow(IGRIS_FINAL_MELEE,facing),
                          feet,238,.5f,.82f,false,0,WHITE);
            return;
        }
        art = igrisSkills[skill].art;
        columns = skill == IGRIS_FINAL_CLAW ? 4 : skill <= IGRIS_ECLIPSE ? 8 : 6;
        int impactFrame = skill <= IGRIS_ECLIPSE ? 4 :
            (skill == IGRIS_ERUPTION || skill == IGRIS_FINAL_CLAW) ? 2 : 3;
        float t = enemy->stateTime, impact = igrisSkills[skill].impact;
        frame = t < impact ? (int)(t/impact*impactFrame) :
            impactFrame+(int)((t-impact)/(igrisSkills[skill].duration-impact)*(columns-impactFrame));
        frame = (int)Clamp(frame,0,columns-1);
    }
    bool flip = (art <= IGRIS_MELEE_ART && facing == 3) ||
                (art == IGRIS_SPEARS_ART && facing == 2);
    float anchorX = art <= IGRIS_MELEE_ART ? (facing == 2 ? .50f : facing == 3 ? .50f : .61f) : .50f;
    float ground = enemy->igrisFinalForm ? .84f : art == IGRIS_SPEARS_ART && facing == 0 ? .84f : .94f;
    if (enemy->igrisFinalForm && art == IGRIS_FINAL_WALK && facing == 0) ground = .92f;
    bool fallen = enemy->state == ENEMY_FALLEN;
    float fall = fallen ? 1 : enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime/1.2f,0,1) : 0;
    if (fallen) DrawBossGroundSeal(group->sealMarker,feet);
    Color tint = fall > 0 ? Fade(WHITE,.82f) : WHITE;
    if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
        DrawCircleV(feet,52,Fade(PURPLE,.35f+.18f*sinf(enemy->stateTime*17)));
        tint = (Color){255,174,226,255};
    }
    if (art == IGRIS_ECLIPSE_ART) DrawIgrisEclipsePose(group->igrisArt[art],frame,IgrisRow(art,facing),feet,tint);
    else DrawIgrisCell(group->igrisArt[art],columns,4,frame,IgrisRow(art,facing),feet,
                       enemy->igrisFinalForm ? 238 : 190,anchorX,ground,flip,fall*72,tint);
    if (fallen) DrawGameText("E: selar",(int)feet.x-30,(int)feet.y+12,18,GOLD);
}

static void DrawTemplarKnightPose(Texture2D texture, const Enemy *enemy, Vector2 feet,
                                  int facing, int frame, float tilt, Color tint) {
    const int rows[] = {0, 2, 3, 1};
    float size = enemy->templarRole == TEMPLAR_SHIELD ? 145 : 138;
    DrawTexturePro(texture, (Rectangle){frame * 160, rows[facing] * 160, 160, 160},
                   (Rectangle){feet.x, feet.y, size, size},
                   (Vector2){size * .5f, size * (150.0f / 160)}, tilt, tint);
}

static void DrawTemplarAttackPose(const EnemyGroup *group, const Enemy *enemy, Vector2 feet,
                                 int facing, int frame, float tilt, Color tint) {
    Texture2D texture = group->templarAttack[enemy->templarRole];
    if (group->templarIdle[enemy->templarRole].id) {
        DrawTemplarKnightPose(texture, enemy, feet, facing, frame, tilt, tint);
        return;
    }
    float tileW = texture.width / 6.0f, tileH = texture.height / 4.0f;
    float size = templarSizes[enemy->templarRole] * (enemy->templarRole == TEMPLAR_EXECUTIONER ? 1.08f : 1);
    Rectangle source = {frame * tileW, templarRows[facing] * tileH, tileW, tileH};
    DrawTexturePro(texture, source, (Rectangle){feet.x, feet.y, size, size},
                   (Vector2){size * .5f, size * .94f}, tilt, tint);
}

static void DrawTemplarSprite(const EnemyGroup *group, const Enemy *enemy,
                              Vector2 position, int facing, int frame, float tilt, Color tint) {
    if (group->templarIdle[enemy->templarRole].id) {
        bool walking = enemy->state == ENEMY_WALK || enemy->state == ENEMY_SPAWNING;
        Texture2D texture = walking ? group->templarWalk[enemy->templarRole] :
                                     group->templarIdle[enemy->templarRole];
        if (!walking) frame = (int)(enemy->stateTime * 4) % 4;
        DrawTemplarKnightPose(texture, enemy, position, facing, frame, tilt, tint);
        return;
    }
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
    if (role == TEMPLAR_MARSHAL) { DrawIgris(group,enemy); return; }
    if (enemy->state == ENEMY_SPAWNING) {
        if (enemy->stateTime < 0) return;
        if (role == TEMPLAR_STANDARD && enemy->stateTime >= templarArrivalTime) {
            float plant = enemy->stateTime - templarArrivalTime;
            float afterImpact = (plant - templarPlantImpact) / (templarPlantTime - templarPlantImpact);
            int frame = plant < templarPlantImpact ?
                (int)Clamp(3 * plant / templarPlantImpact, 0, 2) :
                3 + (int)Clamp(3 * afterImpact, 0, 2);
            DrawTemplarEffect(group->templarDust, 2, enemy->position, (Vector2){1,0},
                              afterImpact, 110);
            DrawTemplarAttackPose(group, enemy, enemy->position, 0, frame, 0, WHITE);
            DrawTemplarEffect(group->templarEffects, 3, enemy->position, (Vector2){1,0}, afterImpact, 205);
            return;
        }
        float t = Clamp(enemy->stateTime / templarArrivalTime, 0, 1);
        float descent = .52f;
        Vector2 landing = {835 + Clamp((enemy->position.x - 835) * .26f, -83, 83), 284};
        Vector2 origin = {landing.x, 70};
        Vector2 position = t < descent ? Vector2Lerp(origin, landing, t / descent) :
            Vector2Lerp(landing, enemy->position, (t - descent) / (1 - descent));
        int facing = t < descent ? 0 : FacingToward(Vector2Subtract(enemy->position, landing));
        int frame = (int)(enemy->stateTime * 10) % 6;
        DrawTemplarSprite(group, enemy, position, facing, frame, 0, WHITE);
        return;
    }
    float fall = enemy->state == ENEMY_FALLEN ? 1.0f :
                 enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / 1.2f, 0, 1) : 0;
    float walkRate = 9 * enemy->speed / 100;
    int frame = enemy->state == ENEMY_WALK ? (int)(enemy->walkTime * walkRate) % 6 : 0;
    float tilt = (enemy->facing == 2 ? -1.0f : 1.0f) * 72 * fall;
    Color tint = enemy->state == ENEMY_HURT ? (Color){255, 173, 173, 255} :
                 Fade(WHITE, 1 - fall);
    if (enemy->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
        float pulse = .55f + .25f * sinf(17 * enemy->stateTime);
        DrawCircleV(enemy->position, 47, Fade(MAROON, pulse));
        tint = (Color){255, 174, 226, 255};
    }
    if (role == TEMPLAR_STANDARD && enemy->state != ENEMY_DIE) {
        float sway = .8f * sinf(enemy->stateTime * 4.5f);
        DrawTemplarAttackPose(group, enemy, enemy->position, enemy->facing, 5, sway, tint);
        return;
    }
    if (enemy->state == ENEMY_ATTACK) {
        float t = enemy->stateTime;
        float impact = role == TEMPLAR_EXECUTIONER ? executionerImpact :
                       role == TEMPLAR_CROSSBOW ? crossbowRelease : shieldImpact;
        float activeEnd = impact + .17f;
        float duration = role == TEMPLAR_EXECUTIONER ? executionerAttackDuration :
                         role == TEMPLAR_CROSSBOW ? crossbowAttackDuration : shieldAttackDuration;
        float preparation = Clamp(t / impact, 0, 1);
        float followThrough = Clamp((t - impact) / (duration - impact), 0, 1);
        int pose = t < impact ? (preparation < .24f ? 0 : preparation < .64f ? 1 : 2) :
            followThrough < .26f ? 3 : followThrough < .70f ? 4 : 5;
        if (role == TEMPLAR_CROSSBOW)
            pose = t < impact ? (preparation < .35f ? 0 : preparation < .72f ? 1 : 2) : 3;
        Vector2 feet = enemy->position;
        float recoilEnd = role == TEMPLAR_CROSSBOW ? impact + .12f : activeEnd;
        float recoilAge = Clamp((t - impact) / (recoilEnd - impact), 0, 1);
        float recoil = sinf(PI * recoilAge);
        if (role != TEMPLAR_CROSSBOW && t < impact) {
            float brace = sinf(PI * preparation) * (role == TEMPLAR_EXECUTIONER ? 5 : 3);
            feet = Vector2Subtract(feet, Vector2Scale(enemy->attackDirection, brace));
        }
        if (role == TEMPLAR_CROSSBOW) {
            feet = Vector2Subtract(feet,Vector2Scale(enemy->attackDirection,recoil*8));
            if (recoil > 0)
                DrawEmberBurst(Vector2Add(EnemyCenter(enemy),Vector2Scale(enemy->attackDirection,25)),
                               (t-impact)/(activeEnd-impact));
        } else DrawTemplarEffect(group->templarDust,2,feet,(Vector2){1,0},(t-impact)/.43f,98);
        float lean = enemy->facing == 2 ? -recoil : recoil;
        DrawTemplarAttackPose(group,enemy,feet,enemy->facing,pose,4*lean,tint);
        if (role != TEMPLAR_SHIELD) {
            int effect = role == TEMPLAR_CROSSBOW ? 2 : 1;
            Vector2 origin = Vector2Add((Vector2){feet.x,feet.y-24},Vector2Scale(enemy->attackDirection,35));
            DrawTemplarEffect(group->templarEffects,effect,origin,enemy->attackDirection,(t-impact)/.32f,
                              role == TEMPLAR_EXECUTIONER ? 142 : 91);
        }
        return;
    }
    if (enemy->state == ENEMY_WALK) {
        float step = fmodf(enemy->walkTime * walkRate / 3, 1);
        static const Vector2 facingDirection[] = {{0,1},{0,-1},{-1,0},{1,0}};
        Vector2 direction = facingDirection[enemy->facing];
        Vector2 heel = Vector2Subtract(enemy->position, Vector2Scale(direction, 10));
        DrawTemplarEffect(group->templarDust, 0, heel, direction, step,
                          52);
    }
    DrawTemplarSprite(group, enemy, enemy->position, enemy->facing, frame, tilt, tint);
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

static void DrawIgrisEchoes(const EnemyGroup *group, const Enemy *boss) {
    float t = boss->stateTime;
    if (t < IGRIS_ECHO_PLANT) return;
    Texture2D art = group->igrisArt[IGRIS_ECHO_FX];
    float fade = 1-Clamp((t-IGRIS_ECHO_FADE)/(IGRIS_ECHO_END-IGRIS_ECHO_FADE),0,1);
    DrawIgrisCell(art,6,4,t < IGRIS_ECHO_FADE ? 1 : 5,0,boss->position,155,.5f,.65f,false,0,Fade(WHITE,fade));
    for (int i = 0; i < 3; i++) {
        float age = t-IgrisEchoLaunch(i);
        if (age < -IGRIS_ECHO_WARNING) continue;
        Vector2 from = boss->igrisEchoFrom[i], to = boss->igrisEchoTo[i];
        Vector2 direction = Vector2Subtract(to,from);
        int row = IgrisRow(IGRIS_FINAL_MELEE,FacingToward(direction));
        if (age < 0) {
            float opacity = 1+age/IGRIS_ECHO_WARNING;
            DrawLineEx(from,to,2*IGRIS_ECHO_WIDTH,Fade(PURPLE,.15f+.19f*opacity));
            DrawLineEx(from,to,3,Fade(VIOLET,.35f+.55f*opacity));
            DrawIgrisCell(art,6,4,1,row,from,156,.5f,.65f,false,0,WHITE);
            if (opacity > .35f)
                DrawIgrisCell(art,6,4,2,row,from,178,.5f,.90f,false,0,Fade(WHITE,opacity*.8f));
        } else if (age < IGRIS_ECHO_TRAVEL) {
            float progress = age/IGRIS_ECHO_TRAVEL;
            DrawIgrisCell(art,6,4,progress < .58f ? 3 : 4,row,from,178,.5f,.90f,false,0,Fade(WHITE,1-progress));
            Vector2 position = Vector2Lerp(from,to,progress);
            float angle = atan2f(direction.y,direction.x)*RAD2DEG;
            DrawIgrisCell(group->igrisArt[IGRIS_ECLIPSE_FX],8,4,2+(int)(progress*5),1,
                          position,115,.5f,.5f,false,angle,Fade(WHITE,1-.4f*progress));
        } else {
            float dissolve = 1-Clamp((age-IGRIS_ECHO_TRAVEL)/.25f,0,1);
            DrawIgrisCell(art,6,4,5,row,from,156,.5f,.70f,false,0,Fade(WHITE,fmaxf(dissolve,.30f*fade)));
        }
    }
}

static void DrawIgrisWarning(const EnemyGroup *group, const Enemy *boss) {
    if (boss->state != ENEMY_ATTACK) return;
    int skill = boss->bossAttack;
    if (skill == IGRIS_ECLIPSE_ECHOES) {
        DrawIgrisEchoes(group,boss);
        return;
    }
    if (skill == IGRIS_ABYSS_GRAB) {
        float t = boss->stateTime;
        Vector2 end = Vector2Add(boss->position,Vector2Scale(boss->attackDirection,IGRIS_GRAB_REACH));
        if (t < IGRIS_GRAB_IMPACT) {
            DrawLineEx(boss->position,end,2*IGRIS_GRAB_HALF_WIDTH,Fade(PURPLE,.24f));
            DrawLineEx(boss->position,end,4,Fade(VIOLET,.82f));
        }
        if (t >= .40f && t < IGRIS_GRAB_RELEASE) {
            Texture2D art = group->igrisArt[IGRIS_GRAB_FX];
            int frame = t < IGRIS_GRAB_IMPACT ? 2 : t < .79f ? 3 : 4;
            float w = art.width/6.0f, h = art.height/4.0f;
            float angle = atan2f(boss->attackDirection.y,boss->attackDirection.x)*RAD2DEG;
            float length = t < IGRIS_GRAB_IMPACT ? IGRIS_GRAB_REACH :
                Lerp(IGRIS_GRAB_REACH,95,Clamp((t-IGRIS_GRAB_IMPACT)/(IGRIS_GRAB_RELEASE-IGRIS_GRAB_IMPACT),0,1));
            Vector2 anchor = {boss->position.x,boss->position.y-28};
            DrawTexturePro(art,(Rectangle){frame*w,h,w,h},
                           (Rectangle){anchor.x,anchor.y,length,140},(Vector2){0,70},angle,WHITE);
        }
        return;
    }
    if (skill == IGRIS_Z_RIP) {
        if (boss->stateTime < igrisZFrames[1]) {
            for (int leg = 0; leg < 3; leg++) {
                DrawLineEx(boss->igrisZPath[leg],boss->igrisZPath[leg+1],2*IGRIS_Z_RADIUS,Fade(PURPLE,.18f));
                DrawLineEx(boss->igrisZPath[leg],boss->igrisZPath[leg+1],4,Fade(VIOLET,.8f));
            }
            DrawCircleLinesV(boss->igrisZPath[3],104,Fade(VIOLET,.8f));
        } else if (boss->stateTime >= igrisZFrames[10] && boss->stateTime < igrisZFrames[11]) {
            DrawCircleLinesV(boss->position,104,Fade(VIOLET,.55f));
        }
        return;
    }
    float t = boss->stateTime, impact = igrisSkills[skill].impact;
    float radius = igrisSkills[skill].reach;
    Vector2 center = skill == IGRIS_ERUPTION ? boss->attackTarget : boss->position;
    if (t < impact) {
        float opacity = .12f+.22f*t/impact;
        if (skill == IGRIS_SPEARS) {
            Vector2 normal = {-boss->attackDirection.y,boss->attackDirection.x};
            for (int lane = -1; lane <= 1; lane++) {
                Vector2 start = Vector2Add(center,Vector2Scale(normal,lane*42));
                DrawLineEx(start,Vector2Add(start,Vector2Scale(boss->attackDirection,radius)),3,
                           Fade(VIOLET,opacity*2));
            }
        } else if (skill == IGRIS_ERUPTION || skill == IGRIS_VORTEX) {
            DrawCircleV(center,radius,Fade(PURPLE,opacity));
            DrawCircleLinesV(center,radius,Fade(VIOLET,.8f));
        } else {
            float heading = atan2f(boss->attackDirection.y,boss->attackDirection.x)*RAD2DEG;
            float spread = acosf(igrisSkills[skill].arcDot)*RAD2DEG;
            DrawCircleSector(center,radius,heading-spread,heading+spread,30,Fade(PURPLE,opacity));
        }
        return;
    }
    float progress = Clamp((t-impact)/(igrisSkills[skill].duration-impact),0,.999f);
    int facing = boss->facing;
    if (skill == IGRIS_ECLIPSE) {
        center = Vector2Add(center,Vector2Scale(boss->attackDirection,radius*(.42f+.32f*progress)));
        DrawIgrisCell(group->igrisArt[IGRIS_ECLIPSE_FX],8,4,(int)(progress*8),IgrisRow(IGRIS_ECLIPSE_ART,facing),
                      center,145,.5f,.57f,false,0,WHITE);
    } else if (skill == IGRIS_ERUPTION) {
        int frame = (int)(progress*24);
        DrawIgrisEruption(group->igrisArt[IGRIS_ERUPTION_FX],frame,center);
    } else if (skill == IGRIS_VORTEX) {
        DrawIgrisCell(group->igrisArt[IGRIS_VORTEX_FX],6,4,(int)(progress*6),IgrisRow(IGRIS_VORTEX_ART,facing),
                      center,360,.5f,.60f,false,0,WHITE);
    }
}

static void DrawTemplarWarnings(const EnemyGroup *group) {
    for (int i = 0; i < ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->templarRole == TEMPLAR_NONE || !EnemyCanBeHit(enemy)) continue;
        if (enemy->templarRole == TEMPLAR_MARSHAL) {
            DrawIgrisWarning(group,enemy);
        } else if (enemy->templarRole == TEMPLAR_STANDARD) {
            DrawCircleV(enemy->position,220,Fade(GOLD,.08f));
            DrawCircleLinesV(enemy->position,220,Fade(GOLD,.5f));
        } else if (enemy->state == ENEMY_ATTACK) {
            bool shield = enemy->templarRole == TEMPLAR_SHIELD;
            float t = enemy->stateTime;
            if (enemy->templarRole == TEMPLAR_CROSSBOW) {
                if (!enemy->attackConnected) {
                    Vector2 origin = EnemyCenter(enemy);
                    DrawLineEx(origin,Vector2Add(origin,Vector2Scale(enemy->attackDirection,300)),3,Fade(GOLD,.65f));
                }
                continue;
            }
            float windup = shield ? shieldImpact : executionerImpact;
            float heading = atan2f(enemy->attackDirection.y,enemy->attackDirection.x)*RAD2DEG;
            float reach = shield ? ENEMY_ATTACK_RANGE+8 : 106;
            float spread = shield ? 90 : 99;
            if (t < windup) {
                if (!shield) DrawLineEx(enemy->position,Vector2Add(enemy->position,
                    Vector2Scale(enemy->attackDirection,120)),4,Fade(ORANGE,.68f));
                DrawCircleSector(enemy->position,reach,heading-spread,heading+spread,24,
                                 Fade(ORANGE,.12f+.22f*t/windup));
            } else if (t < windup+.20f) {
                float age = (t-windup)/.20f;
                float radius = reach*(.54f+.46f*age);
                float sweep = heading-spread+2*spread*age;
                float start = fmaxf(heading-spread,sweep-spread*.64f);
                float end = fminf(heading+spread,sweep+spread*.64f);
                DrawRing(enemy->position,radius*.72f,radius,start,end,24,Fade(ORANGE,.68f*(1-age)));
                DrawRing(enemy->position,radius-4,radius,start,end,24,Fade(RAYWHITE,.86f*(1-age)));
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

static Rectangle BoneFrame(Texture2D texture, int columns, int rows, int column, int row) {
    int left = column * texture.width / columns;
    int right = (column + 1) * texture.width / columns;
    int top = row * texture.height / rows;
    int bottom = (row + 1) * texture.height / rows;
    return (Rectangle){left, top, right - left, bottom - top};
}

static int BoneFrameAt(float time, float start, float end, int count) {
    return (int)Clamp((time - start) / (end - start) * count, 0, count - 1);
}

static void DrawBoneGroundEffect(Texture2D texture, int columns, int frame, Vector2 center,
                                 float width) {
    int row = frame / columns;
    Rectangle source = BoneFrame(texture, columns, 3, frame % columns, row);
    float height = width * source.height / source.width;
    float ground = columns == 4 ? (float[]){.68f,.72f,.89f}[row] : .92f;
    DrawTexturePro(texture, source, (Rectangle){center.x, center.y, width, height},
                   (Vector2){width * .5f, height * ground}, 0, WHITE);
}

static void DrawBoneLordWarning(const EnemyGroup *group, const Enemy *boss) {
    if (boss->type != ENEMY_BOSS || boss->state != ENEMY_ATTACK) return;
    int attack = boss->bossAttack;
    float t = boss->stateTime;
    float windup = boneAttacks[attack].windup;
    float radius = boneAttacks[attack].radius;
    float strength = .35f + .40f * Clamp(t / windup, 0, 1);
    if (attack == BONE_RAIN || attack == BONE_BLAST) {
        bool rain = attack == BONE_RAIN;
        Vector2 center = rain ? boss->attackTarget : boss->position;
        if (t < windup) {
            DrawCircleV(center, radius, Fade(RED, strength * .19f));
            DrawRing(center, radius - 3, radius, 0, 360, 64, Fade(ORANGE, strength));
        }
        int frame;
        if (rain) {
            frame = t < windup ? BoneFrameAt(t, 0, windup, 8) :
                8 + BoneFrameAt(t, windup, boneAttacks[attack].duration, 16);
            DrawBoneGroundEffect(group->boneLordSheets[BONE_RAIN_FX_SHEET], 8, frame,
                                 center, radius * 2.1f);
        } else {
            frame = t < windup ? BoneFrameAt(t, 0, windup, 4) :
                4 + BoneFrameAt(t, windup, boneAttacks[attack].duration, 8);
            DrawBoneGroundEffect(group->boneLordSheets[BONE_BLAST_FX_SHEET], 4, frame,
                                 center, radius * 2.15f);
            if (t >= windup) {
                float pulse = Clamp((t - windup) / .30f, 0, 1);
                float edge = radius * (.25f + .75f * pulse);
                DrawCircleV(center, edge, Fade(ORANGE, .25f * (1 - pulse)));
                DrawRing(center, edge - 9, edge, 0, 360, 64, Fade(GOLD, .9f * (1 - pulse)));
            }
        }
        return;
    }
    if (t >= windup) return;
    if (attack == BONE_CLAW) {
        float facing = atan2f(boss->attackDirection.y, boss->attackDirection.x) * RAD2DEG;
        float angle = acosf(.45f) * RAD2DEG;
        DrawCircleSector(boss->position, radius, facing - angle, facing + angle, 28, Fade(ORANGE, strength * .38f));
    } else if (attack != BONE_DASH || t >= BONE_DASH_REAPPEAR) {
        float reach = attack == BONE_DASH ? 225 : boneAttacks[attack].range;
        Vector2 end = Vector2Add(boss->position, Vector2Scale(boss->attackDirection, reach));
        DrawLineEx(boss->position, end, 2, Fade(GOLD, strength));
    }
}

static void DrawBoneLord(const EnemyGroup *group, const Enemy *boss) {
    static const int rowForFacing[] = {0, 2, 3, 1};
    static const int idleGround[] = {236, 477, 763, 983};
    static const int walkGround[] = {242, 486, 752, 998};
    static const int clawGround[] = {223, 463, 719, 956};
    int row = rowForFacing[boss->facing];
    int sheet = boss->state == ENEMY_WALK ? BONE_WALK_SHEET : BONE_IDLE_SHEET;
    float t = boss->stateTime;
    int frame = (int)fmaxf(0, (boss->state == ENEMY_WALK ? boss->walkTime : t) *
                                  (boss->state == ENEMY_WALK ? 10 : 5)) % 6;
    int columns = 6, rows = 4;
    int ground = (sheet == BONE_WALK_SHEET ? walkGround : idleGround)[row];
    bool lateral = false;

    if (boss->state == ENEMY_ATTACK) {
        int attack = boss->bossAttack;
        float windup = boneAttacks[attack].windup;
        float duration = boneAttacks[attack].duration;
        if (attack == BONE_CLAW) {
            sheet = BONE_CLAW_SHEET;
            columns = 8;
            ground = clawGround[row];
            frame = t < windup ? BoneFrameAt(t, 0, windup, 4) :
                4 + BoneFrameAt(t, windup, duration, 4);
        } else {
            lateral = true;
            columns = 8;
            rows = 3;
            if (attack == BONE_RANGED) {
                sheet = BONE_RANGED_SHEET;
                frame = t < windup ? BoneFrameAt(t, 0, windup, 13) :
                    t < windup + .10f ? 13 : 16 + BoneFrameAt(t, windup + .10f, duration, 8);
                ground = (int[]){225,464,705}[frame / 8];
            } else if (attack == BONE_RAIN) {
                sheet = BONE_RAIN_SHEET;
                rows = 2;
                frame = t < windup ? BoneFrameAt(t, 0, windup, 8) :
                    8 + BoneFrameAt(t, windup, duration, 8);
                ground = (int[]){349,711}[frame / 8];
            } else if (attack == BONE_BLAST) {
                sheet = BONE_BLAST_SHEET;
                frame = t < windup ? BoneFrameAt(t, 0, windup, 8) :
                    8 + BoneFrameAt(t, windup, duration, 16);
                ground = (int[]){240,481,685}[frame / 8];
            } else {
                sheet = BONE_DASH_SHEET;
                if (t < BONE_DASH_HIDE) {
                    columns = 6;
                    frame = BoneFrameAt(t, 0, BONE_DASH_HIDE, 4);
                    ground = 222;
                } else if (t < BONE_DASH_REAPPEAR) {
                    return;
                } else {
                    columns = 5;
                    bool charging = t >= windup;
                    frame = charging ? 10 + BoneFrameAt(t, windup, duration, 5) :
                        6 + BoneFrameAt(t, BONE_DASH_REAPPEAR, windup, 4);
                    ground = charging ? 692 : 462;
                }
            }
            row = frame / columns;
            frame %= columns;
        }
    }

    Texture2D texture = group->boneLordSheets[sheet];
    Rectangle source = BoneFrame(texture, columns, rows, frame, row);
    float collapse = boss->state == ENEMY_FALLEN ? 1 :
        boss->state == ENEMY_DIE ? Clamp(t / ENEMY_DEFEAT_DURATION, 0, 1) : 0;
    float reveal = boss->state == ENEMY_SPAWNING ? Clamp(t, 0, 1) : 1;
    const float scale = .62f;
    float heightScale = scale * (1 - .48f * collapse);
    float width = source.width * scale;
    Color tint = Fade(collapse > 0 ? (Color){211, 171, 163, 255} : WHITE, reveal);
    if (boss->state == ENEMY_FALLEN) DrawBossGroundSeal(group->sealMarker, boss->position);
    DrawEllipse((int)boss->position.x, (int)boss->position.y - 9, 54, 19, Fade(BLACK, .38f * reveal));
    if (boss->guardianPhase == GUARDIAN_PHASE_TRANSITION) {
        float flare = Clamp(t / GUARDIAN_PHASE_TRANSITION_TIME, 0, 1);
        DrawRing(boss->position, 45 + 72 * flare, 50 + 72 * flare, 0, 360, 36,
                 Fade(ORANGE, 1 - flare));
        DrawEmberBurst(boss->position, flare);
        tint = ColorLerp((Color){255, 165, 120, 255}, WHITE, flare);
    }
    if (lateral && boss->facing == 2) source.width = -source.width;
    DrawTexturePro(texture, source,
        (Rectangle){boss->position.x, boss->position.y + 28 * (1 - reveal), width, source.height * heightScale},
        (Vector2){width * .5f, (ground - source.y) * heightScale},
        (boss->facing == 2 ? -1 : 1) * 14 * collapse, tint);
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
        if (group->boneland) DrawBoneLordWarning(group, &group->list[0]);
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
            if (enemy->type != ENEMY_BOSS)
                DrawEnemyHealthBar(enemy, enemy->position.y - templarSizes[enemy->templarRole] * 1.02f);
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
        if (enemy->health < enemy->maxHealth)
            DrawEnemyHealthBar(enemy, (int)enemy->position.y - (enemy->type == ENEMY_SPECIAL ? 106 : 85));
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
