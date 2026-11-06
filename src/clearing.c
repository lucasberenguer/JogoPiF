#include "clearing.h"
#include <stddef.h>
#include "raymath.h"
#include <math.h>

bool MapHasClearPath(const Clearing *map,Vector2 from,Vector2 to) {
    Vector2 delta=Vector2Subtract(to,from);
    int steps=(int)(Vector2Length(delta)/4)+1;
    for (int i=0;i<=steps;i++)
        if (IsClearingBlocked(map,Vector2Add(from,Vector2Scale(delta,(float)i/steps)))) return false;
    return true;
}

bool IsMapActorBlocked(const Clearing *map, Vector2 point) {
    /* Test the feet with clearance, rather than a single pixel on the edge. */
    for (int y=-1;y<=1;y++) for (int x=-1;x<=1;x++)
        if (IsClearingBlocked(map,(Vector2){point.x+x*7,point.y+y*7})) return true;
    return false;
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
        {.vertices={{229,215},{242,205},{263,209},{272,218},{271,242},{281,261},{267,269},{253,277},{232,271},{237,249}},.count=10},
        {.vertices={{304,190},{310,169},{330,166},{346,173},{348,204},{360,222},{355,230},{325,240},{303,233}},.count=9},
        {.vertices={{382,215},{402,213},{410,236},{397,248},{379,242}},.count=5},
        {.vertices={{137,363},{151,353},{175,357},{188,371},{192,402},{198,421},{181,433},{156,433},{128,418}},.count=9},
        {.vertices={{264,317},{289,314},{307,322},{313,359},{287,368},{265,374},{261,352}},.count=7},
        {.vertices={{170,450},{203,428},{229,435},{277,468},{251,480},{219,491},{197,477}},.count=7},
        {.vertices={{121,525},{138,519},{157,532},{158,553},{173,579},{147,590},{117,597}},.count=7},
        {.vertices={{280,542},{299,532},{313,535},{320,550},{321,586},{277,592}},.count=6},
        {.vertices={{137,627},{159,613},{181,623},{229,651},{232,665},{214,679},{194,675},{162,660}},.count=8},
        {.vertices={{279,710},{289,691},{313,682},{331,692},{334,724},{330,750},{311,763},{281,770}},.count=8},
        {.vertices={{381,735},{402,719},{419,725},{429,750},{418,773},{384,768}},.count=6},
        {.vertices={{316,768},{345,748},{381,756},{421,782},{433,797},{410,823},{383,827},{352,812},{327,793}},.count=9},
        {.vertices={{939,785},{969,769},{993,774},{995,740},{1005,711},{1023,708},{1037,721},{1039,755},{1046,777},{1041,797},{1011,820},{995,827},{970,818},{943,799}},.count=14},
        {.vertices={{1064,795},{1086,773},{1089,751},{1082,746},{1085,735},{1103,738},{1114,727},{1124,735},{1125,746},{1108,764},{1100,786},{1089,798}},.count=12},
        {.vertices={{1144,818},{1187,804},{1234,781},{1240,747},{1248,734},{1265,741},{1287,756},{1286,807},{1279,820},{1248,835},{1219,848},{1188,839},{1144,838}},.count=13},
        {.vertices={{1107,614},{1128,604},{1144,613},{1151,634},{1152,664},{1105,660}},.count=6},
        {.vertices={{1244,642},{1255,629},{1267,635},{1279,620},{1290,631},{1287,645},{1297,654},{1291,667},{1274,667},{1268,684},{1247,686},{1251,663},{1243,652}},.count=13},
        {.vertices={{1227,296},{1242,289},{1253,298},{1258,334},{1226,337}},.count=5},
        {.vertices={{1274,263},{1291,253},{1314,255},{1322,272},{1324,319},{1314,333},{1288,326},{1267,316}},.count=8},
        {.vertices={{1322,302},{1340,295},{1351,305},{1352,335},{1320,337}},.count=5},
    };
    map->shapeCount=(int)(sizeof(shapes)/sizeof(shapes[0]));
    map->obstacleCount=0;
    for (int i=0;i<map->shapeCount;i++) {
        map->shapes[i].count=shapes[i].count;
        for (int j=0;j<shapes[i].count;j++)
            map->shapes[i].vertices[j]=(Vector2){shapes[i].vertices[j].x/1668.0f*map->background.width,
                shapes[i].vertices[j].y/906.0f*map->background.height};
    }
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
    return true;
}

void ConfigureClearingBoundary(Clearing *map) {
    const Vector2 outline[] = {
        {190,238},
        {213,214},
        {234,209},
        {281,216},
        {312,211},
        {335,201},
        {331,169},
        {344,151},
        {379,139},
        {414,145},
        {448,139},
        {484,135},
        {512,126},
        {538,129},
        {563,139},
        {588,137},
        {609,129},
        {631,130},
        {661,129},
        {694,132},
        {731,134},
        {769,134},
        {806,135},
        {843,146},
        {870,144},
        {884,155},
        {904,151},
        {940,167},
        {965,162},
        {980,176},
        {1016,181},
        {1030,178},
        {1049,166},
        {1057,171},
        {1052,184},
        {1070,199},
        {1087,198},
        {1104,190},
        {1122,191},
        {1136,201},
        {1138,213},
        {1158,205},
        {1175,212},
        {1194,208},
        {1198,200},
        {1214,208},
        {1240,201},
        {1253,193},
        {1267,213},
        {1280,235},
        {1263,238},
        {1253,264},
        {1244,297},
        {1248,329},
        {1265,345},
        {1290,353},
        {1329,355},
        {1319,381},
        {1310,405},
        {1302,416},
        {1289,407},
        {1274,403},
        {1258,414},
        {1245,436},
        {1250,448},
        {1288,454},
        {1273,471},
        {1272,491},
        {1281,508},
        {1307,507},
        {1340,496},
        {1368,471},
        {1380,478},
        {1375,501},
        {1364,522},
        {1393,532},
        {1400,552},
        {1397,587},
        {1386,619},
        {1356,630},
        {1317,634},
        {1283,635},
        {1259,622},
        {1237,604},
        {1232,591},
        {1211,583},
        {1185,577},
        {1160,582},
        {1154,599},
        {1138,605},
        {1128,626},
        {1122,651},
        {1123,675},
        {1132,688},
        {1128,697},
        {1103,701},
        {1080,719},
        {1053,738},
        {1024,743},
        {1001,754},
        {985,773},
        {958,778},
        {942,774},
        {916,782},
        {885,788},
        {872,783},
        {867,767},
        {848,757},
        {827,750},
        {810,754},
        {801,776},
        {801,803},
        {505,803},
        {505,782},
        {491,780},
        {482,755},
        {464,743},
        {448,738},
        {429,746},
        {416,742},
        {409,717},
        {400,687},
        {383,674},
        {352,663},
        {330,662},
        {317,679},
        {301,679},
        {297,639},
        {290,602},
        {272,587},
        {251,583},
        {240,596},
        {236,631},
        {234,653},
        {225,664},
        {196,668},
        {171,666},
        {148,657},
        {116,651},
        {94,640},
        {81,626},
        {64,613},
        {49,591},
        {36,558},
        {31,529},
        {40,503},
        {40,480},
        {31,456},
        {38,440},
        {62,432},
        {84,440},
        {107,430},
        {119,414},
        {112,390},
        {133,390},
        {141,380},
        {146,355},
        {160,339},
        {180,336},
        {193,340},
        {202,319},
        {211,301},
        {219,281},
        {218,263},
        {206,251}
    };
    map->boundaryCount=(int)(sizeof(outline)/sizeof(outline[0]));
    for (int i=0;i<map->boundaryCount;i++)
        map->boundary[i]=(Vector2){outline[i].x/1413.0f*map->background.width,
            outline[i].y/804.0f*map->background.height};
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

    Image foregroundImage = LoadImage("assets/clareira.png");
    Image foregroundMask = LoadImage("assets/clareira_foreground_mask.png");
    if (foregroundImage.data == NULL || foregroundMask.data == NULL ||
        foregroundMask.width != clearing->background.width ||
        foregroundMask.height != clearing->background.height) {
        TraceLog(LOG_ERROR, "Mascara dos galhos ausente ou com tamanho diferente do mapa");
        if (foregroundImage.data != NULL) UnloadImage(foregroundImage);
        if (foregroundMask.data != NULL) UnloadImage(foregroundMask);
        UnloadClearing(clearing);
        return false;
    }
    ImageFormat(&foregroundImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    clearing->foregroundMaskPixels = LoadImageColors(foregroundMask);
    UnloadImage(foregroundMask);
    if (clearing->foregroundMaskPixels == NULL) {
        UnloadImage(foregroundImage);
        UnloadClearing(clearing);
        return false;
    }
    Color *foregroundPixels = (Color *) foregroundImage.data;
    int count = foregroundImage.width * foregroundImage.height;
    for (int index = 0; index < count; index++) {
        foregroundPixels[index].a = clearing->foregroundMaskPixels[index].a;
    }
    clearing->foreground = LoadTextureFromImage(foregroundImage);
    UnloadImage(foregroundImage);
    if (clearing->foreground.id == 0) {
        UnloadClearing(clearing);
        return false;
    }
    SetTextureFilter(clearing->foreground, TEXTURE_FILTER_POINT);

    // A mascara bloqueia as marcas vermelhas; este retangulo fecha o resto da borda.
    clearing->playableBounds = (Rectangle) {
        50.0f, 150.0f,
        (float) clearing->background.width - 100.0f,
        (float) clearing->background.height - 166.0f
    };
    ConfigureClearingBoundary(clearing);
    /* Bottom dirt opening marked in the map; below the initial spawn. */
    clearing->exit = (Rectangle){clearing->background.width*.36f,
        clearing->background.height*.97f,clearing->background.width*.21f,
        clearing->background.height*.03f};
    clearing->pedestal = (Vector2) { 869.0f, 162.0f };
    return true;
}

void DrawClearing(const Clearing *clearing) {
    DrawTexture(clearing->background, 0, 0, WHITE);
}

void DrawClearingForeground(const Clearing *clearing, Vector2 player) {
    if (!clearing->foregroundMaskPixels) return;
    int x = (int) player.x;
    int y = (int) player.y;
    bool behindBranch = x >= 0 && y >= 0 &&
        x < clearing->background.width && y < clearing->background.height &&
        clearing->foregroundMaskPixels[y * clearing->background.width + x].a > 0;
    int middle = clearing->background.width / 2;
    DrawTextureRec(clearing->foreground,
        (Rectangle) { 0, 0, (float) middle, (float) clearing->background.height },
        (Vector2) { 0, 0 },
        behindBranch && x < middle ? Fade(WHITE, 0.7f) : WHITE);
    DrawTextureRec(clearing->foreground,
        (Rectangle) { (float) middle, 0,
                      (float) (clearing->background.width - middle),
                      (float) clearing->background.height },
        (Vector2) { (float) middle, 0 },
        behindBranch && x >= middle ? Fade(WHITE, 0.7f) : WHITE);
}

void DrawClearingCollisionGuide(const Clearing *clearing) {
    if (!clearing->boundaryCount && clearing->collisionGuide.id) DrawTexture(clearing->collisionGuide, 0, 0, WHITE);
    for (int i=0;i<clearing->boundaryCount;i++)
        DrawLineEx(clearing->boundary[i],clearing->boundary[(i+1)%clearing->boundaryCount],3,RED);
    for (int i=0;i<clearing->obstacleCount;i++)
        DrawRectangleLinesEx(clearing->obstacles[i],2,RED);
    for (int i=0;i<clearing->shapeCount;i++) {
        const MapObstacle *shape=&clearing->shapes[i];
        for (int j=0;j<shape->count;j++)
            DrawLineEx(shape->vertices[j],shape->vertices[(j+1)%shape->count],2,RED);
    }
    DrawRectangleLinesEx(clearing->exit,2,GREEN);
    if (!clearing->boundaryCount) DrawRectangleLinesEx(clearing->playableBounds, 3.0f, RED);
    DrawCircleLines((int) clearing->pedestal.x,
                    (int) clearing->pedestal.y, 72.0f, BLUE);
}

bool IsClearingBlocked(const Clearing *clearing, Vector2 point) {
    if (point.x<0 || point.y<0 || point.x>=clearing->background.width ||
        point.y>=clearing->background.height) return true;
    if (clearing->boundaryCount>0) {
        /* The authored polygon replaces the old painted contour entirely. */
        if (!CheckCollisionPointPoly(point,clearing->boundary,clearing->boundaryCount)) return true;
        for (int i=0;i<clearing->shapeCount;i++)
            if (CheckCollisionPointPoly(point,clearing->shapes[i].vertices,clearing->shapes[i].count)) return true;
        if (clearing->cemetery) return false;
        /* Isolated small altar stone remains solid inside the new perimeter. */
        Vector2 rock={clearing->background.width*.622f,clearing->background.height*.228f};
        return CheckCollisionPointCircle(point,rock,22);
    }
    /* Open only the marked passage through the old bottom boundary/mask. */
    if (!clearing->cemetery && clearing->exit.width > 0 &&
        CheckCollisionPointRec(point,(Rectangle){clearing->exit.x,
            clearing->background.height*.94f,clearing->exit.width,
            clearing->background.height*.06f})) return false;
    if (!CheckCollisionPointRec(point, clearing->playableBounds)) return true;
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
    if (clearing->foregroundMaskPixels != NULL) {
        UnloadImageColors(clearing->foregroundMaskPixels);
        clearing->foregroundMaskPixels = NULL;
    }
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
