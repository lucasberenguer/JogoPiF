#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "raylib.h"

void UpdateGameInput(void);
bool PadPressed(int button);
bool PadNavigate(int direction);
Vector2 PadStick(int xAxis, int yAxis);
unsigned GameInputFrame(void);
bool UsingGamepad(void);
const char *InputHint(const char *keyboardMouse, const char *gamepad);

#endif
