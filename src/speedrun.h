#ifndef SPEEDRUN_H
#define SPEEDRUN_H
#include <stdbool.h>
#define SPEEDRUN_TOP 10
typedef struct SpeedrunBoard { double times[SPEEDRUN_TOP]; int count; } SpeedrunBoard;
void SpeedrunStart(double seconds, bool eligible);
void SpeedrunTick(double dt);
void SpeedrunInvalidate(void);
double SpeedrunTime(void);
bool SpeedrunEligible(void);
void SpeedrunLoad(SpeedrunBoard *board);
bool SpeedrunRecord(SpeedrunBoard *board, double seconds);
void SpeedrunFormat(char *buffer, int size, double seconds);
#endif
