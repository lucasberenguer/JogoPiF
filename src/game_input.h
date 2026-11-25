#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "raylib.h"

void UpdateGameInput(void);
bool ArcadeCalibrationActive(void);
void BeginArcadeCalibration(void);
const char *ArcadeCalibrationPrompt(void);
const char *ArcadeCalibrationStatus(void);
bool KeyPressed(int key);
bool KeyDown(int key);
int TextInputPressed(void);
bool PadButtonPressed(void);
bool PadPressed(int button);
bool PadNavigate(int direction);
Vector2 PadMovement(void);
Vector2 PadStick(int xAxis, int yAxis);
unsigned GameInputFrame(void);
bool UsingGamepad(void);
bool UsingArcade(void);
bool UsingAutomaticArcade(void);
bool ArcadeRawButtonPressed(void);
const char *InputHint(const char *keyboardMouse, const char *gamepad);

#endif
