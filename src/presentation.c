#include "game_text.h"
#include "presentation.h"
#include <math.h>
void BeginStagePresentation(StagePresentation *p,int wave,const char *mapName,const char *bossName) {
    *p=(StagePresentation){.duration=bossName?3.0f:1.6f,.wave=wave,
        .boss=bossName!=0,.mapName=mapName,.bossName=bossName,.portraitCenterX=.77f};
    p->remaining=p->duration;
}
void ShowWaveBeforeBoss(StagePresentation *p) {
    p->separateBossWave = true;
    p->boss = false;
    p->duration = p->remaining = 1.6f;
}
void QueueAreaUnlock(StagePresentation *p,const char *mapName) {
    p->nextAreaName=mapName;
}
void BeginAreaEntry(StagePresentation *p) {
    if(p->remaining<=0) return;
    p->areaEntry=true;
    p->duration=p->remaining=2.8f;
}
void UpdateStagePresentation(StagePresentation *p,float dt) {
    if(dt<=0) return;
    float excess=dt-p->remaining;
    p->remaining=fmaxf(0,p->remaining-dt);
    if(p->remaining==0 && p->areaEntry) {
        if (p->wave <= 0 && !p->bossName) {
            p->areaEntry = false;
            return;
        }
        p->areaEntry = false;
        p->duration = p->boss ? 3.0f : 1.6f;
        p->remaining = fmaxf(0, p->duration - excess);
        excess = fmaxf(0, excess - p->duration);
    }
    if (p->remaining == 0 && p->separateBossWave && !p->boss) {
        p->boss = true;
        p->duration = 3.0f;
        p->remaining = fmaxf(0, p->duration - excess);
    }
    if(p->remaining==0 && p->nextAreaName) {
        const char *name=p->nextAreaName;
        *p=(StagePresentation){.duration=3.2f,.remaining=fmaxf(0,3.2f-excess),
            .mapName=name,.areaUnlock=true};
    }
}
void DrawStagePresentation(const StagePresentation *p,Texture2D sprite,int columns,int rows,bool gothic) {
    if (p->remaining<=0) return;
    float elapsed=p->duration-p->remaining;
    float alpha=fminf(1,fminf(elapsed/.22f,p->remaining/.35f));
    float w=(float)GetScreenWidth(),h=(float)GetScreenHeight();
    float slide=(1-alpha)*50;
    if(p->areaUnlock || p->areaEntry) {
        float center=h*.48f;
        float reveal=fminf(1,elapsed/.65f);
        reveal=1-powf(1-reveal,3);
        DrawRectangle(0,0,(int)w,(int)h,Fade(BLACK,alpha*.28f));
        DrawRectangle(0,(int)(center-85),(int)w,174,Fade((Color){16,19,25,255},alpha*.94f));
        float half=w*.34f*reveal;
        DrawLineEx((Vector2){w/2-half,center-85},(Vector2){w/2+half,center-85},2,Fade(GOLD,alpha));
        DrawLineEx((Vector2){w/2-half,center+89},(Vector2){w/2+half,center+89},2,Fade(GOLD,alpha));
        const char *label=p->areaEntry?(p->areaSubtitle?"":gothic?"Entrando em":"ENTRANDO EM"):
            gothic?"Nova área desbloqueada":"NOVA ÁREA DESBLOQUEADA";
        float labelSize = gothic ? 23 : 19;
        DrawGameTextEx(label,(Vector2){(w-MeasureGameTextEx(label,labelSize,gothic).x)/2,center-61},
                       labelSize,Fade(GOLD,alpha),gothic);
        DrawGameTextIn(p->mapName,(Rectangle){w*.07f+(1-reveal)*65,center-(p->areaSubtitle?39:30),w*.86f,58},
                       52,Fade(RAYWHITE,alpha),true);
        const char *hint=p->areaEntry?(p->areaSubtitle?p->areaSubtitle:""):"O caminho adiante foi revelado";
        float hintSize=p->areaSubtitle?24:gothic?21:18;
        float hintWidth=MeasureGameTextEx(hint,hintSize,gothic).x;
        if (hintWidth>w*.86f) hintSize*=w*.86f/hintWidth;
        DrawGameTextEx(hint,(Vector2){(w-MeasureGameTextEx(hint,hintSize,gothic).x)/2,
            center+(p->areaSubtitle?26:48)},hintSize,Fade(p->areaSubtitle?GOLD:LIGHTGRAY,alpha),gothic);
        return;
    }
    const Color headline = {255, 232, 190, 255};
    const char *waveLabel = gothic ? "Onda" : "ONDA";
    const char *waveTitle = p->waveTotal > 0
        ? TextFormat("%s %d/%d", waveLabel, p->wave, p->waveTotal) : TextFormat("%s %d", waveLabel, p->wave);
    DrawRectangle(0,0,(int)w,(int)h,Fade(BLACK,alpha*(p->boss?.88f:.22f)));
    if (!p->boss) {
        float y=h*.42f;
        DrawRectangle(0,(int)y,(int)w,118,Fade((Color){20,14,27,255},alpha*.94f));
        DrawLine(0,(int)y,(int)w,(int)y,Fade(GOLD,alpha));
        DrawLine(0,(int)y+118,(int)w,(int)y+118,Fade(GOLD,alpha));
        const char *title=p->announcement?p->announcement:waveTitle;
        DrawGameTextIn(title,(Rectangle){w*.06f+slide,y+8,w*.88f,68},58,Fade(headline,alpha),gothic);
        const char *subtitle = p->announcement && p->waveTotal > 0 && p->wave > 0 ? waveTitle : p->mapName;
        DrawGameTextIn(subtitle,(Rectangle){w*.06f,y+77,w*.88f,30},gothic?26:22,Fade(RAYWHITE,alpha),gothic);
    } else {
        DrawRectangle(0,0,(int)w,(int)(h*.10f),Fade(BLACK,alpha));
        DrawRectangle(0,(int)(h*.90f),(int)w,(int)(h*.10f+1),Fade(BLACK,alpha));
        float offset=(1-alpha)*w*.8f;
        Vector2 a={-offset,h*.23f},b={w-offset,h*.34f};
        Vector2 c={w-offset,h*.77f},d={-offset,h*.66f};
        Color band=Fade(p->waveTotal > 0 ? (Color){65,25,21,255} : (Color){48,28,58,255},alpha);
        DrawTriangle(a,d,c,band); DrawTriangle(a,c,b,band);
        DrawLineEx(a,b,3,Fade(GOLD,alpha));
        DrawLineEx(d,c,3,Fade(GOLD,alpha));
        float size=fminf(h*.60f,w*.43f);
        if (sprite.id && columns>0 && rows>0) {
            float cw=(float)sprite.width/columns,ch=(float)sprite.height/rows;
            int frame=(int)(elapsed*6)%columns;
            float zoom=1+.025f*sinf(elapsed*1.5f);
            float widthLimit = w * (1 - p->portraitCenterX - .025f) * 2;
            float drawHeight=fminf(size*zoom, widthLimit*ch/cw);
            float drawWidth=drawHeight*cw/ch;
            DrawTexturePro(sprite,(Rectangle){frame*cw,0,cw,ch},
                (Rectangle){w*p->portraitCenterX-drawWidth/2-offset,h*.50f-drawHeight/2,drawWidth,drawHeight},
                (Vector2){0},0,Fade(WHITE,alpha));
        }
        float textX=w*.13f-offset;
        if (!p->separateBossWave)
            DrawGameTextEx(TextFormat("%s - %s",waveTitle,gothic?"Chefe":"MINI-BOSS"),(Vector2){textX,h*.37f},
                           gothic?27:24,Fade(headline,alpha),gothic);
        float bossSize=fminf(gothic?56:48, w*.42f/MeasureGameTextEx(p->bossName,1,gothic).x);
        DrawGameTextEx(p->bossName,(Vector2){textX,h*(p->separateBossWave?.43f:.44f)},
                       bossSize,Fade(RAYWHITE,alpha),gothic);
        DrawGameTextEx(p->mapName,(Vector2){textX,h*.56f},gothic?24:20,Fade(LIGHTGRAY,alpha),gothic);
    }
}
