#include "speedrun.h"
#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static double elapsed;
static bool valid;
static const char *BoardPath(void) {
    static char path[1024];
    snprintf(path,sizeof(path),"%s/speedrun_pif.dat",GetApplicationDirectory());
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
void SpeedrunLoad(SpeedrunBoard *board) {
    *board=(SpeedrunBoard){0};
    FILE *file=fopen(BoardPath(),"r");
    if (!file) return;
    char header[32];
    if (!fgets(header,sizeof(header),file) || strcmp(header,"PIF-SPEEDRUN-1\n")) { fclose(file); return; }
    double value;
    while (board->count<SPEEDRUN_TOP && fscanf(file,"%lf",&value)==1) {
        if (value<=0 || !isfinite(value) || (board->count && value<board->times[board->count-1])) break;
        board->times[board->count++]=value;
    }
    fclose(file);
}
bool SpeedrunRecord(SpeedrunBoard *board, double seconds) {
    if (seconds<=0 || !isfinite(seconds)) return false;
    int index=0;
    while (index<board->count && board->times[index]<=seconds) index++;
    if (index>=SPEEDRUN_TOP) return true;
    if (board->count<SPEEDRUN_TOP) board->count++;
    for (int i=board->count-1;i>index;i--) board->times[i]=board->times[i-1];
    board->times[index]=seconds;
    FILE *file=fopen(BoardPath(),"w");
    if (!file) return false;
    fprintf(file,"PIF-SPEEDRUN-1\n");
    for (int i=0;i<board->count;i++) fprintf(file,"%.2f\n",board->times[i]);
    return fclose(file)==0;
}
