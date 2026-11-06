#include "presentation.h"
#include <math.h>
void BeginStagePresentation(StagePresentation *p,int wave,const char *mapName,const char *bossName) {
    *p=(StagePresentation){.duration=bossName?3.0f:1.6f,.wave=wave,
        .boss=bossName!=0,.mapName=mapName,.bossName=bossName,.portraitCenterX=.77f};
    p->remaining=p->duration;
}
void UpdateStagePresentation(StagePresentation *p,float dt) {
    if (dt>0) p->remaining=fmaxf(0,p->remaining-dt);
}
void DrawStagePresentation(const StagePresentation *p,Texture2D sprite,int columns,int rows) {
    if (p->remaining<=0) return;
    float elapsed=p->duration-p->remaining;
    float alpha=fminf(1,fminf(elapsed/.22f,p->remaining/.35f));
    float w=(float)GetScreenWidth(),h=(float)GetScreenHeight();
    float slide=(1-alpha)*50;
    DrawRectangle(0,0,(int)w,(int)h,Fade(BLACK,alpha*(p->boss?.88f:.22f)));
    if (!p->boss) {
        float y=h*.42f;
        DrawRectangle(0,(int)y,(int)w,118,Fade((Color){20,14,27,255},alpha*.94f));
        DrawLine(0,(int)y,(int)w,(int)y,Fade(GOLD,alpha));
        DrawLine(0,(int)y+118,(int)w,(int)y+118,Fade(GOLD,alpha));
        const char *title=p->announcement?p->announcement:TextFormat("ONDA %d",p->wave);
        DrawText(title,(int)((w-MeasureText(title,46))/2+slide),(int)y+18,46,Fade(GOLD,alpha));
        DrawText(p->mapName,(int)((w-MeasureText(p->mapName,21))/2),(int)y+79,21,Fade(RAYWHITE,alpha));
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
        int font=(int)fminf(38,w*.029f);
        DrawText(TextFormat("ONDA %d - MINI-BOSS",p->wave),(int)(w*.13f-offset),(int)(h*.38f),20,Fade(GOLD,alpha));
        DrawText(p->bossName,(int)(w*.13f-offset),(int)(h*.46f),font,Fade(RAYWHITE,alpha));
        DrawText(p->mapName,(int)(w*.13f-offset),(int)(h*.56f),20,Fade(LIGHTGRAY,alpha));
    }
}
