#include "clearing.h"
#include "run_progress.h"
#include <stddef.h>
#include "raymath.h"
#include <math.h>

static bool ObstacleMayTouch(const MapObstacle *obstacle, Vector2 point, float margin) {
    return !obstacle->hasBounds ||
        (point.x >= obstacle->min.x - margin && point.x <= obstacle->max.x + margin &&
         point.y >= obstacle->min.y - margin && point.y <= obstacle->max.y + margin);
}

static void SetObstacleBounds(MapObstacle *obstacle) {
    obstacle->min = obstacle->max = obstacle->vertices[0];
    for (int i = 1; i < obstacle->count; i++) {
        Vector2 vertex = obstacle->vertices[i];
        obstacle->min = (Vector2){fminf(obstacle->min.x, vertex.x), fminf(obstacle->min.y, vertex.y)};
        obstacle->max = (Vector2){fmaxf(obstacle->max.x, vertex.x), fmaxf(obstacle->max.y, vertex.y)};
    }
    obstacle->hasBounds = true;
}

bool MapHasClearPath(const Clearing *map,Vector2 from,Vector2 to) {
    Vector2 delta=Vector2Subtract(to,from);
    int steps=(int)(Vector2Length(delta)/4)+1;
    for (int i=0;i<=steps;i++)
        if (IsClearingBlocked(map,Vector2Add(from,Vector2Scale(delta,(float)i/steps)))) return false;
    return true;
}

bool IsMapActorBlocked(const Clearing *map, Vector2 point) {
    /* Rounded feet: square corners used to catch on slanted map edges. */
    if (IsClearingBlocked(map,point)) return true;
    for (int i=0;i<16;i++) {
        float angle=(float)i*2*PI/16;
        if (IsClearingBlocked(map,(Vector2){point.x+cosf(angle)*7,point.y+sinf(angle)*7})) return true;
    }
    return false;
}

static void PlayerSlideCandidate(const Clearing *map, Vector2 position, Vector2 movement,
                                 Vector2 candidate, Vector2 *best, float *score) {
    float progress=Vector2DotProduct(movement,candidate);
    if (progress>*score && !IsMapActorBlocked(map,Vector2Add(position,candidate))) {
        *best=candidate;*score=progress;
    }
}

static const float playerSlideRadius = 11;

static void PlayerSlideEdge(const Clearing *map,Vector2 position,Vector2 movement,
                            Vector2 a,Vector2 b,Vector2 *best,float *score) {
    Vector2 edge=Vector2Subtract(b,a);
    float lengthSqr=Vector2LengthSqr(edge);
    if (lengthSqr<.001f) return;
    float along=Clamp(Vector2DotProduct(Vector2Subtract(position,a),edge)/lengthSqr,0,1);
    Vector2 nearest=Vector2Add(a,Vector2Scale(edge,along));
    if (Vector2DistanceSqr(position,nearest)>playerSlideRadius*playerSlideRadius) return;
    /* Project input onto the contacted edge; pushing straight into it stops. */
    Vector2 tangent=Vector2Scale(edge,1/sqrtf(lengthSqr));
    Vector2 slide=Vector2Scale(tangent,Vector2DotProduct(movement,tangent));
    PlayerSlideCandidate(map,position,movement,slide,best,score);
}

static float FindWallContact(const Clearing *map, Vector2 position, Vector2 step) {
    /* Keep the part of the step that stops short of the wall. */
    float low = 0, high = 1;
    for (int i = 0; i < 8; i++) {
        float mid = (low + high) * .5f;
        if (IsMapActorBlocked(map, Vector2Add(position, Vector2Scale(step, mid)))) high = mid;
        else low = mid;
    }
    return low;
}

static Vector2 FindPlayerSlide(const Clearing *map, Vector2 position, Vector2 remaining) {
    Vector2 slide = {0};
    float score = .000001f;
    for (int i = 0; i < map->boundaryCount; i++)
        PlayerSlideEdge(map, position, remaining, map->boundary[i],
            map->boundary[(i + 1) % map->boundaryCount], &slide, &score);
    for (int i = 0; i < map->shapeCount; i++) {
        const MapObstacle *obstacle = &map->shapes[i];
        if (!ObstacleMayTouch(obstacle, position, playerSlideRadius)) continue;
        for (int j = 0; j < obstacle->count; j++)
            PlayerSlideEdge(map, position, remaining, obstacle->vertices[j],
                obstacle->vertices[(j + 1) % obstacle->count], &slide, &score);
    }
    PlayerSlideCandidate(map, position, remaining, (Vector2){remaining.x, 0}, &slide, &score);
    PlayerSlideCandidate(map, position, remaining, (Vector2){0, remaining.y}, &slide, &score);
    return slide;
}

void MoveMapPlayer(const Clearing *map,Vector2 *position,Vector2 delta) {
    if (IsMapActorBlocked(map,*position)) {
        Vector2 freePosition;
        if (!FindFreeMapPosition(map,*position,&freePosition) ||
            Vector2DistanceSqr(*position,freePosition)>24*24) return;
        *position=freePosition;
    }
    float distance=Vector2Length(delta);
    if (distance<.001f) return;
    int steps=(int)ceilf(distance); /* At most one pixel, including during dodge. */
    Vector2 step=Vector2Scale(delta,1.0f/(float)steps);
    for (int i=0;i<steps;i++) {
        Vector2 next=Vector2Add(*position,step);
        if (!IsMapActorBlocked(map,next)) {
            *position = next;
            continue;
        }
        float contact = FindWallContact(map, *position, step);
        *position = Vector2Add(*position, Vector2Scale(step, contact));
        Vector2 slide = FindPlayerSlide(map, *position, Vector2Scale(step, 1 - contact));
        *position = Vector2Add(*position, slide);
    }
}

bool FindFreeMapPosition(const Clearing *map, Vector2 origin, Vector2 *result) {
    if (!IsMapActorBlocked(map,origin)) { *result=origin; return true; }
    for (int radius=4;radius<=256;radius+=4) {
        for (int i=0;i<32;i++) {
            float angle=i*2*PI/32;
            Vector2 next={origin.x+cosf(angle)*radius,origin.y+sinf(angle)*radius};
            if (!IsMapActorBlocked(map,next)) { *result=next; return true; }
        }
    }
    return false;
}

void MoveMapActor(const Clearing *map, Vector2 *position, Vector2 delta) {
    /* Recover actors already embedded by a spawn or knockback. */
    if (IsMapActorBlocked(map,*position) && !FindFreeMapPosition(map,*position,position)) return;
    float length=Vector2Length(delta);
    if (length<.001f) return;
    int steps=(int)(length/2)+1;
    Vector2 step=Vector2Scale(delta,1.0f/steps);
    for (int i=0;i<steps;i++) {
        Vector2 next=Vector2Add(*position,step);
        if (!IsMapActorBlocked(map,next)) { *position=next; continue; }
        /* Search nearby tangents when both axis-only slides hit a jagged mask. */
        bool moved=false;
        for (int angle=15;angle<=90 && !moved;angle+=15) {
            for (int side=-1;side<=1;side+=2) {
                Vector2 tangent=Vector2Rotate(step,side*angle*DEG2RAD);
                next=Vector2Add(*position,tangent);
                if (!IsMapActorBlocked(map,next)) { *position=next; moved=true; break; }
            }
        }
        if (!moved) break;
    }
}

void ConfigureCemeteryBoundary(Clearing *map) {
    const Vector2 outline[] = {{772,92},{892,92},{892,140},{947,174},{985,156},{1158,158},{1175,173},{1177,194},{1195,210},{1243,215},{1269,201},{1325,202},{1351,213},{1357,228},{1376,237},{1367,268},{1383,283},{1414,290},{1428,308},{1414,319},{1405,338},{1388,342},{1355,349},{1322,356},{1309,366},{1313,379},{1345,370},{1368,364},{1392,370},{1413,364},{1455,364},{1473,377},{1667,378},{1667,585},{1586,580},{1505,598},{1485,600},{1478,615},{1466,632},{1446,653},{1420,650},{1418,690},{1409,705},{1404,730},{1411,747},{1425,757},{1431,776},{1428,798},{1415,824},{1401,830},{1390,854},{1257,853},{1187,845},{1095,859},{1025,857},{962,849},{880,848},{800,851},{740,851},{670,861},{610,858},{560,850},{470,845},{420,833},{377,826},{326,824},{283,825},{251,815},{228,799},{205,774},{181,764},{150,755},{129,742},{121,714},{124,686},{116,654},{115,591},{116,551},{129,515},{134,481},{137,443},{128,415},{132,366},{151,337},{178,307},{174,297},{193,277},{201,253},{202,221},{224,205},{270,184},{278,167},{295,163},{447,164},{514,171},{552,177},{591,177},{596,171},{651,174},{690,179},{772,137}};
    map->boundaryCount=(int)(sizeof(outline)/sizeof(outline[0]));
    for (int i=0;i<map->boundaryCount;i++)
        map->boundary[i]=(Vector2){outline[i].x/1668.0f*map->background.width,
            outline[i].y/906.0f*map->background.height};
    const MapObstacle shapes[] = {
        {.vertices={{229,257},{254,245},{269,237},{275,250},{252,263}},.count=5},
        {.vertices={{299,208},{309,201},{333,199},{352,204},{358,213},{341,225},{319,229},{306,221}},.count=8},
        {.vertices={{378,220},{386,214},{401,216},{409,225},{404,238},{393,240},{380,235}},.count=7},
        {.vertices={{132,399},{142,390},{176,381},{188,388},{193,401},{174,411},{148,418}},.count=7},
        {.vertices={{252,350},{264,338},{289,331},{302,332},{312,342},{295,352},{275,360},{260,357}},.count=8},
        {.vertices={{169,442},{188,428},{216,414},{239,427},{267,443},{255,457},{230,467},{199,456}},.count=8},
        {.vertices={{115,564},{129,555},{156,547},{165,556},{167,566},{142,576},{127,578},{116,573}},.count=8},
        {.vertices={{272,564},{289,557},{311,552},{319,560},{301,570},{284,576}},.count=6},
        {.vertices={{135,613},{153,596},{167,595},{221,627},{214,635},{188,639},{163,629}},.count=7},
        {.vertices={{275,735},{286,724},{319,717},{332,722},{324,736},{293,749},{281,745}},.count=7},
        {.vertices={{378,740},{401,729},{413,732},{420,742},{405,752},{391,754}},.count=6},
        {.vertices={{316,750},{337,734},{354,726},{384,741},{403,758},{397,769},{376,783},{346,768}},.count=8},
        {.vertices={{939,769},{959,752},{990,735},{995,722},{1020,724},{1037,731},{1038,768},{1017,783},{1000,789},{962,783}},.count=10},
        {.vertices={{1078,756},{1086,727},{1101,727},{1105,740},{1094,760},{1085,765}},.count=6},
        {.vertices={{1171,789},{1210,770},{1238,754},{1241,735},{1261,739},{1280,746},{1288,765},{1264,783},{1233,799},{1212,810},{1195,804}},.count=11},
        {.vertices={{1105,619},{1118,612},{1135,617},{1153,624},{1148,643},{1126,646},{1105,643}},.count=7},
        {.vertices={{1244,652},{1256,628},{1272,633},{1265,657},{1256,663}},.count=5},
        {.vertices={{1227,296},{1242,289},{1253,298},{1258,334},{1226,337}},.count=5},
        {.vertices={{1274,263},{1291,253},{1314,255},{1322,272},{1324,319},{1314,333},{1288,326},{1267,316}},.count=8},
        {.vertices={{1322,302},{1340,295},{1351,305},{1352,335},{1320,337}},.count=5},
    };
    map->shapeCount=(int)(sizeof(shapes)/sizeof(shapes[0]));
    map->obstacleCount=0;
    for (int i=0;i<map->shapeCount;i++) {
        MapObstacle *obstacle = &map->shapes[i];
        obstacle->count = shapes[i].count;
        for (int j = 0; j < obstacle->count; j++)
            obstacle->vertices[j]=(Vector2){shapes[i].vertices[j].x/1668.0f*map->background.width,
                shapes[i].vertices[j].y/906.0f*map->background.height};
        SetObstacleBounds(obstacle);
    }
}

static bool LoadMapForeground(Clearing *clearing, const char *backgroundPath,
                              const char *maskPath) {
    Image foregroundImage = LoadImage(backgroundPath);
    Image foregroundMask = LoadImage(maskPath);
    if (foregroundImage.data == NULL || foregroundMask.data == NULL ||
        foregroundMask.width != clearing->background.width ||
        foregroundMask.height != clearing->background.height) {
        TraceLog(LOG_ERROR, "Mascara de primeiro plano ausente ou com tamanho diferente do mapa");
        if (foregroundImage.data != NULL) UnloadImage(foregroundImage);
        if (foregroundMask.data != NULL) UnloadImage(foregroundMask);
        return false;
    }
    ImageFormat(&foregroundImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *maskPixels = LoadImageColors(foregroundMask);
    UnloadImage(foregroundMask);
    if (maskPixels == NULL) {
        UnloadImage(foregroundImage);
        return false;
    }
    Color *foregroundPixels = (Color *) foregroundImage.data;
    int count = foregroundImage.width * foregroundImage.height;
    for (int index = 0; index < count; index++) {
        foregroundPixels[index].a = maskPixels[index].a;
    }
    UnloadImageColors(maskPixels);
    clearing->foreground = LoadTextureFromImage(foregroundImage);
    UnloadImage(foregroundImage);
    if (clearing->foreground.id == 0) {
        return false;
    }
    SetTextureFilter(clearing->foreground, TEXTURE_FILTER_POINT);
    return true;
}

bool InitCemetery(Clearing *map) {
    *map = (Clearing){0};
    map->background = LoadTexture("assets/cemiterio.png");
    if (!map->background.id) return false;
    SetTextureFilter(map->background, TEXTURE_FILTER_POINT);
    map->cemetery = true;
    float w = (float)map->background.width, h = (float)map->background.height;
    map->playableBounds = (Rectangle){w*.07f,h*.17f,w*.88f,h*.72f};
    map->spawn = (Vector2){w*.50f,h*.22f};
    map->pedestal = (Vector2){-1000,-1000};
    map->exit = (Rectangle){w*.90f,h*.46f,w*.05f,h*.12f};
    ConfigureCemeteryBoundary(map);
    if (!LoadMapForeground(map, "assets/cemiterio.png",
                           "assets/cemiterio_foreground_mask.png")) {
        UnloadClearing(map);
        return false;
    }
    return true;
}

void ConfigureVillageBoundary(Clearing *map) {
    float w = (float)map->background.width, h = (float)map->background.height;
    const float pts[][2] = {
        {0.00f, 0.44f}, {0.15f, 0.44f}, {0.22f, 0.46f}, {0.35f, 0.46f}, {0.40f, 0.36f},
        {0.43f, 0.28f}, {0.46f, 0.24f}, {0.46f, 0.16f}, {0.55f, 0.16f}, {0.55f, 0.24f},
        {0.58f, 0.28f}, {0.62f, 0.42f}, {0.78f, 0.49f}, {0.83f, 0.46f}, {0.88f, 0.52f},
        {0.84f, 0.65f}, {0.79f, 0.77f}, {0.70f, 0.78f}, {0.65f, 0.85f}, {0.58f, 0.85f},
        {0.53f, 0.80f}, {0.42f, 0.80f}, {0.36f, 0.83f}, {0.29f, 0.78f}, {0.24f, 0.69f},
        {0.19f, 0.59f}, {0.12f, 0.55f}, {0.00f, 0.55f}
    };
    map->boundaryCount = (int)(sizeof(pts) / sizeof(pts[0]));
    for (int i = 0; i < map->boundaryCount; i++) {
        map->boundary[i] = (Vector2){pts[i][0] * w, pts[i][1] * h};
    }
}

static bool LoadMapWithCollision(Clearing *map, const char *bgPath, const char *colPath) {
    *map = (Clearing){0};
    map->background = LoadTexture(bgPath);
    if (!map->background.id) return false;
    SetTextureFilter(map->background, TEXTURE_FILTER_POINT);
    Image colImg = LoadImage(colPath);
    if (colImg.data == NULL || colImg.width != map->background.width ||
        colImg.height != map->background.height) {
        if (colImg.data != NULL) UnloadImage(colImg);
        UnloadClearing(map);
        return false;
    }
    map->collisionPixels = LoadImageColors(colImg);
    map->collisionGuide = LoadTextureFromImage(colImg);
    UnloadImage(colImg);
    if (map->collisionPixels == NULL || map->collisionGuide.id == 0) {
        UnloadClearing(map);
        return false;
    }
    SetTextureFilter(map->collisionGuide, TEXTURE_FILTER_POINT);
    map->playableBounds = (Rectangle){0.0f, 0.0f, (float)map->background.width, (float)map->background.height};
    return true;
}

bool InitVillage(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/vila.png", "assets/vila_collision.png")) return false;
    if (!LoadMapForeground(map, "assets/vila.png", "assets/vila_foreground_mask.png")) {
        UnloadClearing(map);
        return false;
    }
    map->village = true;
    float w = (float)map->background.width, h = (float)map->background.height;
    map->spawn = (Vector2){w * 0.08f, h * 0.465f};
    map->pedestal = (Vector2){-1000.0f, -1000.0f};
    map->exit = (Rectangle){w * 0.43f, h * 0.21f, w * 0.14f, h * 0.06f};
    return true;
}

bool InitChapel(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/capela.png", "assets/capela_collision.png")) return false;
    if (!LoadMapForeground(map, "assets/capela.png", "assets/capela_foreground_mask.png")) {
        UnloadClearing(map);
        return false;
    }
    map->chapel = true;
    float w = (float)map->background.width, h = (float)map->background.height;
    map->spawn = (Vector2){w * 0.50f, h * 0.90f};
    map->pedestal = (Vector2){w * 0.50f, h * 0.44f};
    map->exit = (Rectangle){w * 0.18f, h * 0.22f, w * 0.09f, h * 0.09f};
    return true;
}

bool InitChapelBasement(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/capela_porao.png", "assets/capela_porao_collision.png")) return false;
    if (!LoadMapForeground(map, "assets/capela_porao.png", "assets/capela_porao_foreground_mask.png")) {
        UnloadClearing(map);
        return false;
    }
    map->basement = true;
    float w = (float)map->background.width, h = (float)map->background.height;
    map->spawn = (Vector2){w * 0.50f, h * 0.28f};
    map->pedestal = (Vector2){w * 0.50f, h * 0.56f};
    map->exit = (Rectangle){w * 0.455f, h * 0.195f, w * 0.090f, h * 0.065f};
    return true;
}

bool InitInvertedChapel(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/capela_invertida.png", "assets/capela_collision.png")) return false;
    if (!LoadMapForeground(map, "assets/capela_invertida.png", "assets/capela_foreground_mask.png")) {
        UnloadClearing(map);
        return false;
    }
    map->invertedChapel = true;
    float w = (float)map->background.width, h = (float)map->background.height;
    map->spawn = (Vector2){w * 0.50f, h * 0.62f};
    map->pedestal = (Vector2){w * 0.50f, h * 0.44f};
    map->exit = (Rectangle){w * 0.44f, h * 0.88f, w * 0.12f, h * 0.10f};
    return true;
}

bool InitBoneland(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/terra_dos_ossos.png",
                             "assets/terra_dos_ossos_collision.png")) return false;
    if (!LoadMapForeground(map, "assets/terra_dos_ossos.png",
                           "assets/terra_dos_ossos_foreground_mask.png")) {
        UnloadClearing(map);
        return false;
    }
    map->boneland = true;
    map->spawn = (Vector2){835, 842};
    map->pedestal = (Vector2){-1000, -1000};
    map->exit = (Rectangle){765, 32, 140, 75};
    return true;
}

bool InitTemplarField(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/campo_templarios.png",
                             "assets/campo_templarios_collision.png")) return false;
    map->templarField = true;
    map->exitLocked = true;
    map->spawn = (Vector2){835, 853};
    map->pedestal = (Vector2){-1000, -1000};
    map->exit = (Rectangle){690, 0, 290, 273};
    return true;
}

static bool InitNecromancerArena(Clearing *map) {
    if (!LoadMapWithCollision(map, "assets/arena_necromante.png",
                             "assets/arena_necromante_collision.png")) return false;
    map->necromancerArena = true;
    map->spawn = (Vector2){836, 861};
    map->pedestal = (Vector2){836, 480};
    return true;
}

bool InitCampaignMap(Clearing *map, int mapId) {
    switch (mapId) {
        case 0: return InitClearing(map);
        case 1: return InitCemetery(map);
        case 2: return InitVillage(map);
        case 3: return InitChapel(map);
        case 4: return InitChapelBasement(map);
        case 5: return InitInvertedChapel(map);
        case BONELAND_MAP: return InitBoneland(map);
        case TEMPLAR_FIELD_MAP: return InitTemplarField(map);
        case NECROMANCER_MAP: return InitNecromancerArena(map);
        default: return false;
    }
}

int ClearingMapId(const Clearing *map) {
    if (map->necromancerArena) return NECROMANCER_MAP;
    if (map->templarField) return TEMPLAR_FIELD_MAP;
    if (map->boneland) return BONELAND_MAP;
    if (map->invertedChapel) return 5;
    if (map->basement) return 4;
    if (map->chapel) return 3;
    if (map->village) return 2;
    return map->cemetery ? 1 : 0;
}

void ConfigureClearingBoundary(Clearing *map) {
    /* Outline traced from the 1280 by 720 markup. */
    const Vector2 outline[] = {
        {173,214}, {194,197}, {220,190}, {259,193}, {283,187}, {302,178},
        {302,153}, {310,137}, {331,130}, {355,125}, {382,130}, {416,125},
        {443,119}, {469,113}, {491,117}, {513,125}, {534,122}, {556,115},
        {593,115}, {628,117}, {691,118}, {729,120}, {753,128}, {785,134},
        {816,136}, {855,149}, {876,148}, {891,157}, {925,162}, {948,154},
        {959,158}, {952,169}, {975,178}, {997,178}, {1014,171}, {1027,176},
        {1034,190}, {1053,189}, {1074,185}, {1096,195}, {1110,206}, {1121,218},
        {1134,225}, {1154,209}, {1144,221}, {1138,230}, {1132,247},
        {1133,266}, {1140,291}, {1155,307}, {1175,314}, {1201,318}, {1193,342},
        {1183,364}, {1174,372}, {1158,365}, {1144,369}, {1138,382}, {1141,395},
        {1164,406}, {1161,419}, {1158,431}, {1168,440}, {1190,446}, {1212,444},
        {1239,424}, {1248,429}, {1245,446}, {1236,468}, {1258,478}, {1267,491},
        {1268,510}, {1261,535}, {1255,553}, {1229,563}, {1202,565}, {1163,566},
        {1146,564}, {1130,549}, {1119,532}, {1100,524}, {1075,518}, {1052,522},
        {1045,535}, {1031,544}, {1021,566}, {1017,591}, {1022,610}, {1022,622},
        {1003,627}, {979,641}, {954,658}, {928,665}, {906,674}, {898,689},
        {884,694}, {859,696}, {833,702}, {794,706}, {784,702}, {781,688},
        {771,681}, {747,674}, {733,677}, {726,696}, {725,719}, {457,719},
        {454,701}, {444,697}, {435,676}, {423,668}, {404,661}, {389,665},
        {373,669}, {368,653}, {362,631}, {354,614}, {330,603}, {299,594},
        {286,610}, {274,607}, {270,581}, {266,550}, {256,533}, {243,526},
        {228,524}, {218,534}, {215,560}, {212,584}, {201,595}, {171,602},
        {145,602}, {118,593}, {94,582}, {74,567}, {57,549}, {44,530},
        {34,500}, {30,474}, {35,451}, {36,431}, {27,416}, {23,402},
        {37,394}, {58,389}, {75,394}, {89,391}, {100,382}, {96,367},
        {103,350}, {121,347}, {128,336}, {131,321}, {142,308}, {159,303},
        {177,297}, {187,278}, {195,258}, {198,241}, {186,226}, {176,224}
    };
    map->boundaryCount = sizeof(outline)/sizeof(outline[0]);
    for (int i = 0; i < map->boundaryCount; i++)
        map->boundary[i] = (Vector2){outline[i].x/1280.0f*map->background.width,
            outline[i].y/720.0f*map->background.height};
    map->spawn=(Vector2){map->background.width*.5f,map->background.height*.5f};
}

bool InitClearing(Clearing *clearing) {
    clearing->background = LoadTexture("assets/clareira.png");
    if (clearing->background.id == 0) {
        TraceLog(LOG_ERROR, "Nao foi possivel carregar assets/clareira.png");
        return false;
    }

    SetTextureFilter(clearing->background, TEXTURE_FILTER_POINT);

    Image collisionImage = LoadImage("assets/clareira_collision.png");
    if (collisionImage.data == NULL ||
        collisionImage.width != clearing->background.width ||
        collisionImage.height != clearing->background.height) {
        TraceLog(LOG_ERROR, "Mascara de colisao ausente ou com tamanho diferente do mapa");
        if (collisionImage.data != NULL) UnloadImage(collisionImage);
        UnloadTexture(clearing->background);
        clearing->background.id = 0;
        return false;
    }
    clearing->collisionPixels = LoadImageColors(collisionImage);
    clearing->collisionGuide = LoadTextureFromImage(collisionImage);
    UnloadImage(collisionImage);
    if (clearing->collisionPixels == NULL || clearing->collisionGuide.id == 0) {
        TraceLog(LOG_ERROR, "Nao foi possivel preparar a mascara de colisao");
        UnloadClearing(clearing);
        return false;
    }
    SetTextureFilter(clearing->collisionGuide, TEXTURE_FILTER_POINT);

    if (!LoadMapForeground(clearing, "assets/clareira.png",
                           "assets/clareira_foreground_mask.png")) {
        UnloadClearing(clearing);
        return false;
    }

    // A mascara bloqueia as marcas vermelhas; este retangulo fecha o resto da borda.
    clearing->playableBounds = (Rectangle) {
        50.0f, 150.0f,
        (float) clearing->background.width - 100.0f,
        (float) clearing->background.height - 166.0f
    };
    ConfigureClearingBoundary(clearing);
    clearing->pedestal = (Vector2) { 869.0f, 162.0f };
    return true;
}

void DrawClearing(const Clearing *clearing) {
    DrawTexture(clearing->background, 0, 0, WHITE);
}

void DrawClearingAltarGlow(const Clearing *clearing, float time) {
    const Color cyan={101,240,229,255};
    Vector2 sigil = {clearing->pedestal.x, clearing->pedestal.y - 82.0f};
    float pulse = 0.5f + 0.5f*sinf(time*2.4f);
    DrawCircleGradient((int)sigil.x, (int)sigil.y, 70.0f + 8.0f*pulse,
                       Fade(cyan, 0.35f + 0.15f*pulse), Fade(cyan, 0.0f));
    DrawRing(sigil, 33.0f, 35.0f, 0, 360, 40, Fade(cyan, 0.32f + 0.25f*pulse));
    DrawEllipse((int)clearing->pedestal.x, (int)clearing->pedestal.y + 19,
                43.0f, 11.0f, Fade(cyan, 0.16f + 0.10f*pulse));
}

/* Screen-space source rectangles and foot depth in the same reference
   coordinates as the silhouette mask. Each object has its own sort line. */
static const Rectangle clearingOverhangs[] = {
    {26, 392, 165, 69}, {53, 452, 188, 101}, {212, 519, 63, 90},
    {730, 618, 176, 78}, {1014, 510, 145, 94}, {1071, 465, 139, 100}
};

static const Rectangle villageOverhangs[] = {
    {210, 551, 222, 133}, {422, 606, 168, 59},
    {725, 581, 128, 63}, {917, 550, 155, 86}
};

static const Rectangle chapelOverhangs[] = {
    {158, 348, 48, 142}, {154, 490, 51, 151},
    {363, 357, 51, 138}, {357, 492, 51, 150},
    {869, 357, 51, 138}, {873, 492, 51, 151},
    {1073, 348, 51, 143}, {1075, 491, 52, 151},
    {580, 264, 121, 56}
};

static const Rectangle basementOverhangs[] = {
    {249, 155, 61, 71}, {970, 155, 63, 71},
    {248, 451, 63, 63}, {970, 451, 64, 63}
};

static const Rectangle bonelandOverhangs[] = {
    {196, 214, 63, 87}, {1023, 179, 51, 106}, {1040, 390, 55, 90}
};

void DrawClearingForeground(const Clearing *clearing, Vector2 feet) {
    if (!clearing->foreground.id) return;
    if (clearing->cemetery) {
        const float sx = clearing->background.width/1276.0f;
        const float sy = clearing->background.height/720.0f;
        if (feet.y >= 516*sy) return;
        int x = (int)floorf((1058-2)*sx), y = (int)floorf(404*sy);
        Rectangle source = {x, y, ceilf((1176-2)*sx)-x, ceilf(516*sy)-y};
        DrawTextureRec(clearing->foreground, source, (Vector2){x,y}, WHITE);
        return;
    }
    const float sx = clearing->background.width/1280.0f;
    const float sy = clearing->background.height/720.0f;
    const Rectangle *overhangs = clearing->village ? villageOverhangs : clearingOverhangs;
    unsigned count = clearing->village
        ? sizeof(villageOverhangs)/sizeof(villageOverhangs[0])
        : sizeof(clearingOverhangs)/sizeof(clearingOverhangs[0]);
    if (clearing->chapel || clearing->invertedChapel) {
        overhangs = chapelOverhangs;
        count = sizeof(chapelOverhangs)/sizeof(chapelOverhangs[0]);
        /* The last rectangle is the altar, shared only by the normal chapel. */
        if (clearing->invertedChapel) count--;
    } else if (clearing->basement) {
        overhangs = basementOverhangs;
        count = sizeof(basementOverhangs)/sizeof(basementOverhangs[0]);
    } else if (clearing->boneland) {
        overhangs = bonelandOverhangs;
        count = sizeof(bonelandOverhangs)/sizeof(bonelandOverhangs[0]);
    }
    for (unsigned i = 0; i < count; i++) {
        Rectangle part = overhangs[i];
        float depth = part.y + part.height;
        if (feet.y >= depth*sy) continue;
        int x = (int)floorf(part.x*sx), y = (int)floorf(part.y*sy);
        Rectangle source = {x, y, ceilf((part.x+part.width)*sx)-x, ceilf(depth*sy)-y};
        DrawTextureRec(clearing->foreground, source, (Vector2){x,y}, WHITE);
    }
}

void DrawClearingCollisionGuide(const Clearing *clearing) {
    if (!clearing->boundaryCount && clearing->collisionGuide.id)
        DrawTexture(clearing->collisionGuide, 0, 0, Fade(WHITE, .55f));
    for (int i=0;i<clearing->boundaryCount;i++)
        DrawLineEx(clearing->boundary[i],clearing->boundary[(i+1)%clearing->boundaryCount],3,RED);
    for (int i=0;i<clearing->obstacleCount;i++)
        DrawRectangleLinesEx(clearing->obstacles[i],2,RED);
    for (int i=0;i<clearing->shapeCount;i++) {
        const MapObstacle *shape=&clearing->shapes[i];
        for (int j=0;j<shape->count;j++)
            DrawLineEx(shape->vertices[j],shape->vertices[(j+1)%shape->count],2,RED);
    }
    if (clearing->exit.width > 0) DrawRectangleLinesEx(clearing->exit,3,GREEN);
    if (clearing->templarField)
        DrawRectangleLinesEx((Rectangle){700,0,270,150},3,LIME);
    DrawCircleV(clearing->spawn,7,SKYBLUE);
    DrawText("Spawn", (int)clearing->spawn.x+10,(int)clearing->spawn.y,20,SKYBLUE);
    if (!clearing->boundaryCount) DrawRectangleLinesEx(clearing->playableBounds, 3.0f, RED);
    if (clearing->pedestal.x >= 0)
        DrawCircleLines((int) clearing->pedestal.x,
                        (int) clearing->pedestal.y, CLEARING_ALTAR_RADIUS, BLUE);
    if (ClearingMapId(clearing) == 0)
        DrawCircleLines((int)(clearing->background.width*.622f),
                        (int)(clearing->background.height*.228f),22,RED);
}

bool IsClearingBlocked(const Clearing *clearing, Vector2 point) {
    if (point.x<0 || point.y<0 || point.x>=clearing->background.width ||
        point.y>=clearing->background.height) return true;
    if (clearing->boundaryCount>0) {
        /* The authored polygon replaces the old painted contour entirely. */
        if (!CheckCollisionPointPoly(point,clearing->boundary,clearing->boundaryCount)) return true;
        for (int i = 0; i < clearing->shapeCount; i++) {
            const MapObstacle *obstacle = &clearing->shapes[i];
            if (ObstacleMayTouch(obstacle, point, 0) &&
                CheckCollisionPointPoly(point, obstacle->vertices, obstacle->count)) return true;
        }
        if (clearing->cemetery || clearing->village) return false;
        /* Isolated small altar stone remains solid inside the new perimeter. */
        Vector2 rock={clearing->background.width*.622f,clearing->background.height*.228f};
        return CheckCollisionPointCircle(point,rock,22);
    }
    if (!CheckCollisionPointRec(point, clearing->playableBounds) ||
        (clearing->exitLocked && CheckCollisionPointRec(point, clearing->exit))) return true;
    for (int i=0;i<clearing->obstacleCount;i++)
        if (CheckCollisionCircleRec(point, 8.0f, clearing->obstacles[i])) return true;
    if (!clearing->collisionPixels) return false;

    int x = (int) point.x;
    int y = (int) point.y;
    int width = clearing->background.width;
    int height = clearing->background.height;
    if (x < 0 || y < 0 || x >= width || y >= height) return true;
    return clearing->collisionPixels[y * width + x].a > 0;
}

void UnloadClearing(Clearing *clearing) {
    if (clearing->foreground.id != 0) {
        UnloadTexture(clearing->foreground);
        clearing->foreground.id = 0;
    }
    if (clearing->collisionPixels != NULL) {
        UnloadImageColors(clearing->collisionPixels);
        clearing->collisionPixels = NULL;
    }
    if (clearing->collisionGuide.id != 0) {
        UnloadTexture(clearing->collisionGuide);
        clearing->collisionGuide.id = 0;
    }
    if (clearing->background.id != 0) {
        UnloadTexture(clearing->background);
        clearing->background.id = 0;
    }
}
