#include "speedrun.h"
#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static double elapsed;
static bool valid;
static const char *BoardPath(void) {
    static char path[1024];
    snprintf(path,sizeof(path),"%s/speedrun_pif.dat",GetApplicationDirectory());
    return path;
}
static const char *NamedBoardPath(void) {
    static char path[1024];
    snprintf(path,sizeof(path),"%s/speedrun_pif_v2.dat",GetApplicationDirectory());
    return path;
}
void SpeedrunStart(double seconds, bool eligible) { elapsed=seconds>=0 && isfinite(seconds) ? seconds : 0; valid=eligible; }
void SpeedrunTick(double dt) { if (dt>0 && isfinite(dt)) elapsed+=dt; }
void SpeedrunInvalidate(void) { valid=false; }
double SpeedrunTime(void) { return elapsed; }
bool SpeedrunEligible(void) { return valid; }
void SpeedrunFormat(char *buffer, int size, double seconds) {
    long long centis=(long long)(fmax(0,seconds)*100+0.5);
    snprintf(buffer,size,"%02lld:%02lld:%02lld.%02lld",centis/360000,centis/6000%60,centis/100%60,centis%100);
}
static void InsertRun(SpeedrunBoard *board, double seconds, const char *name) {
    if (seconds<=0 || !isfinite(seconds)) return;
    int index=0;
    while (index<board->count && board->times[index]<=seconds) index++;
    if (index>=SPEEDRUN_TOP) return;
    if (board->count<SPEEDRUN_TOP) board->count++;
    for (int i=board->count-1;i>index;i--) {
        board->times[i]=board->times[i-1];
        memcpy(board->names[i],board->names[i-1],SPEEDRUN_NAME_BYTES);
    }
    board->times[index]=seconds;
    snprintf(board->names[index],SPEEDRUN_NAME_BYTES,"%s",name && name[0]?name:"Sem nome");
}
static void LoadLegacy(SpeedrunBoard *board) {
    FILE *file=fopen(BoardPath(),"r");
    if (!file) return;
    char header[32];
    if (fgets(header,sizeof(header),file) && !strcmp(header,"PIF-SPEEDRUN-1\n")) {
        double value;
        while (fscanf(file,"%lf",&value)==1) InsertRun(board,value,"Sem nome");
    }
    fclose(file);
}
void SpeedrunLoad(SpeedrunBoard *board) {
    *board=(SpeedrunBoard){0};
    FILE *file=fopen(NamedBoardPath(),"r");
    if (!file) {LoadLegacy(board);return;}
    char line[256];
    if (!fgets(line,sizeof(line),file) || strcmp(line,"PIF-SPEEDRUN-2\n")) {
        fclose(file);
        LoadLegacy(board);
        return;
    }
    while (fgets(line,sizeof(line),file)) {
        char *end;
        double seconds=strtod(line,&end);
        if (end==line || *end!='\t') continue;
        char *name=end+1;
        name[strcspn(name,"\r\n")]=0;
        InsertRun(board,seconds,name);
    }
    fclose(file);
}
bool SpeedrunRecord(SpeedrunBoard *board, double seconds, const char *name) {
    if (!board || seconds<=0 || !isfinite(seconds) || !name || !name[0]) return false;
    char safe[SPEEDRUN_NAME_BYTES];
    int length=0;
    for (const unsigned char *p=(const unsigned char *)name; *p && length<SPEEDRUN_NAME_BYTES-1; p++) {
        if (*p=='\t' || *p=='\r' || *p=='\n' || *p<32) continue;
        safe[length++]=(char)*p;
    }
    safe[length]=0;
    if (!length) return false;
    FILE *file=fopen(NamedBoardPath(),"r");
    bool exists=file!=NULL;
    if (exists) {
        char header[32];
        bool validHeader=fgets(header,sizeof(header),file) && !strcmp(header,"PIF-SPEEDRUN-2\n");
        fclose(file);
        if (!validHeader) return false;
    }
    if (!exists) {
        SpeedrunBoard legacy={0};
        LoadLegacy(&legacy);
        file=fopen(NamedBoardPath(),"w");
        if (!file) return false;
        fprintf(file,"PIF-SPEEDRUN-2\n");
        for (int i=0;i<legacy.count;i++)
            fprintf(file,"%.2f\t%s\n",legacy.times[i],legacy.names[i]);
    } else file=fopen(NamedBoardPath(),"a");
    if (!file) return false;
    bool written=fprintf(file,"%.2f\t%s\n",seconds,safe)>0;
    bool closed=fclose(file)==0;
    if (!written || !closed) return false;
    SpeedrunLoad(board);
    return true;
}
