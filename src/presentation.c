#include "game_text.h"
#include "presentation.h"
#include <math.h>
void BeginStagePresentation(StagePresentation *p,int wave,const char *mapName,const char *bossName) {
    *p=(StagePresentation){.duration=bossName?3.0f:1.6f,.wave=wave,
        .boss=bossName!=0,.mapName=mapName,.bossName=bossName,.portraitCenterX=.77f};
    p->remaining=p->duration;
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
        float portraitCenter=p->portraitCenterX;
        BeginStagePresentation(p,p->wave,p->mapName,p->bossName);
        p->portraitCenterX=portraitCenter;
        p->remaining=fmaxf(0,p->remaining-excess);
        return;
    }
    if(p->remaining==0 && p->nextAreaName) {
        const char *name=p->nextAreaName;
        *p=(StagePresentation){.duration=3.2f,.remaining=fmaxf(0,3.2f-excess),
            .mapName=name,.areaUnlock=true};
    }
}
void DrawStagePresentation(const StagePresentation *p,Texture2D sprite,int columns,int rows) {
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
        const char *label=p->areaEntry?(p->areaSubtitle?"":"ENTRANDO EM"):"NOVA ÁREA DESBLOQUEADA";
        DrawGameText(label,(int)((w-MeasureGameText(label,19))/2),(int)center-61,19,Fade(GOLD,alpha));
        DrawGameTextIn(p->mapName,(Rectangle){w*.07f+(1-reveal)*65,center-(p->areaSubtitle?39:30),w*.86f,58},
                       52,Fade(RAYWHITE,alpha),true);
        const char *hint=p->areaEntry?(p->areaSubtitle?p->areaSubtitle:""):"O caminho adiante foi revelado";
        int hintSize=p->areaSubtitle?24:18;
        while(hintSize>12 && MeasureGameText(hint,hintSize)>w*.86f) hintSize--;
        DrawGameText(hint,(int)((w-MeasureGameText(hint,hintSize))/2),(int)center+(p->areaSubtitle?26:48),
            hintSize,Fade(p->areaSubtitle?GOLD:LIGHTGRAY,alpha));
        return;
    }
    const Color headline = {255, 232, 190, 255};
    DrawRectangle(0,0,(int)w,(int)h,Fade(BLACK,alpha*(p->boss?.88f:.22f)));
    if (!p->boss) {
        float y=h*.42f;
        DrawRectangle(0,(int)y,(int)w,118,Fade((Color){20,14,27,255},alpha*.94f));
        DrawLine(0,(int)y,(int)w,(int)y,Fade(GOLD,alpha));
        DrawLine(0,(int)y+118,(int)w,(int)y+118,Fade(GOLD,alpha));
        const char *title=p->announcement?p->announcement:TextFormat("ONDA %d",p->wave);
        DrawGameTextIn(title,(Rectangle){w*.06f+slide,y+8,w*.88f,68},58,Fade(headline,alpha),false);
        DrawGameTextIn(p->mapName,(Rectangle){w*.06f,y+77,w*.88f,28},22,Fade(RAYWHITE,alpha),false);
    } else {
        DrawRectangle(0,0,(int)w,(int)(h*.10f),Fade(BLACK,alpha));
        DrawRectangle(0,(int)(h*.90f),(int)w,(int)(h*.10f+1),Fade(BLACK,alpha));
        float offset=(1-alpha)*w*.8f;
        Vector2 a={-offset,h*.23f},b={w-offset,h*.34f};
        Vector2 c={w-offset,h*.77f},d={-offset,h*.66f};
        Color band=Fade((Color){48,28,58,255},alpha);
        DrawTriangle(a,d,c,band); DrawTriangle(a,c,b,band);
        DrawLineEx(a,b,3,Fade(GOLD,alpha));
        DrawLineEx(d,c,3,Fade(GOLD,alpha));
        float size=fminf(h*.60f,w*.43f);
        if (sprite.id && columns>0 && rows>0) {
            float cw=(float)sprite.width/columns,ch=(float)sprite.height/rows;
            int frame=(int)(elapsed*6)%columns;
            float zoom=1+.025f*sinf(elapsed*1.5f);
            float drawHeight=size*zoom;
            float drawWidth=drawHeight*cw/ch;
            DrawTexturePro(sprite,(Rectangle){frame*cw,0,cw,ch},
                (Rectangle){w*p->portraitCenterX-drawWidth/2-offset,h*.50f-drawHeight/2,drawWidth,drawHeight},
                (Vector2){0},0,Fade(WHITE,alpha));
        }
        float textX=w*.13f-offset;
        DrawGameTextEx(TextFormat("ONDA %d - MINI-BOSS",p->wave),(Vector2){textX,h*.37f},
                       24,Fade(headline,alpha),false);
        float bossSize=fminf(48, w*.42f/MeasureGameTextEx(p->bossName,1,false).x);
        DrawGameTextEx(p->bossName,(Vector2){textX,h*.44f},bossSize,Fade(RAYWHITE,alpha),false);
        DrawGameText(p->mapName,(int)(w*.13f-offset),(int)(h*.56f),20,Fade(LIGHTGRAY,alpha));
    }
}
