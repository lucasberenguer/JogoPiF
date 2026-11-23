#include "speedrun.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define make_dir _mkdir
#define process_id _getpid
#define current_dir _getcwd
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(path) mkdir(path,0700)
#define process_id getpid
#define current_dir getcwd
#endif

static char storage[512];
const char *GetApplicationDirectory(void) {return storage;}

int main(void) {
    char base[384];
    assert(current_dir(base,sizeof(base)));
    snprintf(storage,sizeof(storage),"%s/speedrun_fixture_%ld_%d",base,(long)time(NULL),process_id());
    assert(make_dir(storage)==0);
    char oldPath[600],newPath[600];
    snprintf(oldPath,sizeof(oldPath),"%s/speedrun_pif.dat",storage);
    snprintf(newPath,sizeof(newPath),"%s/speedrun_pif_v2.dat",storage);
    FILE *file=fopen(oldPath,"w");
    assert(file);
    fprintf(file,"PIF-SPEEDRUN-1\n90.00\n120.00\n");
    assert(fclose(file)==0);
    SpeedrunBoard board;
    SpeedrunLoad(&board);
    assert(board.count==2 && board.times[0]==90 && !strcmp(board.names[0],"Sem nome"));
    assert(SpeedrunRecord(&board,80,"João Paulo"));
    assert(board.count==3 && board.times[0]==80 && !strcmp(board.names[0],"João Paulo"));
    assert(SpeedrunRecord(&board,110,"Arthur"));
    SpeedrunLoad(&board);
    assert(board.count==4 && board.times[2]==110 && !strcmp(board.names[2],"Arthur"));
    for (int i=0;i<8;i++) assert(SpeedrunRecord(&board,200+i,"Outro jogador"));
    assert(board.count==SPEEDRUN_TOP);
    assert(SpeedrunRecord(&board,1000,"Run fora do top"));
    assert(board.count==SPEEDRUN_TOP && board.times[0]==80);
    file=fopen(newPath,"r");
    assert(file);
    char header[32];
    assert(fgets(header,sizeof(header),file) && !strcmp(header,"PIF-SPEEDRUN-2\n"));
    bool found=false;
    char line[256];
    while (fgets(line,sizeof(line),file))
        if (strstr(line,"Run fora do top")) found=true;
    assert(found);
    fclose(file);
    return 0;
}
