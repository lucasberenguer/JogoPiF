#include "game_input.h"
#include "raymath.h"
#include <ctype.h>
#include <string.h>

static unsigned inputFrame;
static bool usingGamepad;
static bool playStation;
static int stickHorizontal, stickVertical;
static Vector2 lastLeft, lastRight;
static bool navigate[4];

bool PadPressed(int button) {
    return IsGamepadAvailable(0) && IsGamepadButtonPressed(0, button);
}

Vector2 PadStick(int xAxis, int yAxis) {
    if (!IsGamepadAvailable(0)) return (Vector2){0};
    const float deadzone = .22f;
    Vector2 stick = {GetGamepadAxisMovement(0, xAxis), GetGamepadAxisMovement(0, yAxis)};
    float magnitude = Vector2Length(stick);
    if (magnitude <= deadzone) return (Vector2){0};
    float strength = (fminf(magnitude, 1.0f) - deadzone) / (1.0f - deadzone);
    return Vector2Scale(stick, strength / magnitude);
}

static int StickDirection(float value, int previous) {
    if (value > .52f) return 1;
    if (value < -.52f) return -1;
    return fabsf(value) < .30f ? 0 : previous;
}

void UpdateGameInput(void) {
    inputFrame++;
    Vector2 left = PadStick(GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y);
    Vector2 right = PadStick(GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y);
    // A stick held at the same angle does not steal focus back from the mouse.
    const float stickChange = .07f;
    bool movedStick = (Vector2LengthSqr(left) > .01f &&
                       Vector2DistanceSqr(left, lastLeft) >= stickChange * stickChange) ||
                      (Vector2LengthSqr(right) > .01f &&
                       Vector2DistanceSqr(right, lastRight) >= stickChange * stickChange);
    if (movedStick || Vector2LengthSqr(left) <= .01f) lastLeft = left;
    if (movedStick || Vector2LengthSqr(right) <= .01f) lastRight = right;
    int x = StickDirection(left.x, stickHorizontal);
    int y = StickDirection(left.y, stickVertical);
    navigate[0] = y < 0 && y != stickVertical;
    navigate[1] = x > 0 && x != stickHorizontal;
    navigate[2] = y > 0 && y != stickVertical;
    navigate[3] = x < 0 && x != stickHorizontal;
    stickHorizontal = x;
    stickVertical = y;

    bool padButton = false;
    for (int button = GAMEPAD_BUTTON_LEFT_FACE_UP; button <= GAMEPAD_BUTTON_RIGHT_THUMB; button++)
        padButton |= PadPressed(button);
    bool key = false;
    for (int code = KEY_SPACE; code <= KEY_KB_MENU && !key; code++)
        key = IsKeyPressed(code);
    Vector2 mouse = GetMouseDelta();
    bool mouseButton = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                       IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                       IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    if (key || mouseButton || !IsGamepadAvailable(0)) {
        usingGamepad = false;
    } else if (padButton || navigate[0] || navigate[1] || navigate[2] || navigate[3]) {
        usingGamepad = true;
    } else if (Vector2LengthSqr(mouse) >= 4.0f || GetMouseWheelMove() != 0.0f) {
        usingGamepad = false;
    } else if (movedStick) {
        usingGamepad = true;
    }

    if (usingGamepad) {
        char name[128] = {0};
        const char *raw = GetGamepadName(0);
        if (raw)
            for (int i = 0; i < (int)sizeof(name) - 1 && raw[i]; i++)
                name[i] = (char)tolower((unsigned char)raw[i]);
        playStation = strstr(name, "dualsense") || strstr(name, "dualshock") ||
                      strstr(name, "playstation") || strstr(name, "sony") ||
                      strstr(name, "ps4") || strstr(name, "ps5") ||
                      strstr(name, "wireless controller");
        if (strstr(name, "xbox") || strstr(name, "xinput")) playStation = false;
    }
}

bool PadNavigate(int direction) {
    return PadPressed(direction) || navigate[direction - GAMEPAD_BUTTON_LEFT_FACE_UP];
}

unsigned GameInputFrame(void) {
    return inputFrame;
}

bool UsingGamepad(void) {
    return usingGamepad && IsGamepadAvailable(0);
}

const char *InputHint(const char *keyboardMouse, const char *gamepad) {
    if (!UsingGamepad()) return keyboardMouse;
    if (playStation) return gamepad;
    // Pad hints use PlayStation names in brackets. Translate those tokens for Xbox.
    static char buffers[8][256];
    static unsigned index;
    char *line = buffers[index++ % 8];
    static const char *const labels[][2] = {
        {"[Cruz]", "[A]"}, {"[Círculo]", "[B]"}, {"[Quadrado]", "[X]"},
        {"[Triângulo]", "[Y]"}, {"[Options]", "[Menu]"}, {"[Share]", "[View]"},
        {"R1/R2", "RB/RT"}, {"L1/L2", "LB/LT"}, {"L1/R1", "LB/RB"}
    };
    size_t used = 0;
    while (*gamepad && used < sizeof(buffers[0]) - 1) {
        bool replaced = false;
        for (int i = 0; i < (int)(sizeof(labels) / sizeof(labels[0])); i++) {
            size_t length = strlen(labels[i][0]);
            if (strncmp(gamepad, labels[i][0], length) != 0) continue;
            size_t output = strlen(labels[i][1]);
            if (used + output >= sizeof(buffers[0])) break;
            memcpy(line + used, labels[i][1], output);
            used += output;
            gamepad += length;
            replaced = true;
            break;
        }
        if (!replaced) line[used++] = *gamepad++;
    }
    line[used] = 0;
    return line;
}
