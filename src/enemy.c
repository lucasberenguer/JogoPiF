#include "enemy.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define ENEMY_SPEED 98.0f
#define ENEMY_ATTACK_RANGE 64.0f
#define ENEMY_ATTACK_DAMAGE 12
#define ENEMY_HURT_RADIUS 20.0f
#define CLEARING_MELEE_ATTACK_DURATION .62f
#define CLEARING_MELEE_ATTACK_FRAMES 8
#define CLEARING_MELEE_IMPACT_FRAME 4
#define RANGED_MIN_DISTANCE 170.0f
#define RANGED_MAX_DISTANCE 300.0f
#define RANGED_SHOT_SPEED 365.0f
#define ARENA_BOSS_BASE_HEALTH 2200
#define CEMETERY_BOSS_BASE_HEALTH 2420
#define ENEMY_DEFEAT_DURATION .85f
#define GUARDIAN_DEFEAT_FRAMES 8
#define GUARDIAN_SPAWN_FPS 20.0f
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
    return !group->cemetery && enemy->type == ENEMY_BOSS && enemy->state == ENEMY_SPAWNING;
}

static int GuardianSpawnFrame(const Enemy *enemy) {
    return (int)Clamp(enemy->stateTime * GUARDIAN_SPAWN_FPS, 0, GUARDIAN_SPAWN_FRAMES - 1);
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

static int ClearingMeleeFrame(const Enemy *enemy) {
    switch (enemy->state) {
        case ENEMY_IDLE: return (int)(enemy->stateTime * 7.5f) % 5;
        case ENEMY_WALK: return (int)(enemy->walkTime * 11.0f) % 8;
        case ENEMY_ATTACK:
            return (int)fminf(enemy->stateTime / CLEARING_MELEE_ATTACK_DURATION *
                             CLEARING_MELEE_ATTACK_FRAMES, CLEARING_MELEE_ATTACK_FRAMES - 1);
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

static Texture2D LoadClearingMeleeAttack(void) {
    Texture2D source = LoadTexture("assets/clareira_melee_ataque.png");
    if (!source.id) return source;
    Shader shader = LoadShader(NULL, "assets/shaders/clareira_melee.fs");
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
    if (shader.id) UnloadShader(shader);
    UnloadTexture(source);
    return result;
}

bool InitEnemies(EnemyGroup *group) {
    *group = (EnemyGroup){0};
    group->sprites = LoadTexture("assets/inimigo_encapuzado.png");
    group->backSprites = LoadTexture("assets/inimigo_encapuzado_costas.png");
    group->frontSprites = LoadTexture("assets/inimigo_encapuzado_frente.png");
    group->skeleton = LoadTexture("assets/esqueleto.png");
    group->mage = LoadTexture("assets/esqueleto_mago.png");
    group->skeletonFront=LoadTexture("assets/esqueleto_frente.png");
    group->skeletonBack=LoadTexture("assets/esqueleto_costas.png");
    group->coveiroWalk=LoadTexture("assets/coveiro_walk_v4.png");
    group->greenFire=LoadTexture("assets/mago_fogo_v2.png");
    group->boss[0] = LoadTexture("assets/coveiro_slam_v3.png");
    group->boss[1] = LoadTexture("assets/coveiro_throw_v4.png");
    group->boss[2] = LoadTexture("assets/coveiro_charge_v3.png");
    group->effects[0]=LoadTexture("assets/coveiro_efeito_raizes.png");
    group->effects[1]=LoadTexture("assets/coveiro_pa_vento_v4.png");
    group->effects[2]=LoadTexture("assets/coveiro_efeito_investida.png");
    if (!group->skeletonFront.id || !group->skeletonBack.id || !group->coveiroWalk.id || !group->greenFire.id ||
        !group->skeleton.id || !group->mage.id || !group->boss[0].id ||
        !group->boss[1].id || !group->boss[2].id || !group->effects[0].id ||
        !group->effects[1].id || !group->effects[2].id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->skeleton, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->mage, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->skeletonFront,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->skeletonBack,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->coveiroWalk,TEXTURE_FILTER_POINT);
    SetTextureFilter(group->greenFire,TEXTURE_FILTER_POINT);
    for (int i=0;i<3;i++) SetTextureFilter(group->boss[i], TEXTURE_FILTER_POINT);
    for (int i=0;i<3;i++) SetTextureFilter(group->effects[i], TEXTURE_FILTER_POINT);
    group->rangedSprites = LoadTexture("assets/inimigo_ranged.png");
    group->clearingMelee[0] = LoadTexture("assets/clareira_melee_idle.png");
    group->clearingMelee[1] = LoadTexture("assets/clareira_melee_andar.png");
    group->clearingMelee[2] = LoadClearingMeleeAttack();
    group->bossSprites = LoadTexture("assets/boss_clareira.png");
    group->guardianSpawn = LoadTexture("assets/boss_clareira_spawn.png");
    group->guardianDefeat = LoadTexture("assets/boss_clareira_derrota.png");
    group->bossWalk = LoadTexture("assets/boss_clareira_andar.png");
    group->bossPortrait = LoadTexture("assets/guardiao_intro_pixel_v2.png");
    group->cemeteryPortrait = LoadTexture("assets/coveiro_intro_pixel.png");
    group->bossAttack = LoadTexture("assets/boss_clareira_especial_1.png");
    group->roots = LoadTexture("assets/boss_clareira_raizes_area.png");
    group->rootWave = LoadTexture("assets/boss_clareira_onda.png");
    group->curse = LoadTexture("assets/boss_clareira_maldicao.png");
    group->guardianMelee = LoadTexture("assets/boss_clareira_corpo_a_corpo.png");
    if (!group->sprites.id || !group->backSprites.id || !group->frontSprites.id ||
        !group->rangedSprites.id || !group->bossSprites.id || !group->bossWalk.id || !group->bossAttack.id ||
        !group->guardianSpawn.id || !group->guardianDefeat.id ||
        !group->clearingMelee[0].id || !group->clearingMelee[1].id || !group->clearingMelee[2].id ||
        !group->roots.id || !group->rootWave.id || !group->curse.id || !group->guardianMelee.id ||
        !group->bossPortrait.id || !group->cemeteryPortrait.id) {
        UnloadEnemies(group);
        return false;
    }
    SetTextureFilter(group->sprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->backSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->frontSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rangedSprites, TEXTURE_FILTER_POINT);
    for (int i=0; i<3; i++) SetTextureFilter(group->clearingMelee[i], TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossSprites, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianSpawn, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianDefeat, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossWalk, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->cemeteryPortrait, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->bossAttack, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->roots, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->rootWave, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->curse, TEXTURE_FILTER_POINT);
    SetTextureFilter(group->guardianMelee, TEXTURE_FILTER_POINT);
    return true;
}

void UnloadEnemies(EnemyGroup *group) {
    if (group->skeleton.id) UnloadTexture(group->skeleton);
    if (group->mage.id) UnloadTexture(group->mage);
    if (group->skeletonFront.id) UnloadTexture(group->skeletonFront);
    if (group->skeletonBack.id) UnloadTexture(group->skeletonBack);
    if (group->coveiroWalk.id) UnloadTexture(group->coveiroWalk);
    if (group->greenFire.id) UnloadTexture(group->greenFire);
    for (int i=0;i<3;i++) if (group->boss[i].id) UnloadTexture(group->boss[i]);
    for (int i=0;i<3;i++) if (group->effects[i].id) UnloadTexture(group->effects[i]);

    if (group->sprites.id) UnloadTexture(group->sprites);
    if (group->backSprites.id) UnloadTexture(group->backSprites);
    if (group->frontSprites.id) UnloadTexture(group->frontSprites);
    if (group->rangedSprites.id) UnloadTexture(group->rangedSprites);
    for (int i=0; i<3; i++) if (group->clearingMelee[i].id) UnloadTexture(group->clearingMelee[i]);
    if (group->bossSprites.id) UnloadTexture(group->bossSprites);
    if (group->guardianSpawn.id) UnloadTexture(group->guardianSpawn);
    if (group->guardianDefeat.id) UnloadTexture(group->guardianDefeat);
    if (group->bossWalk.id) UnloadTexture(group->bossWalk);
    if (group->bossPortrait.id) UnloadTexture(group->bossPortrait);
    if (group->cemeteryPortrait.id) UnloadTexture(group->cemeteryPortrait);
    if (group->bossAttack.id) UnloadTexture(group->bossAttack);
    if (group->roots.id) UnloadTexture(group->roots);
    if (group->rootWave.id) UnloadTexture(group->rootWave);
    if (group->curse.id) UnloadTexture(group->curse);
    if (group->guardianMelee.id) UnloadTexture(group->guardianMelee);
    *group = (EnemyGroup){0};
}

void ResetEnemies(EnemyGroup *group, const Clearing *clearing) {
    StartEnemyWave(group, clearing, 1, clearing->spawn, 1.0f);
}

void ClearEnemyAttacks(EnemyGroup *group) {
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

void StartEnemyWave(EnemyGroup *group, const Clearing *clearing, int wave,
                    Vector2 player, float playerPower) {
    group->cemetery=clearing->cemetery;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    int total = 7 + (wave - 1) * 4;
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
        enemy->health = enemy->maxHealth;
        enemy->speed = ENEMY_SPEED * (1.0f + (wave-1)*.045f);
        enemy->power = 1.0f + (wave-1)*.09f;
        enemy->state = ENEMY_SPAWNING;
        enemy->stateTime = -i*(.30f - (wave-1)*.025f);
        enemy->facing = i%2 ? 2 : 3;
        enemy->lastKatanaSwing = -1;
        enemy->attackCooldown = 0.8f;
    }
}

void StartArenaBoss(EnemyGroup *group, const Clearing *clearing, Vector2 player, float power) {
    group->cemetery=clearing->cemetery;
    ClearEnemyAttacks(group);
    group->playerHitCooldown = 0;
    for (int i=0; i<ENEMY_COUNT; i++) group->list[i] = (Enemy){.state = ENEMY_GONE};
    Enemy *boss = &group->list[0];
    int baseHealth = clearing->cemetery ? CEMETERY_BOSS_BASE_HEALTH : ARENA_BOSS_BASE_HEALTH;
    *boss = (Enemy){.type = ENEMY_BOSS, .state = ENEMY_SPAWNING,
        .position = SpawnPosition(clearing, player), .maxHealth = (int)(baseHealth*fmaxf(1, power)),
        .speed = clearing->cemetery ? 160 : 100, .power = power, .attackCooldown = 1.25f, .lastKatanaSwing = -1,
        .bossAttack = (clearing->cemetery ? COVEIRO_ATTACK_COUNT : GUARDIAN_ATTACK_COUNT) - 1};
    boss->health = boss->maxHealth;
}

int EnemiesRemaining(const EnemyGroup *group) {
    int count = 0;
    for (int i=0; i<ENEMY_COUNT; i++) if (group->list[i].state != ENEMY_GONE) count++;
    return count;
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
    if (!EnemyCanBeHit(enemy) || damage<=0) return;
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
    if (enemy->type == ENEMY_BOSS) distance *= .15f;
    Vector2 away = Vector2Subtract(enemy->position, source);
    if (Vector2LengthSqr(away) < 0.001f) return;
    MoveEnemy(clearing, enemy, Vector2Scale(Vector2Normalize(away), distance));
}

static void FireRangedProjectile(EnemyGroup *group, const Enemy *enemy, Vector2 player) {
    Vector2 origin = EnemyCenter(enemy);
    Vector2 toward = Vector2Subtract((Vector2){player.x,player.y-35},origin);
    if (group->cemetery && Vector2LengthSqr(enemy->attackDirection)>.001f) toward=enemy->attackDirection;
    if (Vector2LengthSqr(toward) < 0.001f) return;
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        if (group->projectiles[i].remaining <= 0.0f) {
            group->projectiles[i] = (EnemyProjectile){origin, Vector2Normalize(toward),
                group->cemetery ? 4.0f : 1.5f,
                (int)((group->cemetery ? 10 : ENEMY_ATTACK_DAMAGE)*enemy->power)};
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

static void AddEnemyImpact(EnemyGroup *group,Vector2 position) {
    for(int i=0;i<16;i++) if(group->impacts[i].remaining<=0) {
        group->impacts[i]=(EnemyImpact){position,.32f};return;
    }
}

void UpdateEnemies(EnemyGroup *group, const Clearing *clearing,
                   Vector2 player, float dt, bool playerDodging,
                   int *playerHealth, int playerMaxHealth) {
    group->playerHitCooldown = fmaxf(0.0f, group->playerHitCooldown-dt);
    for (int i=0;i<16;i++) group->impacts[i].remaining=fmaxf(0,group->impacts[i].remaining-dt);
    if (group->cemetery && group->list[0].type == ENEMY_BOSS &&
        (group->list[0].guardianPhase == GUARDIAN_PHASE_TRANSITION || group->list[0].state == ENEMY_DIE))
        ClearEnemyAttacks(group);
    if (group->cemetery)
        CemeteryUpdateProjectiles(group, clearing, player, dt, playerDodging, playerHealth);
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
                enemy->bossAttack = group->cemetery ? COVEIRO_SHOVEL : GUARDIAN_ROOT_WAVE;
                enemy->attackCooldown = .20f;
                enemy->guardianMeleeCooldown = .90f;
            }
            continue;
        }
        if (enemy->state == ENEMY_SPAWNING) {
            float duration = IsGuardianSpawn(group, enemy)
                ? GUARDIAN_SPAWN_FRAMES / GUARDIAN_SPAWN_FPS : 1.0f;
            if (enemy->stateTime >= duration) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        enemy->attackCooldown = fmaxf(0.0f, enemy->attackCooldown-dt);
        if (enemy->type == ENEMY_BOSS)
            enemy->guardianMeleeCooldown = fmaxf(0.0f, enemy->guardianMeleeCooldown - dt);
        enemy->bibleHitCooldown = fmaxf(0.0f, enemy->bibleHitCooldown-dt);
        if (enemy->state == ENEMY_DIE) {
            if (enemy->type == ENEMY_BOSS) ClearEnemyAttacks(group);
            if (enemy->stateTime >= ENEMY_DEFEAT_DURATION) {
                enemy->state = enemy->type == ENEMY_SPECIAL || enemy->type == ENEMY_BOSS ?
                               ENEMY_FALLEN : ENEMY_GONE;
            }
            continue;
        }
        if (enemy->state == ENEMY_HURT) {
            if (enemy->stateTime >= 0.32f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
            continue;
        }
        if (group->cemetery && enemy->type == ENEMY_BOSS) {
            CemeteryUpdateBoss(group, clearing, enemy, player, dt, playerDodging, playerHealth);
            continue;
        }
        if (enemy->state == ENEMY_ATTACK) {
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
                if (enemy->stateTime >= .5f && enemy->stateTime < 1.0f) {
                    MoveEnemy(clearing, enemy, Vector2Scale(enemy->attackDirection, 360 * dt));
                    if (!enemy->attackConnected && !playerDodging && group->playerHitCooldown <= 0 &&
                        Vector2Distance(player, enemy->position) < 65 && *playerHealth > 0 &&
                        MapHasClearPath(clearing, enemy->position, player)) {
                        *playerHealth = (int)fmaxf(0, *playerHealth - 18*enemy->power);
                        group->playerHitCooldown = .65f;
                        enemy->attackConnected = true;
                    }
                }
                if (enemy->stateTime >= 1.1f) { enemy->state = ENEMY_IDLE; enemy->stateTime = 0; }
                continue;
            }
            bool mage = group->cemetery && enemy->type == ENEMY_COMMON_RANGED;
            bool clearingMelee = !group->cemetery && enemy->type == ENEMY_COMMON_MELEE;
            bool impact = clearingMelee ? ClearingMeleeFrame(enemy) >= CLEARING_MELEE_IMPACT_FRAME :
                          enemy->stateTime >= (mage ? .55f : .32f);
            if (!enemy->attackConnected && impact) {
                enemy->attackConnected = true;
                if (enemy->type == ENEMY_COMMON_RANGED) {
                    FireRangedProjectile(group, enemy, player);
                } else if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0 &&
                    Vector2Distance(enemy->position, player) <= ENEMY_ATTACK_RANGE+8.0f &&
                    MapHasClearPath(clearing, enemy->position, player)) {
                    *playerHealth -= (int)(ENEMY_ATTACK_DAMAGE * enemy->power);
                    if (*playerHealth < 0) *playerHealth = 0;
                    group->playerHitCooldown = 0.65f;
                }
            }
            if (enemy->stateTime >= (mage ? .95f : CLEARING_MELEE_ATTACK_DURATION)) {
                enemy->state = ENEMY_IDLE;
                enemy->stateTime = 0;
            }
            continue;
        }
        if (*playerHealth <= 0) { enemy->state = ENEMY_IDLE; continue; }
        Vector2 toPlayer = Vector2Subtract(player, enemy->position);
        float distance = Vector2Length(toPlayer);
        if (distance > 0.001f) {
            enemy->facing = fabsf(toPlayer.x) > fabsf(toPlayer.y)*0.65f
                ? (toPlayer.x < 0 ? 2 : 3) : (toPlayer.y < 0 ? 1 : 0);
        }
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
            if (distance <= RANGED_MAX_DISTANCE && enemy->attackCooldown <= 0.0f) {
                enemy->state = ENEMY_ATTACK;
                enemy->stateTime = 0.0f;
                enemy->attackConnected = false;
                enemy->attackDirection = Vector2Normalize(toPlayer);
                enemy->attackCooldown = 1.5f / fminf(enemy->power, 1.36f);
            } else if (distance < RANGED_MIN_DISTANCE || distance > RANGED_MAX_DISTANCE) {
                enemy->state = ENEMY_WALK;
                enemy->walkTime += dt;
                Vector2 step = Vector2Scale(Vector2Normalize(toPlayer), enemy->speed * dt);
                MoveEnemy(clearing, enemy, distance < RANGED_MIN_DISTANCE ?
                          Vector2Negate(step) : step);
            } else {
                enemy->state = ENEMY_IDLE;
            }
        } else if (distance <= ENEMY_ATTACK_RANGE && enemy->attackCooldown <= 0.0f) {
            enemy->state = ENEMY_ATTACK;
            enemy->stateTime = 0.0f;
            enemy->attackConnected = false;
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
        float speed = group->cemetery ? 210 : RANGED_SHOT_SPEED;
        Vector2 movement = Vector2Scale(shot->direction, speed * dt);
        int steps = (int)(Vector2Length(movement) / 6.0f) + 1;
        Vector2 step = Vector2Scale(movement, 1.0f / steps);
        for (int j = 0; j < steps && shot->remaining > 0.0f; j++) {
            Vector2 next = Vector2Add(shot->position, step);
            Vector2 collisionPoint=next;
            if (group->cemetery) collisionPoint.y+=35;
            if (IsClearingBlocked(clearing, collisionPoint)) {
                if (group->cemetery) AddEnemyImpact(group,next);
                shot->remaining = 0.0f;
            } else {
                shot->position = next;
                Vector2 playerCenter = {player.x, player.y - 35.0f};
                float radius = group->cemetery ? 30 : 24;
                if (Vector2DistanceSqr(next, playerCenter) <= radius * radius) {
                    if (group->cemetery) AddEnemyImpact(group,next);
                    if (!playerDodging && group->playerHitCooldown <= 0.0f && *playerHealth > 0) {
                        *playerHealth -= shot->damage;
                        if (*playerHealth < 0) *playerHealth = 0;
                        group->playerHitCooldown = 0.65f;
                    }
                    shot->remaining = 0.0f;
                }
            }
        }
        if (shot->remaining > 0.0f) shot->remaining = fmaxf(0.0f, shot->remaining - dt);
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

static void DrawRangedProjectiles(const EnemyGroup *group) {
    if (group->cemetery) for(int i=0;i<16;i++) {
        const EnemyImpact *impact=&group->impacts[i];
        if (impact->remaining<=0) continue;
        Texture2D tex=group->greenFire;float cw=tex.width/4.0f,ch=tex.height/2.0f;
        int frame=(int)((.32f-impact->remaining)/.08f);if(frame>3)frame=3;
        DrawTexturePro(tex,(Rectangle){frame*cw,ch,cw,ch},
            (Rectangle){impact->position.x-35,impact->position.y-35,70,70},(Vector2){0},0,WHITE);
    }
    for (int i = 0; i < ENEMY_PROJECTILE_COUNT; i++) {
        const EnemyProjectile *shot = &group->projectiles[i];
        if (shot->remaining <= 0.0f) continue;
        if (group->cemetery) {
            Texture2D tex=group->greenFire;
            float cw=tex.width/4.0f,ch=tex.height/2.0f;
            int frame=(int)(GetTime()*12)%4;
            float angle=atan2f(shot->direction.y,shot->direction.x)*RAD2DEG;
            DrawTexturePro(tex,(Rectangle){frame*cw,0,cw,ch},
                (Rectangle){shot->position.x,shot->position.y,64,48},(Vector2){45,24},angle,WHITE);
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
        DrawText("E: selar", (int)enemy->position.x - 30, (int)enemy->position.y + 10, 18, GOLD);
}

typedef struct ClearingMeleeRow {
    int top, bottom, ground;
    float scale;
    int edges[9];
    int pivots[8];
} ClearingMeleeRow;

// Os pivôs acompanham o corpo e a linha do chão, mesmo quando o arco se estende para o lado.
static const ClearingMeleeRow clearingMeleeIdleRows[] = {
    {747, 995, 961, .36f, {160,406,657,906,1155,1430}, {278,529,781,1034,1285}},
    {0,   254, 238, .35f, {160,406,657,906,1155,1430}, {279,530,785,1036,1289}},
    {254, 502, 471, .39f, {160,406,657,906,1155,1430}, {277,532,785,1037,1289}},
    {502, 747, 710, .39f, {160,406,657,906,1155,1430}, {280,531,780,1034,1286}},
};

static const ClearingMeleeRow clearingMeleeWalkRows[] = {
    {270, 480, 449, .46f, {157,340,500,660,820,980,1140,1300,1480}, {257,419,579,741,899,1058,1219,1378}},
    {0,   245, 212, .43f, {157,340,500,660,820,980,1140,1300,1480}, {256,417,577,736,895,1056,1215,1376}},
    {515, 730, 698, .44f, {157,340,500,660,820,980,1140,1300,1480}, {265,429,592,750,909,1068,1230,1388}},
    {770, 985, 951, .46f, {157,340,500,660,820,980,1140,1300,1480}, {258,414,575,732,893,1050,1216,1376}},
};

static const ClearingMeleeRow clearingMeleeAttackRows[] = {
    {18,  257, 240, .53f, {150,317,480,629,796,981,1152,1329,1490}, {235,395,550,713,877,1049,1212,1415}},
    {272, 510, 483, .49f, {151,319,485,644,790,990,1144,1327,1490}, {236,397,557,715,881,1056,1224,1415}},
    {751, 960, 936, .49f, {149,319,484,642,799,997,1151,1327,1490}, {237,402,560,718,880,1053,1223,1414}},
};

static void DrawClearingMelee(const EnemyGroup *group, const Enemy *enemy) {
    bool attack = enemy->state == ENEMY_ATTACK;
    bool walk = enemy->state == ENEMY_WALK;
    // A fileira esquerda do ataque muda de lado no meio do golpe.
    bool flip = attack && enemy->facing == 2;
    const ClearingMeleeRow *row = attack ? &clearingMeleeAttackRows[enemy->facing < 2 ? enemy->facing : 2] :
        walk ? &clearingMeleeWalkRows[enemy->facing] : &clearingMeleeIdleRows[enemy->facing];
    int frame = ClearingMeleeFrame(enemy);
    int left = row->edges[frame];
    int right = row->edges[frame + 1];
    float width = right - left;
    float height = row->bottom - row->top;
    Vector2 pivot = {(flip ? right - row->pivots[frame] : row->pivots[frame] - left) * row->scale,
                     (row->ground - row->top) * row->scale};
    Rectangle source = {left, row->top, flip ? -width : width, height};
    Rectangle destination = {enemy->position.x, enemy->position.y, width * row->scale, height * row->scale};
    float fall = enemy->state == ENEMY_DIE ? Clamp(enemy->stateTime / ENEMY_DEFEAT_DURATION, 0, 1) : 0;
    Color tint = enemy->state == ENEMY_HURT ? (Color){255,160,160,255} : Fade(WHITE, 1 - fall);
    DrawTexturePro(group->clearingMelee[attack ? 2 : walk ? 1 : 0], source, destination, pivot,
                   (enemy->facing == 2 ? -1 : 1) * 75.0f * fall, tint);
}

void DrawEnemies(const EnemyGroup *group, Vector2 player, bool foreground) {
    if (group->cemetery) {
        CemeteryDrawEnemies(group,player,foreground);
        if (foreground) DrawRangedProjectiles(group);
        return;
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
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 9.0f,
                        Fade(GOLD, 0.65f));
            DrawCircleV((Vector2){enemy->position.x, enemy->position.y-9}, 6.0f, RED);
        }
        if (enemy->state == ENEMY_GONE) continue;
        if (enemy->state == ENEMY_SPAWNING) {
            if (IsGuardianSpawn(group, enemy)) DrawGuardianSpawn(group->guardianSpawn, enemy);
            else if (enemy->stateTime >= 0) DrawCircleLines((int)enemy->position.x,
                (int)enemy->position.y, 24 + 10*sinf(enemy->stateTime*8), GOLD);
            continue;
        }
        if (enemy->type == ENEMY_BOSS) {
            if (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN) {
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
                            w*GUARDIAN_SPRITE_SCALE,h*GUARDIAN_SPRITE_SCALE}, (Vector2){0},0,WHITE);
            continue;
        }
        int row = enemy->state == ENEMY_WALK ? 1 : enemy->state == ENEMY_ATTACK ? 2 :
                  enemy->state == ENEMY_HURT ? 3 : enemy->state == ENEMY_DIE ? 4 : 0;
        int frame = EnemyAnimationFrame(enemy);
        if (enemy->state == ENEMY_FALLEN) { row=0; frame=0; }
        Texture2D tex = group->sprites;
        if (enemy->type == ENEMY_COMMON_MELEE) {
            DrawClearingMelee(group, enemy);
        } else if (enemy->type == ENEMY_COMMON_RANGED) {
            Rectangle src = {(enemy->facing * 4 + frame) * 80.0f, row * 80.0f,
                             80.0f, 80.0f};
            DrawTexturePro(group->rangedSprites, src,
                (Rectangle){enemy->position.x - 38, enemy->position.y - 75, 76, 76},
                (Vector2){0}, 0, WHITE);
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
        if (enemy->state == ENEMY_FALLEN) DrawText("E: selar",(int)enemy->position.x-30,(int)enemy->position.y+10,16,GOLD);
        if (enemy->type == ENEMY_SPECIAL && enemy->state == ENEMY_ATTACK && enemy->stateTime < .5f)
            DrawCircleLines((int)enemy->position.x,(int)enemy->position.y,55,ORANGE);
        if (EnemyCanBeHit(enemy) && enemy->health < enemy->maxHealth) {
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,48,5,DARKGRAY);
            DrawRectangle((int)enemy->position.x-24,(int)enemy->position.y-85,
                          48*enemy->health/enemy->maxHealth,5,RED);
        }
    }
    if (foreground) DrawRangedProjectiles(group);
}

void DrawEnemyDebug(const EnemyGroup *group) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        const Enemy *enemy = &group->list[i];
        if (enemy->state == ENEMY_GONE) continue;
        Vector2 center = EnemyCenter(enemy);
        DrawCircleLines((int)center.x,(int)center.y,EnemyHurtRadius(enemy),GREEN);
        DrawCircleV(enemy->position,3.0f,YELLOW);
        int frame = IsGuardianSpawn(group, enemy) ? GuardianSpawnFrame(enemy) : EnemyAnimationFrame(enemy);
        if (!group->cemetery && enemy->type == ENEMY_COMMON_MELEE) frame = ClearingMeleeFrame(enemy);
        if (!group->cemetery && enemy->type == ENEMY_BOSS &&
            (enemy->state == ENEMY_DIE || enemy->state == ENEMY_FALLEN))
            frame = GuardianDefeatFrame(enemy);
        DrawText(TextFormat("E%d F%d",enemy->state,frame),
                 (int)enemy->position.x-28,(int)enemy->position.y-97,12,GOLD);
    }
}
