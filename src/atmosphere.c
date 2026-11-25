#include "atmosphere.h"
#include "raymath.h"
#include "rlgl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
__declspec(dllimport) int __stdcall SystemParametersInfoA(unsigned int, unsigned int,
                                                          void *, unsigned int);
#endif

enum { LIGHTS = 12, RAYS = 3, EMITTERS = 6, PARTICLES = 128, NOISE_SIZE = 128 };
enum { VEGETATION = 8, FLAMES = 32, REFLECTIONS = 6 };
enum { FOG = 1, LIGHT = 2, RAY = 4, PARTICLE = 8, SHADOW = 16, TINT = 32,
       WIND = 64, FIRE = 128 };

typedef struct {
    Vector2 at;
    float radius;
    unsigned color;
    float strength;
    int mode;
} Light;

typedef struct {
    Vector2 from, to;
    float width;
    unsigned color;
    float strength;
} LightBeam;

typedef struct {
    int type;
    Rectangle area;
    int count;
    unsigned color;
} Emitter;

typedef struct {
    Vector2 at;
    float width, height;
    unsigned color;
} SurfaceEffect;

typedef struct {
    unsigned fog;
    float mist;
    unsigned tint;
    Vector2 shadow;
    int source;
    Light lights[LIGHTS];
    LightBeam rays[RAYS];
    Emitter emitters[EMITTERS];
    Rectangle vegetation[VEGETATION];
    SurfaceEffect flames[FLAMES], reflections[REFLECTIONS];
    int gustCount;
    unsigned gustColors[2];
    bool sky, centerFog;
} AtmosphereConfig;

#include "atmosphere_config.h"

typedef struct {
    Vector2 origin;
    float age, life, speed, angle;
    unsigned color;
    int type, ray;
} Particle;

static struct {
    Texture2D noise, glow, beam, shadow;
    Shader fogShader, surfaceShader, actorShader;
    int fogClock, fogAspect, fogCenter, fogLayers, fogPass;
    int surfaceSize, surfaceClock, surfaceZones, surfaceKind, surfaceRegion;
    int actorLights, actorColors, actorCount;
    AtmosphereConfig config;
    Particle particles[PARTICLES];
    int count, map, quality, flags, selected, bossPhase;
    float time;
    uint32_t random;
    bool debug, reduced, dragging, guides;
} atmosphere;

static Color Paint(unsigned rgb, float opacity) {
    return (Color){(unsigned char)(rgb >> 16), (unsigned char)(rgb >> 8),
                   (unsigned char)rgb, (unsigned char)(Clamp(opacity, 0, 1) * 255)};
}

static float RandomUnit(void) {
    atmosphere.random = atmosphere.random * 1664525u + 1013904223u;
    return (float)(atmosphere.random >> 8) / 16777216.0f;
}

static Texture2D MakeTexture(Color *pixels, int size) {
    Texture2D texture = LoadTextureFromImage((Image){pixels, size, size, 1,
                                                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
    if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

static void InitLightingShaders(void) {
    const char *surface =
        "#version 330\n"
        "in vec2 fragTexCoord; in vec4 fragColor; out vec4 finalColor;\n"
        "uniform sampler2D texture0; uniform vec2 dimensions; uniform float clock;\n"
        "uniform vec4 zones[8], region; uniform int kind;\n"
        "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
        "void main(){\n"
        " vec2 uv=fragTexCoord; vec4 base=texture(texture0,uv);\n"
        " if(kind==0){\n"
        "  float weight=0.;\n"
        "  for(int i=0;i<8;i++){ vec4 r=zones[i]; vec2 t=(uv-r.xy)/max(r.zw,vec2(.001));\n"
        "   vec2 edge=smoothstep(vec2(0.),vec2(.15),min(t,1.-t));\n"
        "   weight=max(weight,edge.x*edge.y*(1.-clamp(t.y,0.,1.))); }\n"
        "  float green=smoothstep(.01,.14,base.g-base.r*.72-base.b*.42);\n"
        "  float plant=max(green,.50);\n"
        "  float row=floor(uv.y*dimensions.y/3.);\n"
        "  float wave=sin(clock*2.8+row*.076+uv.x*43.)+.45*sin(clock*1.7-row*.17);\n"
        "  float shift=clamp(floor(wave*4.8*weight*plant+.5),-3.,3.);\n"
        "  finalColor=texture(texture0,uv+vec2(shift/dimensions.x,0.)); return; }\n"
        " vec2 pixel=(floor(uv*dimensions/2.)*2.+1.)/dimensions;\n"
        " vec2 p=(pixel-region.xy)/region.zw;\n"
        " float edge=clamp(1.-length(p),0.,1.);\n"
        " if(kind<=2){\n"
        "  float sheen=.45+.55*smoothstep(.36,.68,max(base.r,max(base.g,base.b)));\n"
        "  if(kind==2) sheen*=.38+.62*smoothstep(.58,.95,hash(floor(pixel*dimensions/4.)+floor(clock*5.)));\n"
        "  finalColor=vec4(base.rgb*fragColor.rgb,fragColor.a*edge*edge*sheen); return; }\n"
        " if(kind==5){\n"
        "  float hot=smoothstep(.16,.55,base.r-base.g*.55-base.b*.27)*smoothstep(.38,.7,base.r);\n"
        "  finalColor=vec4(base.rgb*fragColor.rgb,fragColor.a*edge*hot); return; }\n"
        " float frame=floor(clock); vec2 cell=floor(pixel*dimensions/2.);\n"
        " float noise=hash(vec2(floor(cell.x/(kind==4?2.:1.)),cell.y+frame));\n"
        " float taper=1.-clamp((-p.y+.1)*.72,0.,1.);\n"
        " float flame=1.-abs(p.x+.12*sin(frame+p.y*5.))/max(.11,taper);\n"
        " float heat=clamp(flame*.62+noise*.37+(p.y+.2)*.32,0.,1.);\n"
        " vec3 core=mix(fragColor.rgb,vec3(1.,.96,.80),smoothstep(.53,.92,heat));\n"
        " float envelope=smoothstep(.02,.25,edge);\n"
        " float flicker=kind==4 ? .38+.60*smoothstep(.22,.87,heat) : .22+.75*smoothstep(.39,.81,heat);\n"
        " finalColor=vec4(core,fragColor.a*envelope*flicker);\n"
        "}\n";
    atmosphere.surfaceShader = LoadShaderFromMemory(NULL,surface);
    atmosphere.surfaceSize = GetShaderLocation(atmosphere.surfaceShader,"dimensions");
    atmosphere.surfaceClock = GetShaderLocation(atmosphere.surfaceShader,"clock");
    atmosphere.surfaceZones = GetShaderLocation(atmosphere.surfaceShader,"zones");
    atmosphere.surfaceKind = GetShaderLocation(atmosphere.surfaceShader,"kind");
    atmosphere.surfaceRegion = GetShaderLocation(atmosphere.surfaceShader,"region");

    const char *vertex =
        "#version 330\n"
        "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor; uniform mat4 mvp;\n"
        "out vec2 fragTexCoord, world; out vec4 fragColor;\n"
        "void main(){world=vertexPosition.xy; fragTexCoord=vertexTexCoord;"
        "fragColor=vertexColor; gl_Position=mvp*vec4(vertexPosition,1.);}\n";
    const char *actor =
        "#version 330\n"
        "in vec2 fragTexCoord, world; in vec4 fragColor; out vec4 finalColor;\n"
        "uniform sampler2D texture0; uniform vec4 colDiffuse, lights[12], colors[12]; uniform int count;\n"
        "void main(){vec4 c=texture(texture0,fragTexCoord)*fragColor*colDiffuse;\n"
        " if(c.a<.01){finalColor=c; return;}\n"
        " float best=0.; vec3 tint=vec3(0.);\n"
        " for(int i=0;i<count;i++){vec4 l=lights[i]; float f=max(0.,1.-length(world-l.xy)/l.z)*l.w;"
        " if(f>best){best=f; tint=colors[i].rgb;}}\n"
        " finalColor=vec4(min(vec3(1.),c.rgb*(1.+tint*best*.48)+tint*best*.035),c.a);}\n";
    atmosphere.actorShader = LoadShaderFromMemory(vertex,actor);
    atmosphere.actorLights = GetShaderLocation(atmosphere.actorShader,"lights");
    atmosphere.actorColors = GetShaderLocation(atmosphere.actorShader,"colors");
    atmosphere.actorCount = GetShaderLocation(atmosphere.actorShader,"count");
}

void InitAtmosphere(void) {
    const char *reduced = getenv("PIF_REDUCED_MOTION");
    memset(&atmosphere, 0, sizeof(atmosphere));
    atmosphere.map = atmosphere.selected = -1;
    atmosphere.quality = 2;
    atmosphere.flags = 255;
#ifdef _WIN32
    int animate = 1;
    if (SystemParametersInfoA(0x1042u, 0, &animate, 0)) atmosphere.reduced = !animate;
#endif
    if (reduced) atmosphere.reduced = strcmp(reduced, "0") != 0;
    Color pixels[NOISE_SIZE * NOISE_SIZE];
    for (int y = 0; y < NOISE_SIZE; y++) for (int x = 0; x < NOISE_SIZE; x++) {
        float u = 2 * PI * x / NOISE_SIZE, v = 2 * PI * y / NOISE_SIZE;
        float n = .49f + .15f*sinf(u+v) + .16f*cosf(2*u-v) +
                  .11f*sinf(3*u+2*v) + .055f*cosf(5*u-3*v);
        unsigned char value = (unsigned char)(Clamp(n, 0, 1) * 255);
        pixels[y*NOISE_SIZE+x] = (Color){value,value,value,255};
    }
    atmosphere.noise = MakeTexture(pixels, NOISE_SIZE);
    if (atmosphere.noise.id) SetTextureWrap(atmosphere.noise, TEXTURE_WRAP_REPEAT);
    for (int kind = 0; kind < 3; kind++) {
        const int size = 64;
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) {
            float u = (x+.5f)/size*2-1, v = (y+.5f)/size*2-1;
            float a = kind == 1
                ? Clamp(1-fabsf(v)/(.32f+.68f*(x+.5f)/size),0,1)*(1-(x+.5f)/size)
                : Clamp(1-sqrtf(u*u+v*v),0,1);
            pixels[y*size+x] = Paint(0xffffff, kind == 2 ? a : a*a);
        }
        Texture2D texture = MakeTexture(pixels, size);
        if (kind == 0) atmosphere.glow = texture;
        if (kind == 1) atmosphere.beam = texture;
        if (kind == 2) atmosphere.shadow = texture;
    }
    const char *fragment =
        "#version 330\n"
        "in vec2 fragTexCoord; in vec4 fragColor; out vec4 finalColor;\n"
        "uniform sampler2D texture0; uniform float clock; uniform vec2 grids;\n"
        "uniform float centerFog; uniform int layers; uniform int pass;\n"
        "void main(){\n"
        " vec2 p=(floor(fragTexCoord*grids)+0.5)/grids;\n"
        " float side=smoothstep(.29,.51,abs(p.x-.5));\n"
        " float edge=max(side,smoothstep(.54,1.0,p.y)*.7);\n"
        " float n=texture(texture0,fract(p*vec2(1.95,2.85)+clock*vec2(.017,.006))).r;\n"
        " if(layers>=2) n=.59*n+.41*texture(texture0,fract(p*vec2(2.6,4.5)+clock*vec2(-.012,.008))).r;\n"
        " if(layers>=3) n=.83*n+.17*texture(texture0,fract(p*vec2(3.2,5.6)+clock*vec2(.008,-.011))).r;\n"
        " float mask=.04+edge*.88+centerFog*exp(-length((p-vec2(.5,.46))*vec2(13.,4.))*2.)*.54;\n"
        " if(pass==1) mask=pow(edge,2.)*.45;\n"
        " if(pass==2) mask=smoothstep(.14,.44,abs(p.x-.5))*(1.-smoothstep(.04,.17,p.y));\n"
        " if(pass==3) mask=1.-smoothstep(.19,.43,length((p-vec2(.51,.51))*vec2(1.2,1.)));\n"
        " if(pass==4) { finalColor=vec4(fragColor.rgb,fragColor.a*edge); return; }\n"
        " finalColor=vec4(fragColor.rgb,fragColor.a*mask*smoothstep(.24,.78,n));\n"
        "}\n";
    atmosphere.fogPass = -1;
    atmosphere.fogShader = LoadShaderFromMemory(NULL, fragment);
    if (atmosphere.fogShader.id) {
        atmosphere.fogClock = GetShaderLocation(atmosphere.fogShader, "clock");
        atmosphere.fogAspect = GetShaderLocation(atmosphere.fogShader, "grids");
        atmosphere.fogCenter = GetShaderLocation(atmosphere.fogShader, "centerFog");
        atmosphere.fogLayers = GetShaderLocation(atmosphere.fogShader, "layers");
        atmosphere.fogPass = GetShaderLocation(atmosphere.fogShader, "pass");
    }
    InitLightingShaders();
}

void UnloadAtmosphere(void) {
    Texture2D textures[] = {atmosphere.noise,atmosphere.glow,atmosphere.beam,atmosphere.shadow};
    for (int i = 0; i < 4; i++) if (textures[i].id) UnloadTexture(textures[i]);
    if (atmosphere.fogShader.id) UnloadShader(atmosphere.fogShader);
    if (atmosphere.surfaceZones >= 0) UnloadShader(atmosphere.surfaceShader);
    if (atmosphere.actorCount >= 0) UnloadShader(atmosphere.actorShader);
}

static const char *ConfigPath(void) {
    return TextFormat("atmosfera-%d.json", atmosphere.map);
}

static void ReloadLights(void) {
    const char *path = ConfigPath();
    if (!FileExists(path)) return;
    char *data = LoadFileText(path);
    if (!data) return;
    Light lights[LIGHTS] = {0};
    int count = 0;
    for (const char *cursor = data; (cursor = strchr(cursor, '{')) && count < LIGHTS; cursor++) {
        if (cursor == data) continue;
        Light light;
        if (sscanf(cursor, "{ \"x\" : %f , \"y\" : %f , \"raio\" : %f , \"cor\" : \"%6x\" , \"intensidade\" : %f , \"modo\" : %d",
                   &light.at.x,&light.at.y,&light.radius,&light.color,&light.strength,&light.mode) != 6 ||
            !(light.at.x >= 0 && light.at.x <= 1 && light.at.y >= 0 && light.at.y <= 1 &&
              light.radius >= .006f && light.radius <= .65f &&
              light.strength >= 0 && light.strength <= .7f && light.mode >= STEADY && light.mode <= PHASE)) {
            UnloadFileText(data);
            return;
        }
        lights[count++] = light;
    }
    if (count) memcpy(atmosphere.config.lights, lights, sizeof(lights));
    UnloadFileText(data);
}

static void ExportLights(void) {
    char data[2300];
    int used = snprintf(data, sizeof(data), "{\n  \"mapa\": %d,\n  \"luzes\": [", atmosphere.map);
    for (int i = 0; i < LIGHTS && atmosphere.config.lights[i].radius > 0; i++) {
        const Light *light = &atmosphere.config.lights[i];
        used += snprintf(data+used, sizeof(data)-used,
            "%s\n    {\"x\":%.4f,\"y\":%.4f,\"raio\":%.4f,\"cor\":\"%06X\",\"intensidade\":%.3f,\"modo\":%d}",
            i ? "," : "", light->at.x, light->at.y, light->radius, light->color,
            light->strength, light->mode);
    }
    snprintf(data+used, sizeof(data)-used, "\n  ]\n}\n");
    SetClipboardText(data);
}

static void SpawnParticle(Particle *p, Rectangle area) {
    p->origin = (Vector2){area.x+RandomUnit()*area.width, area.y+RandomUnit()*area.height};
    p->life = 2.6f + RandomUnit()*5.0f;
    p->speed = .6f+RandomUnit()*.8f;
    p->angle = RandomUnit()*2*PI;
    p->age = 0;
}

static void LoadMapAtmosphere(int map) {
    atmosphere.map = map;
    atmosphere.config = configs[map];
    atmosphere.time = 0;
    atmosphere.count = 0;
    atmosphere.selected = -1;
    atmosphere.dragging = false;
    atmosphere.random = 417u + (unsigned) map * 733u;
    ReloadLights();
    for (int i = 0; i < EMITTERS+RAYS; i++) {
        bool dust = i >= EMITTERS;
        int ray = i-EMITTERS;
        const Emitter *e = dust ? NULL : &atmosphere.config.emitters[i];
        int count = dust ? (atmosphere.config.rays[ray].strength > 0 ? 10 : 0) : e->count;
        for (int j = 0; j < count && atmosphere.count < PARTICLES; j++) {
            Particle *p = &atmosphere.particles[atmosphere.count++];
            *p = (Particle){.type=dust ? DUST : e->type,.ray=dust ? ray : i,
                            .color=dust ? atmosphere.config.rays[ray].color : e->color};
            SpawnParticle(p, dust ? (Rectangle){0,0,1,1} : e->area);
            p->age = RandomUnit()*p->life;
        }
    }
}

static void AdvanceAtmosphere(float dt, bool visible) {
    if (!atmosphere.reduced && visible) {
        atmosphere.time = fmodf(atmosphere.time + dt, 4000.0f);
        for (int i = 0; i < atmosphere.count; i++) {
            Particle *p = &atmosphere.particles[i];
            if ((p->age += dt) >= p->life)
                SpawnParticle(p, p->type == DUST ? (Rectangle){0,0,1,1} :
                                                        atmosphere.config.emitters[p->ray].area);
        }
    }
}

bool UpdateAtmosphere(const Clearing *map, Camera2D camera, float dt) {
    int id = ClearingMapId(map);
    if (atmosphere.map != id) LoadMapAtmosphere(id);
    if (IsKeyPressed(KEY_F7)) {
        atmosphere.debug = !atmosphere.debug;
        atmosphere.dragging = false;
    }
    if (IsKeyPressed(KEY_F8)) atmosphere.quality = (atmosphere.quality+1)%3;
    if (IsKeyPressed(KEY_F9)) atmosphere.reduced = !atmosphere.reduced;
    AdvanceAtmosphere(dt,IsWindowFocused() && !IsWindowMinimized() && !IsWindowHidden());
    if (!atmosphere.debug) return false;
    static const int keys[] = {KEY_ONE,KEY_TWO,KEY_THREE,KEY_FOUR,KEY_FIVE,KEY_SIX,KEY_SEVEN,KEY_EIGHT};
    for (int i = 0; i < 8; i++) if (IsKeyPressed(keys[i])) atmosphere.flags ^= 1 << i;
    if (IsKeyPressed(KEY_TAB)) atmosphere.guides = !atmosphere.guides;
    if (IsKeyPressed(KEY_C)) ExportLights();
    if (IsKeyPressed(KEY_L)) {
        memcpy(atmosphere.config.lights, configs[id].lights, sizeof(atmosphere.config.lights));
        ReloadLights();
        atmosphere.selected = -1;
        atmosphere.dragging = false;
    }
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    Vector2 at = {mouse.x/map->background.width, mouse.y/map->background.height};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        float closest = 24.0f/camera.zoom;
        atmosphere.selected = -1;
        for (int i = 0; i < LIGHTS && atmosphere.config.lights[i].radius > 0; i++) {
            Vector2 light = Vector2Multiply(atmosphere.config.lights[i].at,
                                           (Vector2){map->background.width,map->background.height});
            float distance = Vector2Distance(mouse, light);
            if (distance < closest) {
                closest = distance;
                atmosphere.selected = i;
            }
        }
        atmosphere.dragging = atmosphere.selected >= 0;
    }
    if (atmosphere.selected >= 0) {
        Light *light = &atmosphere.config.lights[atmosphere.selected];
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) atmosphere.dragging = false;
        if (atmosphere.dragging) light->at = Vector2Clamp(at,(Vector2){0,0},(Vector2){1,1});
        if (IsKeyDown(KEY_LEFT_SHIFT))
            light->strength = Clamp(light->strength+GetMouseWheelMove()*.02f,0,.7f);
        else light->radius = Clamp(light->radius+GetMouseWheelMove()*.009f,.006f,.65f);
    }
    return true;
}

static void FogPass(const Clearing *map, int pass, unsigned color, float opacity) {
    if (!atmosphere.noise.id || atmosphere.fogPass < 0 || opacity <= 0) return;
    float grid[2] = {ceilf(map->background.width/4.f),ceilf(map->background.height/4.f)};
    int layers = atmosphere.quality+1;
    float center = atmosphere.config.centerFog ? 1 : 0;
    float time = atmosphere.reduced ? 0 : atmosphere.time;
    if (pass == 2) time *= .35f;
    BeginShaderMode(atmosphere.fogShader);
    SetShaderValue(atmosphere.fogShader, atmosphere.fogClock, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(atmosphere.fogShader, atmosphere.fogAspect, grid, SHADER_UNIFORM_VEC2);
    SetShaderValue(atmosphere.fogShader, atmosphere.fogCenter, &center, SHADER_UNIFORM_FLOAT);
    SetShaderValue(atmosphere.fogShader, atmosphere.fogLayers, &layers, SHADER_UNIFORM_INT);
    SetShaderValue(atmosphere.fogShader, atmosphere.fogPass, &pass, SHADER_UNIFORM_INT);
    DrawTexturePro(atmosphere.noise,(Rectangle){0,0,NOISE_SIZE,NOISE_SIZE},
        (Rectangle){0,0,map->background.width,map->background.height},(Vector2){0},0,Paint(color,opacity));
    EndShaderMode();
}

static void DrawGusts(Vector2 size, bool foreground) {
    int count = atmosphere.config.gustCount;
    float time = atmosphere.time;
    if (!(atmosphere.flags & PARTICLE) || atmosphere.reduced) return;
    if (atmosphere.quality == 0) count = (count+1)/2;
    for (int i = 0; i < count; i++) {
        if ((i%2 != 0) != foreground) continue;
        float speed = .068f+.009f*(i%5);
        float loop = fmodf(time*speed+(i+.5f)/count,1.f);
        float x = (-.055f+loop*1.11f)*size.x;
        float y = (.37f+.24f*((i*7)%11)/10.f+.062f*sinf(time*1.6f+i))*size.y;
        if (x < 3 || x > size.x-7) continue;
        int px=(int)x, py=(int)y, frame=((int)(time*4+i))%4;
        bool leaf = atmosphere.map < 3 || (atmosphere.map == 6 && i%3 == 0);
        Color color = Paint(atmosphere.config.gustColors[i%2],.64f);
        if (!foreground && leaf)
            DrawRectangle(px+2,py+5,4,2,Paint(0x211529,.21f));
        DrawRectangle(px,py,leaf ? 4+(i%3) : 2,frame%2+1,color);
        if (leaf && frame != 1) DrawRectangle(px+2,py+(frame==2?-2:1),2,2,color);
    }
}

void DrawAtmosphereBelow(const Clearing *map) {
    if (!atmosphere.config.mist) return;
    if ((atmosphere.flags & SHADOW) && atmosphere.map == 0)
        FogPass(map,3,0x1a2230,.12f);
    if (atmosphere.flags & FOG)
        FogPass(map,0,atmosphere.config.fog,atmosphere.config.mist);
    DrawGusts((Vector2){map->background.width,map->background.height},false);
}

static void Stamp(Texture2D texture, Vector2 point, float width, float height,
                  float angle, Color tint) {
    if (!texture.id) return;
    DrawTexturePro(texture,(Rectangle){0,0,texture.width,texture.height},
        (Rectangle){point.x,point.y,width,height},(Vector2){width*.5f,height*.5f},angle,tint);
}

static float Flicker(float time, float seed) {
    return .76f+.15f*sinf(time*5.1f+seed)+.09f*sinf(time*13.7f+seed*2);
}

static float LightStrength(const Light *light, float time, int index, int phase) {
    float seed = index * 1.97f;
    switch (light->mode) {
    case PULSE: return light->strength*(.72f+.28f*sinf(time*1.7f+seed));
    case FLICKER: return light->strength*Flicker(time,seed);
    case PHASE: return light->strength*(.40f+.20f*phase+.18f*sinf(time*(1.4f+phase*.9f)+seed));
    default: return light->strength;
    }
}

static unsigned LightColor(const Light *light) {
    if (light->mode != PHASE || atmosphere.bossPhase <= 1) return light->color;
    return atmosphere.bossPhase == 2 ? 0xf75b49 : 0xc77aef;
}

void DrawAtmosphereShadow(const Clearing *map, Vector2 feet, float radius) {
    if (!atmosphere.config.mist || !(atmosphere.flags & SHADOW)) return;
    Vector2 size = {map->background.width,map->background.height};
    Vector2 direction = atmosphere.config.shadow;
    if (atmosphere.config.source == FROM_SOURCE)
        direction = Vector2Normalize(Vector2Subtract(feet,Vector2Multiply(direction,size)));
    float length = atmosphere.map == 0 || atmosphere.map >= 6 ? 3.5f : 2.7f;
    float strongest = 0;
    if (atmosphere.flags & LIGHT) for (int i = 0; i < LIGHTS && atmosphere.config.lights[i].radius > 0; i++) {
        const Light *l = &atmosphere.config.lights[i];
        Vector2 away = Vector2Subtract(feet,Vector2Multiply(l->at,size));
        float distance = Vector2Length(away);
        float relative = distance/(l->radius*size.x);
        float strength = (1-relative)*LightStrength(l,atmosphere.time,i,atmosphere.bossPhase);
        if (distance > 1 && strength > strongest) {
            strongest = strength;
            direction = Vector2Scale(away,1/distance);
            length = 1.8f+2.5f*relative;
        }
    }
    float stretch = radius*length;
    Vector2 center = Vector2Add(feet,Vector2Scale(direction,stretch*.27f));
    Stamp(atmosphere.shadow,center,stretch,radius*1.15f,
          atan2f(direction.y,direction.x)*RAD2DEG,Paint(0x211529,.42f));
}

static Vector2 RayPoint(const LightBeam *ray, Vector2 size, float t, float lateral) {
    Vector2 from = Vector2Multiply(ray->from,size), to = Vector2Multiply(ray->to,size);
    Vector2 axis = Vector2Normalize(Vector2Subtract(to, from));
    return Vector2Add(Vector2Lerp(from,to,t),Vector2Scale((Vector2){-axis.y,axis.x},
        lateral*ray->width*size.x*(.3f+.7f*t)*.45f));
}

static void DrawSurface(const Clearing *map, int kind, Vector2 center, Vector2 radius,
                        float clock, Color color) {
    float region[] = {center.x,center.y,radius.x,radius.y};
    float dimensions[] = {map->background.width,map->background.height};
    float x=floorf(fmaxf(0,(center.x-radius.x)*dimensions[0]));
    float y=floorf(fmaxf(0,(center.y-radius.y)*dimensions[1]));
    Rectangle source={x,y,ceilf(fminf(dimensions[0],(center.x+radius.x)*dimensions[0]))-x,
                           ceilf(fminf(dimensions[1],(center.y+radius.y)*dimensions[1]))-y};
    if (source.width <= 0 || source.height <= 0) return;
    rlDrawRenderBatchActive();
    SetShaderValue(atmosphere.surfaceShader,atmosphere.surfaceKind,&kind,SHADER_UNIFORM_INT);
    SetShaderValue(atmosphere.surfaceShader,atmosphere.surfaceRegion,region,SHADER_UNIFORM_VEC4);
    SetShaderValue(atmosphere.surfaceShader,atmosphere.surfaceClock,&clock,SHADER_UNIFORM_FLOAT);
    DrawTextureRec(map->background,source,(Vector2){x,y},color);
}

void DrawAtmosphereBackground(const Clearing *map, int bossPhase) {
    atmosphere.bossPhase = bossPhase;
    const AtmosphereConfig *config = &atmosphere.config;
    float time = atmosphere.reduced ? 0 : atmosphere.time;
    if (atmosphere.surfaceZones < 0 || !config->mist) {
        DrawClearing(map);
        return;
    }
    Shader shader=atmosphere.surfaceShader;
    float dims[] = {map->background.width,map->background.height};
    DrawClearing(map);
    BeginShaderMode(shader);
    SetShaderValue(shader,atmosphere.surfaceSize,dims,SHADER_UNIFORM_VEC2);
    if ((atmosphere.flags & WIND) && !atmosphere.reduced && config->vegetation[0].width > 0) {
        float zones[VEGETATION][4];
        for (int i=0;i<VEGETATION;i++) {
            Rectangle r=config->vegetation[i];
            zones[i][0]=r.x; zones[i][1]=r.y; zones[i][2]=r.width; zones[i][3]=r.height;
        }
        SetShaderValueV(shader,atmosphere.surfaceZones,zones,SHADER_UNIFORM_VEC4,VEGETATION);
        for (int i=0;i<VEGETATION && config->vegetation[i].width>0;i++) {
            Rectangle r=config->vegetation[i];
            DrawSurface(map,0,(Vector2){r.x+r.width*.5f,r.y+r.height*.5f},
                        (Vector2){r.width*.5f,r.height*.5f},time,WHITE);
        }
    }
    BeginBlendMode(BLEND_ADDITIVE);
    if (atmosphere.flags & LIGHT) {
        for (int i=0;i<LIGHTS && config->lights[i].radius>0;i++) {
            const Light *l=&config->lights[i];
            float scale=l->mode==FLICKER ? .86f+.19f*Flicker(time,i*1.97f) : 1;
            DrawSurface(map,1,l->at,(Vector2){l->radius*scale,l->radius*1.22f*scale},time,
                        Paint(LightColor(l),LightStrength(l,time,i,bossPhase)*2.1f));
        }
        for (int i=0;i<REFLECTIONS && config->reflections[i].width>0;i++) {
            const SurfaceEffect *r=&config->reflections[i];
            bool fractures=atmosphere.map>=6;
            unsigned color=atmosphere.map==8 && i==1 && bossPhase>2 ? 0xc96bf8 : r->color;
            float speed=atmosphere.map==8 ? 1.f+.55f*bossPhase : 1.3f;
            DrawSurface(map,fractures?5:2,r->at,(Vector2){r->width,r->height},time+i,
                        Paint(color,.19f+.15f*Flicker(time*speed,i*2.5f)));
        }
    }
    if (atmosphere.flags & RAY) for (int i=0;i<RAYS && config->rays[i].strength>0;i++) {
        const LightBeam *ray=&config->rays[i];
        if (ray->to.y <= ray->from.y) continue;
        DrawSurface(map,1,ray->to,(Vector2){ray->width*1.4f,ray->width*.9f},time,
                    Paint(ray->color,(.74f+.26f*sinf(time*1.2f+i))*ray->strength*1.8f));
    }
    if ((atmosphere.flags & (LIGHT|FIRE)) == (LIGHT|FIRE)) {
        for (int i=0;i<FLAMES && config->flames[i].width>0;i++) {
            const SurfaceEffect *fire=&config->flames[i];
            float pulse=Flicker(time,9.7f+i*2.31f);
            float scale=fire->width>.01f ? 5.f : 9.f;
            DrawSurface(map,1,fire->at,(Vector2){fire->width*scale*pulse,fire->width*scale*pulse},
                        time,Paint(fire->color,.32f*pulse));
        }
    }
    EndBlendMode();
    if (atmosphere.flags & FIRE) {
        for (int i=0;i<FLAMES && config->flames[i].width>0;i++) {
            const SurfaceEffect *fire=&config->flames[i];
            float pulse=Flicker(time,9.7f+i*2.31f);
            bool forge=fire->width>.01f;
            float frame=floorf(time*(forge?12.f:7.f)+i*3.3f+pulse*2.f);
            DrawSurface(map,forge?4:3,fire->at,
                        (Vector2){fire->width,fire->height*(.78f+.25f*pulse)},frame,
                        Paint(fire->color,forge?.8f:.86f));
        }
    }
    EndShaderMode();
}

void BeginAtmosphereActors(const Clearing *map) {
    if (atmosphere.actorCount < 0 || !(atmosphere.flags&LIGHT)) return;
    float lights[LIGHTS][4], colors[LIGHTS][4];
    float w=map->background.width,h=map->background.height;
    int count=0;
    for (int i=0;i<LIGHTS && atmosphere.config.lights[i].radius>0;i++) {
        const Light *l=&atmosphere.config.lights[i];
        Color c=Paint(LightColor(l),1);
        lights[count][0]=l->at.x*w; lights[count][1]=l->at.y*h;
        lights[count][2]=l->radius*w;
        lights[count][3]=LightStrength(l,atmosphere.time,i,atmosphere.bossPhase);
        colors[count][0]=c.r/255.f; colors[count][1]=c.g/255.f;
        colors[count][2]=c.b/255.f; colors[count][3]=1;
        count++;
    }
    BeginShaderMode(atmosphere.actorShader);
    if (count) {
        SetShaderValueV(atmosphere.actorShader,atmosphere.actorLights,lights,SHADER_UNIFORM_VEC4,count);
        SetShaderValueV(atmosphere.actorShader,atmosphere.actorColors,colors,SHADER_UNIFORM_VEC4,count);
    }
    SetShaderValue(atmosphere.actorShader,atmosphere.actorCount,&count,SHADER_UNIFORM_INT);
}

void EndAtmosphereActors(void) {
    if (atmosphere.actorCount >= 0 && (atmosphere.flags&LIGHT)) EndShaderMode();
}

static void DrawParticles(Vector2 size, bool dustOnly) {
    int stride = 3-atmosphere.quality;
    for (int i = 0; i < atmosphere.count; i += stride) {
        Particle *p = &atmosphere.particles[i];
        if ((p->type == DUST) != dustOnly) continue;
        float age = p->age;
        float t = age/p->life;
        float wave = sinf(p->angle+age*1.1f);
        float alpha = sinf(t*PI)*(.57f+.18f*wave);
        Vector2 at = p->origin;
        int width = 2, height = 2;
        if (p->type == DUST) {
            if (!(atmosphere.flags & RAY)) continue;
            at = RayPoint(&atmosphere.config.rays[p->ray],size,
                          .08f+.82f*p->origin.x+.038f*sinf(age*.74f+p->angle),
                          (p->origin.y*2-1)*.82f+.10f*wave);
        } else {
            if (p->type == EMBER || p->type == WISP || p->type == SMOKE) {
                float direction = p->type == SMOKE && atmosphere.map == 7 ? 1.f : -1.f;
                at.y += direction * age * p->speed * (p->type == EMBER ? .026f : .018f);
            }
            if (p->type == LEAF || p->type == ASH) at.y += age*p->speed*.013f;
            if (p->type != EMBER) at.x += wave*.012f;
            at = Vector2Multiply(at,size);
            if (p->type == LEAF) { width = 5; height = 3; }
            if (p->type == WISP) height = 5;
            if (p->type == SMOKE) {
                float scale = 27 + age*12;
                Stamp(atmosphere.glow,at,scale,scale*.72f,0,Paint(p->color,alpha*.25f));
                continue;
            }
        }
        if (at.x < 0 || at.y < 0 || at.x >= size.x || at.y >= size.y) continue;
        DrawRectangle((int)(at.x*.5f)*2,(int)(at.y*.5f)*2,width,height,Paint(p->color,alpha));
    }
}

void DrawAtmosphereAbove(const Clearing *map) {
    if (!atmosphere.config.mist) return;
    Vector2 size = {map->background.width,map->background.height};
    float time = atmosphere.reduced ? 0 : atmosphere.time;
    if (atmosphere.flags & RAY) {
        BeginBlendMode(BLEND_ADDITIVE);
        if ((atmosphere.flags & RAY) && atmosphere.beam.id) {
            for (int i = 0; i < RAYS && atmosphere.config.rays[i].strength > 0; i++) {
                const LightBeam *ray = &atmosphere.config.rays[i];
                Vector2 from = Vector2Multiply(ray->from,size);
                Vector2 axis = Vector2Subtract(Vector2Multiply(ray->to,size),from);
                float width = ray->width*size.x;
                DrawTexturePro(atmosphere.beam,(Rectangle){0,0,64,64},
                    (Rectangle){from.x,from.y,Vector2Length(axis),width},(Vector2){0,width*.5f},
                    atan2f(axis.y,axis.x)*RAD2DEG,Paint(ray->color,ray->strength*(.83f+.17f*sinf(time*.8f+i))));
            }
        }
        EndBlendMode();
    }
    if (!atmosphere.reduced) {
        if (atmosphere.flags & PARTICLE) DrawParticles(size,false);
        if (atmosphere.flags & RAY) DrawParticles(size,true);
    }
    DrawGusts(size,true);
    if ((atmosphere.flags & FIRE) && !atmosphere.reduced) {
        for (int i=0;i<FLAMES && atmosphere.config.flames[i].width>0;i++) {
            const SurfaceEffect *f=&atmosphere.config.flames[i];
            float age=fmodf(time*.23f+i*.371f,1.f);
            if (age>.34f) continue;
            int x=(int)(f->at.x*size.x+3*sinf(age*7.f+i));
            int y=(int)(f->at.y*size.y-age*50-7);
            DrawRectangle(x,y,2,2,Paint(f->color,.59f*(1-age/.34f)));
            Stamp(atmosphere.glow,(Vector2){x+2,y-7},10,12,0,Paint(0x8d798b,.18f*(1-age/.34f)));
        }
    }
    if ((atmosphere.flags & FOG) && atmosphere.quality > 0) {
        FogPass(map,1,atmosphere.config.fog,atmosphere.config.mist*.55f);
        if (atmosphere.config.sky) FogPass(map,2,atmosphere.config.fog,.13f);
    }
    if (atmosphere.flags & TINT) FogPass(map,4,atmosphere.config.tint,.085f);
}

void DrawAtmosphereGuides(const Clearing *map) {
    if (!atmosphere.debug) return;
    if (atmosphere.guides) DrawClearingCollisionGuide(map);
    for (int i = 0; i < LIGHTS && atmosphere.config.lights[i].radius > 0; i++) {
        const Light *l = &atmosphere.config.lights[i];
        Vector2 at = Vector2Multiply(l->at,(Vector2){map->background.width,map->background.height});
        Color color = i == atmosphere.selected ? YELLOW : SKYBLUE;
        DrawCircleLinesV(at,l->radius*map->background.width,color);
        DrawCircleV(at,5,color);
    }
    Vector2 size={map->background.width,map->background.height};
    for (int i=0;i<FLAMES && atmosphere.config.flames[i].width>0;i++) {
        const SurfaceEffect *f=&atmosphere.config.flames[i];
        Vector2 at=Vector2Multiply(f->at,size);
        DrawRectangleLinesEx((Rectangle){at.x-f->width*size.x,at.y-f->height*size.y,
            f->width*size.x*2,f->height*size.y*2},1,ORANGE);
    }
}

void DrawAtmospherePanel(const Clearing *map) {
    if (!atmosphere.debug) return;
    int x = GetScreenWidth() > 570 ? GetScreenWidth()-561 : 8;
    DrawRectangle(x,10,553,126,Fade((Color){24,17,30,255},.94f));
    DrawText(TextFormat("Atmosfera  |  mapa %d  |  %d FPS  |  %s  |  reduzido %s",
        ClearingMapId(map),GetFPS(),(const char *[]){"baixa","media","alta"}[atmosphere.quality],
        atmosphere.reduced ? "sim" : "nao"),x+10,19,17,RAYWHITE);
    DrawText("1 Nevoa  2 Luz  3 Raios  4 Particulas  5 Sombras  6 Cor  7 Vento  8 Fogo",x+10,42,14,RAYWHITE);
    DrawText(TextFormat("Visivel: %d %d %d %d %d %d %d %d   |   Tab: colisao e transicoes",
        !!(atmosphere.flags&FOG),!!(atmosphere.flags&LIGHT),!!(atmosphere.flags&RAY),
        !!(atmosphere.flags&PARTICLE),!!(atmosphere.flags&SHADOW),!!(atmosphere.flags&TINT),
        !!(atmosphere.flags&WIND),!!(atmosphere.flags&FIRE)),x+10,62,15,RAYWHITE);
    DrawText("Arraste uma luz  |  Roda: raio  |  Shift + roda: intensidade",x+10,82,15,RAYWHITE);
    DrawText("C: copiar JSON  |  L: recarregar  |  F7: fechar  |  F8/F9: opcoes",x+10,102,15,RAYWHITE);
}
