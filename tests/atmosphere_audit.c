/* Uses the production renderer with a fixed clock and individual layers.
 * Run CTest atmosphere_audit from a built tree with the copied assets. */
#include "atmosphere.c"

enum { WIDTH = 1280, HEIGHT = 720 };
static const char *output = "audit/atmosfera";
static Texture2D hero;

static Image Render(RenderTexture2D target, const Clearing *map, bool guides, int phase) {
    float zoom=fminf(WIDTH/(float)map->background.width,HEIGHT/(float)map->background.height);
    Camera2D camera={{WIDTH*.5f,HEIGHT*.5f},{map->background.width*.5f,map->background.height*.5f},0,zoom};
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode2D(camera);
    DrawAtmosphereBackground(map,phase);
    DrawAtmosphereBelow(map);
    Vector2 feet={map->background.width*.51f,map->background.height*.48f};
    DrawAtmosphereShadow(map,feet,22);
    BeginAtmosphereActors(map);
    DrawTextureRec(hero,(Rectangle){0,0,64,80},(Vector2){feet.x-32,feet.y-76},WHITE);
    EndAtmosphereActors();
    DrawClearingForeground(map,feet);
    DrawAtmosphereAbove(map);
    if (guides) DrawAtmosphereGuides(map);
    EndMode2D();
    EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    return image;
}

static double Difference(Image a, Image b, Rectangle region) {
    size_t sum=0, count=0;
    int left=(int)fmaxf(0,region.x*WIDTH), top=(int)fmaxf(0,region.y*HEIGHT);
    int right=(int)fminf(WIDTH,(region.x+region.width)*WIDTH);
    int bottom=(int)fminf(HEIGHT,(region.y+region.height)*HEIGHT);
    if (right<=left || bottom<=top) return 0;
    const Color *first=a.data,*second=b.data;
    for (int y=top;y<bottom;y++) for (int x=left;x<right;x++) {
        int i=y*WIDTH+x;
        sum+=abs(first[i].r-second[i].r)+abs(first[i].g-second[i].g)+abs(first[i].b-second[i].b);
        count+=3;
    }
    return (double)sum/count;
}

static double Sample(RenderTexture2D target, const Clearing *map, int flags,
                     Rectangle region, int phase) {
    atmosphere.flags=flags;
    Image first=Render(target,map,false,phase);
    AdvanceAtmosphere(.43f,true);
    Image second=Render(target,map,false,phase);
    double diff=Difference(first,second,region);
    UnloadImage(first);
    UnloadImage(second);
    return diff;
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(WIDTH,HEIGHT,"Auditoria atmosfera");
    if (!IsWindowReady()) return 2;
    InitAtmosphere();
    if (atmosphere.fogPass<0 || atmosphere.surfaceZones<0 || atmosphere.actorCount<0) return 3;
    MakeDirectory(output);
    FILE *csv=fopen(TextFormat("%s/medicoes.csv",output),"w");
    if (!csv) return 4;
    FILE *timeline=fopen(TextFormat("%s/quadros.csv",output),"w");
    if (!timeline) return 4;
    fputs("mapa,tempo_ms,regiao,delta_rgb_desde_zero\n",timeline);
    fputs("mapa,delta_nevoa,delta_luz,delta_raio_poeira,delta_particulas_meio,delta_vegetacao,"
          "delta_chama,delta_reflexo_local,delta_sombra,delta_fase,estatico,reduzido,oculto,fps_alta,fps_media,fps_baixa\n",csv);
    RenderTexture2D target=LoadRenderTexture(WIDTH,HEIGHT);
    hero=LoadTexture("assets/protagonista.png");
    if (!target.id || !hero.id) return 5;
    const Rectangle full={0,0,1,1}, middle={.22f,.34f,.56f,.32f}, edge={.014f,.74f,.18f,.23f};
    /* Trunk, stones, and masonry outside the wind masks. Mild light spill is allowed. */
    const Rectangle structures[]={
        [0]={.056f,.728f,.015f,.028f}, [1]={.067f,.094f,.014f,.020f},
        [2]={.041f,.242f,.012f,.026f}, [3]={.049f,.080f,.039f,.060f},
        [5]={.050f,.076f,.038f,.057f}, [6]={.013f,.305f,.021f,.036f},
        [7]={.055f,.157f,.009f,.040f}, [8]={.852f,.136f,.009f,.029f}
    };
    const float times[]={0,.5f,1.f,2.f};
    int failures=0;
    for (int id=0;id<=8;id++) {
        if (id==4) continue;
        Clearing map={0};
        if (!InitCampaignMap(&map,id)) return 6;
        if (IsMapActorBlocked(&map,map.spawn)) failures++;
        LoadMapAtmosphere(id);
        atmosphere.quality=2;
        atmosphere.flags=255;
        atmosphere.reduced=false;
        const AtmosphereConfig *cfg=&atmosphere.config;
        Image initial={0};
        for (int i=0;i<4;i++) {
            if (i) AdvanceAtmosphere(times[i]-times[i-1],true);
            Image frame=Render(target,&map,false,1);
            if (!ExportImage(frame,TextFormat("%s/mapa-%d-%dms.png",output,id,(int)(1000*times[i])))) failures++;
            if (!i) {
                initial=frame;
                continue;
            }
            int ms=(int)(1000*times[i]);
            fprintf(timeline,"%d,%d,centro,%.6f\n%d,%d,borda,%.6f\n",id,ms,
                    Difference(initial,frame,middle),id,ms,Difference(initial,frame,edge));
            double structure=Difference(initial,frame,structures[id]);
            fprintf(timeline,"%d,%d,estrutura_fixa,%.6f\n",id,ms,structure);
            if (structure>1.5) failures++;
            for (int j=0;j<VEGETATION && cfg->vegetation[j].width>0;j++)
                fprintf(timeline,"%d,%d,vegetacao_%d,%.6f\n",id,ms,j,
                        Difference(initial,frame,cfg->vegetation[j]));
            for (int j=0;j<FLAMES && cfg->flames[j].width>0;j++) {
                SurfaceEffect f=cfg->flames[j];
                Rectangle area={f.at.x-f.width,f.at.y-f.height,f.width*2,f.height*2};
                fprintf(timeline,"%d,%d,chama_%d,%.6f\n",id,ms,j,Difference(initial,frame,area));
            }
            for (int j=0;j<REFLECTIONS && cfg->reflections[j].width>0;j++) {
                SurfaceEffect r=cfg->reflections[j];
                Rectangle area={r.at.x-r.width,r.at.y-r.height,r.width*2,r.height*2};
                fprintf(timeline,"%d,%d,reflexo_%d,%.6f\n",id,ms,j,Difference(initial,frame,area));
            }
            for (int j=0;j<RAYS && cfg->rays[j].strength>0;j++) {
                const LightBeam *ray=&cfg->rays[j];
                Vector2 mid=Vector2Lerp(ray->from,ray->to,.5f);
                Rectangle area={mid.x-ray->width*.45f,mid.y-.025f,ray->width*.9f,.05f};
                fprintf(timeline,"%d,%d,feixe_%d,%.6f\n",id,ms,j,Difference(initial,frame,area));
            }
            UnloadImage(frame);
        }
        UnloadImage(initial);
        atmosphere.debug=atmosphere.guides=true;
        Image guide=Render(target,&map,true,1);
        ExportImage(guide,TextFormat("%s/mapa-%d-guias.png",output,id));
        UnloadImage(guide);
        atmosphere.debug=atmosphere.guides=false;
        double fog=Sample(target,&map,FOG,edge,1);
        double light=Sample(target,&map,LIGHT,full,1);
        double rays=Sample(target,&map,RAY,full,1);
        double particles=Sample(target,&map,PARTICLE,middle,1);
        double shadow=Sample(target,&map,SHADOW|LIGHT,full,1);
        double wind=0, fire=0, reflect=0, phaseDelta=0;
        for (int i=0;i<VEGETATION && cfg->vegetation[i].width>0;i++)
            wind=fmax(wind,Sample(target,&map,WIND,cfg->vegetation[i],1));
        for (int i=0;i<FLAMES && cfg->flames[i].width>0;i++) {
            SurfaceEffect f=cfg->flames[i];
            Rectangle roi={f.at.x-f.width,f.at.y-f.height,f.width*2,f.height*2};
            fire=fmax(fire,Sample(target,&map,FIRE,roi,1));
        }
        if (cfg->reflections[0].width>0) {
            SurfaceEffect r=cfg->reflections[0];
            reflect=Sample(target,&map,LIGHT,(Rectangle){r.at.x-r.width,r.at.y-r.height,r.width*2,r.height*2},1);
        }
        if (id==8) {
            atmosphere.flags=LIGHT;
            Image one=Render(target,&map,false,1), three=Render(target,&map,false,3);
            phaseDelta=Difference(one,three,(Rectangle){.25f,.34f,.5f,.40f});
            UnloadImage(one);
            UnloadImage(three);
        }
        double fixed=Sample(target,&map,0,full,1);
        atmosphere.flags=255;
        Image visible=Render(target,&map,false,1);
        Camera2D camera={.zoom=1};
        if (!IsWindowHidden()) failures++;
        UpdateAtmosphere(&map,camera,1.f);
        Image hidden=Render(target,&map,false,1);
        double frozen=Difference(visible,hidden,full);
        UnloadImage(visible);
        UnloadImage(hidden);
        atmosphere.reduced=true;
        double reduced=Sample(target,&map,255,full,1);
        atmosphere.reduced=false;
        double fps[3];
        for (int q=2;q>=0;q--) {
            atmosphere.quality=q;
            double start=GetTime();
            for (int f=0;f<24;f++) {
                AdvanceAtmosphere(1.f/60,true);
                Image frame=Render(target,&map,false,1);
                UnloadImage(frame);
            }
            fps[q]=24/(GetTime()-start);
        }
        fprintf(csv,"%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.1f,%.1f,%.1f\n",
                id,fog,light,rays,particles,wind,fire,reflect,shadow,phaseDelta,fixed,reduced,frozen,fps[2],fps[1],fps[0]);
        fflush(csv);
        if (fog<=0 || light<=0 || fixed!=0 || reduced!=0 || frozen!=0 ||
            (cfg->rays[0].strength>0 && rays<=0) ||
            (cfg->gustCount && particles<=0) ||
            (cfg->vegetation[0].width>0 && wind<=0) ||
            (cfg->flames[0].width>0 && fire<=0) ||
            (cfg->reflections[0].width>0 && reflect<=0) || (id==8 && phaseDelta<=0)) {
            fprintf(stderr,"Efeito sem movimento ou controle instavel no mapa %d\n",id);
            failures++;
        }
        UnloadClearing(&map);
    }
    fclose(csv);
    fclose(timeline);
    UnloadTexture(hero);
    UnloadRenderTexture(target);
    UnloadAtmosphere();
    CloseWindow();
    return failures!=0;
}
