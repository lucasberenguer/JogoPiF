#ifndef SPEEDRUN_H
#define SPEEDRUN_H
#include <stdbool.h>
#define SPEEDRUN_TOP 10
#define SPEEDRUN_NAME_BYTES 64
typedef struct SpeedrunBoard {
    double times[SPEEDRUN_TOP];
    char names[SPEEDRUN_TOP][SPEEDRUN_NAME_BYTES];
    int count;
} SpeedrunBoard;
void SpeedrunStart(double seconds, bool eligible);
void SpeedrunTick(double dt);
void SpeedrunInvalidate(void);
double SpeedrunTime(void);
bool SpeedrunEligible(void);
void SpeedrunLoad(SpeedrunBoard *board);
bool SpeedrunRecord(SpeedrunBoard *board, double seconds, const char *name);
void SpeedrunFormat(char *buffer, int size, double seconds);
#endif
