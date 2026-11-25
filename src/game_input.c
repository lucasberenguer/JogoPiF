#include "game_input.h"
#include "raymath.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef PIF_GLFW_ARCADE
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

enum { ARCADE_KEY = 1, ARCADE_BUTTON, ARCADE_AXIS, ARCADE_HAT };
enum { ARCADE_AXES = 32, ARCADE_BUTTONS = 128, ARCADE_HATS = 8, ARCADE_DEVICES = 16 };
enum { ARCADE_REQUIRED = 8, ARCADE_ACTIONS = 9 };
typedef struct { int type, index, direction; } ArcadeBinding;
typedef struct {
    char guid[33];
    ArcadeBinding bindings[ARCADE_ACTIONS];
} ArcadeProfile;
typedef struct {
    char guid[33];
    float axes[ARCADE_AXES];
    unsigned char buttons[ARCADE_BUTTONS], hats[ARCADE_HATS];
    int buttonCount;
} ArcadeDevice;

static const int arcadeButtons[ARCADE_ACTIONS] = {
    GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
    GAMEPAD_BUTTON_LEFT_FACE_DOWN, GAMEPAD_BUTTON_LEFT_FACE_LEFT,
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT, GAMEPAD_BUTTON_MIDDLE_RIGHT,
    GAMEPAD_BUTTON_RIGHT_FACE_UP
};
static const char *const arcadePrompts[ARCADE_ACTIONS] = {
    "Mova a alavanca para cima", "Mova a alavanca para a direita",
    "Mova a alavanca para baixo", "Mova a alavanca para a esquerda",
    "Pressione o botão de ataque / confirmar", "Pressione o botão de esquiva / voltar",
    "Pressione o botão de interação", "Pressione o botão Start / pausa",
    "Pressione o botão extra, se houver"
};
static ArcadeProfile arcadeProfile, arcadeDraft;
static ArcadeDevice arcadeDevices[ARCADE_DEVICES];
static bool arcadeDown[ARCADE_ACTIONS], arcadePressed[ARCADE_ACTIONS];
static bool arcadeLoaded, arcadeCalibrating, arcadeWaitingForRelease, usingArcade, arcadeAutomatic;
static bool rawButtonPressed;
static unsigned char lastRawButtons[ARCADE_BUTTONS];
static int arcadeStep, arcadeDevice = -1, arcadeDraftDevice = -1;
static const char *arcadeStatus;

static const char *ArcadeConfigPath(void) {
    return TextFormat("%sarcade_pif.cfg", GetApplicationDirectory());
}

static bool IsBindingValid(ArcadeBinding b) {
    switch (b.type) {
        case ARCADE_KEY: return b.index >= KEY_SPACE && b.index <= KEY_KB_MENU &&
                               b.index != KEY_F8 && b.index != KEY_F9 && b.direction == 0;
        case ARCADE_BUTTON: return b.index >= 0 && b.index < ARCADE_BUTTONS && b.direction == 0;
        case ARCADE_AXIS: return b.index >= 0 && b.index < ARCADE_AXES &&
                                 (b.direction == -1 || b.direction == 1);
        case ARCADE_HAT: return b.index >= 0 && b.index < ARCADE_HATS &&
                                (b.direction == 1 || b.direction == 2 ||
                                 b.direction == 4 || b.direction == 8);
        default: return false;
    }
}

static bool BindingsMatch(ArcadeBinding a, ArcadeBinding b) {
    return a.type == b.type && a.index == b.index && a.direction == b.direction;
}

static void LoadArcadeProfile(void) {
    arcadeLoaded = true;
    FILE *file = fopen(ArcadeConfigPath(), "r");
    if (!file) return;
    ArcadeProfile loaded = {0};
    char guid[33] = {0};
    int version = 0;
    bool valid = fscanf(file, "PIF_ARCADE %d %32s", &version, guid) == 2 &&
                 version == 1 && (strcmp(guid, "-") == 0 ||
                 (strlen(guid) == 32 && strspn(guid, "0123456789abcdefABCDEF") == 32));
    if (valid && guid[0] != '-') memcpy(loaded.guid, guid, sizeof(guid));
    for (int i = 0; valid && i < ARCADE_ACTIONS; ++i) {
        ArcadeBinding *b = &loaded.bindings[i];
        valid = fscanf(file, "%d %d %d", &b->type, &b->index, &b->direction) == 3 &&
                (IsBindingValid(*b) || (i >= ARCADE_REQUIRED && !b->type));
        if (b->type > ARCADE_KEY && !loaded.guid[0]) valid = false;
        for (int j = 0; valid && b->type && j < i; ++j)
            valid = !BindingsMatch(loaded.bindings[j], *b);
    }
    fclose(file);
    if (valid) arcadeProfile = loaded;
    else TraceLog(LOG_WARNING, "ARCADE: Configuracao invalida. Reconfigure com F9.");
}

static bool SaveArcadeDraft(void) {
    char contents[384];
    int length = snprintf(contents, sizeof(contents), "PIF_ARCADE 1 %s\n",
                          arcadeDraft.guid[0] ? arcadeDraft.guid : "-");
    for (int i = 0; i < ARCADE_ACTIONS; ++i) {
        ArcadeBinding b = arcadeDraft.bindings[i];
        length += snprintf(contents + length, sizeof(contents) - length,
                           "%d %d %d\n", b.type, b.index, b.direction);
    }
    return SaveFileText(ArcadeConfigPath(), contents);
}

static void ReadArcadeDevice(int id, ArcadeDevice *device) {
    *device = (ArcadeDevice){0};
#ifdef PIF_GLFW_ARCADE
    if (!glfwJoystickPresent(id)) return;
    const char *guid = glfwGetJoystickGUID(id);
    if (!guid || strlen(guid) != sizeof(device->guid) - 1) return;
    memcpy(device->guid, guid, sizeof(device->guid));
    int count = 0;
    const float *axes = glfwGetJoystickAxes(id, &count);
    for (int i = 0; axes && i < count && i < ARCADE_AXES; ++i) device->axes[i] = axes[i];
    const unsigned char *buttons = glfwGetJoystickButtons(id, &device->buttonCount);
    const unsigned char *hats = glfwGetJoystickHats(id, &count);
    for (int i = 0; hats && i < count && i < ARCADE_HATS; ++i) device->hats[i] = hats[i];
    // Raylib's GLFW backend also reports the hat directions as virtual buttons.
    device->buttonCount = (int)fmaxf(0, device->buttonCount - 4 * count);
    for (int i = 0; buttons && i < device->buttonCount && i < ARCADE_BUTTONS; ++i)
        device->buttons[i] = buttons[i];
#else
    (void)id;
#endif
}

static bool BindingDown(ArcadeBinding b, const ArcadeDevice *device, bool wasDown) {
    if (b.type == ARCADE_KEY) return IsKeyDown(b.index);
    if (!device || !device->guid[0]) return false;
    switch (b.type) {
        case ARCADE_BUTTON: return device->buttons[b.index] != 0;
        case ARCADE_AXIS: return device->axes[b.index] * b.direction > (wasDown ? .35f : .60f);
        case ARCADE_HAT: return (device->hats[b.index] & b.direction) != 0;
        default: return false;
    }
}

static ArcadeBinding NewRawBinding(const ArcadeDevice *before, const ArcadeDevice *now) {
    if (!now->guid[0] || strcmp(before->guid, now->guid) != 0) return (ArcadeBinding){0};
    for (int i = 0; i < ARCADE_HATS; ++i) {
        int direction = now->hats[i];
        if (direction != before->hats[i] && direction && (direction & (direction - 1)) == 0)
            return (ArcadeBinding){ARCADE_HAT, i, direction};
    }
    for (int i = 0; i < ARCADE_BUTTONS; ++i)
        if (now->buttons[i] && !before->buttons[i]) return (ArcadeBinding){ARCADE_BUTTON, i, 0};
    for (int i = 0; i < ARCADE_AXES; ++i) {
        float old = before->axes[i], value = now->axes[i];
        if (value > .60f && old <= .60f) return (ArcadeBinding){ARCADE_AXIS, i, 1};
        if (value < -.60f && old >= -.60f) return (ArcadeBinding){ARCADE_AXIS, i, -1};
    }
    return (ArcadeBinding){0};
}

bool ArcadeCalibrationActive(void) { return arcadeCalibrating; }

void BeginArcadeCalibration(void) {
    arcadeDraft = (ArcadeProfile){0};
    arcadeStep = 0;
    arcadeDraftDevice = -1;
    arcadeCalibrating = true;
    arcadeWaitingForRelease = false;
    arcadeStatus = NULL;
    while (GetKeyPressed()) {}
    for (int i = 0; i < ARCADE_DEVICES; ++i) ReadArcadeDevice(i, &arcadeDevices[i]);
}

const char *ArcadeCalibrationPrompt(void) {
    int step = arcadeWaitingForRelease ? arcadeStep - 1 : arcadeStep;
    return step < ARCADE_ACTIONS ? arcadePrompts[step] : "Salvando controles";
}

const char *ArcadeCalibrationStatus(void) {
    if (arcadeStatus) return arcadeStatus;
    if (arcadeWaitingForRelease) return "Solte o comando para continuar";
    return arcadeStep >= ARCADE_REQUIRED ? "Start ou F8 para pular  ·  F9 para cancelar" : "F9 para cancelar";
}

static void UpdateArcadeCalibration(void) {
    if (IsKeyPressed(KEY_F9)) {
        arcadeCalibrating = false;
        return;
    }
    ArcadeBinding binding = {0};
    int key;
    while ((key = GetKeyPressed()) != 0) {
        ArcadeBinding candidate = {ARCADE_KEY, key, 0};
        if (!binding.type && IsBindingValid(candidate) && IsKeyPressed(key)) binding = candidate;
    }
    int source = -1;
    for (int i = 0; i < ARCADE_DEVICES; ++i) {
        ArcadeDevice device;
        ReadArcadeDevice(i, &device);
        if ((arcadeDraftDevice < 0 || arcadeDraftDevice == i) && !binding.type) {
            binding = NewRawBinding(&arcadeDevices[i], &device);
            if (binding.type) source = i;
        }
        arcadeDevices[i] = device;
    }
    if (arcadeWaitingForRelease) {
        const ArcadeDevice *device = arcadeDraftDevice >= 0 ? &arcadeDevices[arcadeDraftDevice] : NULL;
        if (BindingDown(arcadeDraft.bindings[arcadeStep - 1], device, true)) return;
        arcadeWaitingForRelease = false;
        arcadeStatus = NULL;
    }
    if (arcadeStep == ARCADE_ACTIONS ||
        (arcadeStep == ARCADE_REQUIRED && (IsKeyPressed(KEY_F8) ||
         BindingsMatch(binding, arcadeDraft.bindings[ARCADE_REQUIRED - 1])))) {
        arcadeProfile = arcadeDraft;
        arcadeDevice = arcadeDraftDevice;
        usingArcade = true;
        arcadeCalibrating = false;
        if (!SaveArcadeDraft())
            TraceLog(LOG_WARNING, "ARCADE: Mapeamento ativo, mas nao foi possivel salvar arcade_pif.cfg");
        return;
    }
    if (!binding.type) return;
    for (int i = 0; i < arcadeStep; ++i) {
        if (!BindingsMatch(arcadeDraft.bindings[i], binding)) continue;
        arcadeStatus = "Este comando já está em uso. Escolha outro.";
        return;
    }
    if (source >= 0 && arcadeDraftDevice < 0) {
        arcadeDraftDevice = source;
        memcpy(arcadeDraft.guid, arcadeDevices[source].guid, sizeof(arcadeDraft.guid));
    }
    arcadeDraft.bindings[arcadeStep++] = binding;
    arcadeStatus = NULL;
    arcadeWaitingForRelease = true;
}

static bool IsGenericArcadeDevice(int id) {
#ifdef PIF_GLFW_ARCADE
    if (!glfwJoystickPresent(id)) return false;
    if (!glfwJoystickIsGamepad(id)) return true;
    const char *raw = glfwGetJoystickName(id);
    char name[128] = {0};
    for (int i = 0; raw && raw[i] && i < (int)sizeof(name) - 1; ++i)
        name[i] = (char)tolower((unsigned char)raw[i]);
    return strstr(name, "encoder") || strstr(name, "zero delay") ||
           strstr(name, "dragonrise") || strstr(name, "generic usb joystick");
#else
    (void)id;
    return false;
#endif
}

static int FindArcadeDevice(ArcadeDevice *device) {
    // Preserve a calibrated port first, including after unplugging and reconnecting.
    if (arcadeProfile.guid[0]) {
        if (arcadeDevice >= 0) {
            ReadArcadeDevice(arcadeDevice, device);
            if (strcmp(device->guid, arcadeProfile.guid) == 0) return arcadeDevice;
        }
        for (int i = 0; i < ARCADE_DEVICES; ++i) {
            ReadArcadeDevice(i, device);
            if (strcmp(device->guid, arcadeProfile.guid) == 0) return i;
        }
    }
    for (int i = 0; i < ARCADE_DEVICES; ++i) {
        if (!IsGenericArcadeDevice(i)) continue;
        ReadArcadeDevice(i, device);
        if (device->guid[0]) return i;
    }
    return -1;
}

static bool DefaultArcadeDown(const ArcadeDevice *device, int action, bool wasDown) {
    if (action < 4) {
        static const int hats[] = {1, 2, 4, 8};
        static const int axes[] = {1, 0, 1, 0};
        static const int signs[] = {-1, 1, 1, -1};
        return (device->hats[0] & hats[action]) ||
               device->axes[axes[action]] * signs[action] > (wasDown ? .35f : .55f);
    }
    // Encoders label the physical ports starting at 1, while GLFW starts at 0.
    static const int primaryButtons[] = {0, 1, 2, 3, 4};
    if (device->buttons[primaryButtons[action - 4]]) return true;
    return action == ARCADE_REQUIRED - 1 && (device->buttons[8] || device->buttons[9]);
}

static bool UpdateArcadeButtons(void) {
    ArcadeDevice device = {0};
    arcadeDevice = FindArcadeDevice(&device);
    bool calibratedDevice = arcadeDevice >= 0 && strcmp(device.guid, arcadeProfile.guid) == 0;
    arcadeAutomatic = arcadeDevice >= 0 && !calibratedDevice;
    rawButtonPressed = false;
    for (int i = 0; i < ARCADE_BUTTONS; ++i) {
        bool down = arcadeAutomatic && device.buttons[i];
        rawButtonPressed |= down && !lastRawButtons[i];
        lastRawButtons[i] = down;
    }
    bool pressed = rawButtonPressed;
    for (int i = 0; i < ARCADE_ACTIONS; ++i) {
        bool down = BindingDown(arcadeProfile.bindings[i], calibratedDevice ? &device : NULL, arcadeDown[i]) ||
                    (arcadeAutomatic && DefaultArcadeDown(&device, i, arcadeDown[i]));
        arcadePressed[i] = down && !arcadeDown[i];
        arcadeDown[i] = down;
        pressed |= arcadePressed[i];
    }
    return pressed;
}

static bool IsArcadeKey(int key) {
    for (int i = 0; i < ARCADE_ACTIONS; ++i) {
        ArcadeBinding b = arcadeProfile.bindings[i];
        if (b.type == ARCADE_KEY && b.index == key) return true;
    }
    return false;
}

bool KeyPressed(int key) { return !IsArcadeKey(key) && IsKeyPressed(key); }
bool KeyDown(int key) { return !IsArcadeKey(key) && IsKeyDown(key); }

int TextInputPressed(void) {
    if (!usingArcade) return GetCharPressed();
    while (GetCharPressed()) {}
    return 0;
}

static unsigned inputFrame;
static bool usingGamepad;
static bool playStation;
static bool padButtonPressed;
static int gamepad = -1;
static int stickHorizontal, stickVertical;
static Vector2 lastLeft, lastRight;
static bool navigate[4];

bool PadPressed(int button) {
    for (int i = 0; i < ARCADE_ACTIONS; ++i)
        if (arcadePressed[i] && arcadeButtons[i] == button) return true;
    return gamepad >= 0 && IsGamepadButtonPressed(gamepad, button);
}

bool PadButtonPressed(void) { return padButtonPressed; }

Vector2 PadStick(int xAxis, int yAxis) {
    if (gamepad < 0) return (Vector2){0};
    const float deadzone = .22f;
    Vector2 stick = {GetGamepadAxisMovement(gamepad, xAxis), GetGamepadAxisMovement(gamepad, yAxis)};
    float magnitude = Vector2Length(stick);
    if (magnitude <= deadzone) return (Vector2){0};
    float strength = (fminf(magnitude, 1.0f) - deadzone) / (1.0f - deadzone);
    return Vector2Scale(stick, strength / magnitude);
}

Vector2 PadMovement(void) {
    bool direction[4];
    for (int i = 0; i < 4; ++i)
        direction[i] = usingArcade ? arcadeDown[i] :
                       gamepad >= 0 && IsGamepadButtonDown(gamepad, arcadeButtons[i]);
    Vector2 digital = {direction[1] - direction[3], direction[2] - direction[0]};
    if (Vector2LengthSqr(digital) > 0.0f) return Vector2Normalize(digital);
    if (usingArcade) return digital;
    return PadStick(GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y);
}

static int StickDirection(float value, int previous) {
    if (value > .52f) return 1;
    if (value < -.52f) return -1;
    return fabsf(value) < .30f ? 0 : previous;
}

void UpdateGameInput(void) {
    inputFrame++;
    if (!arcadeLoaded) LoadArcadeProfile();
    if (arcadeCalibrating) {
        UpdateArcadeCalibration();
        return;
    }
    bool arcadeActivity = UpdateArcadeButtons();
    gamepad = -1;
    for (int id = 0; id < 4; ++id) {
        if (id == arcadeDevice || !IsGamepadAvailable(id)) continue;
#ifdef PIF_GLFW_ARCADE
        if (glfwJoystickPresent(id) && !glfwJoystickIsGamepad(id)) continue;
#endif
        gamepad = id;
        break;
    }
    if (arcadeActivity) usingArcade = true;
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

    bool standardButton = false;
    for (int button = GAMEPAD_BUTTON_LEFT_FACE_UP; gamepad >= 0 && button <= GAMEPAD_BUTTON_RIGHT_THUMB; button++)
        standardButton |= IsGamepadButtonPressed(gamepad, button);
    padButtonPressed = arcadeActivity || standardButton;
    bool key = false;
    for (int code = KEY_SPACE; code <= KEY_KB_MENU && !key; code++)
        key = KeyPressed(code);
    Vector2 mouse = GetMouseDelta();
    bool mouseButton = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                       IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                       IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    if (arcadeActivity) {
        usingGamepad = false;
    } else if (key || mouseButton) {
        usingArcade = false;
        usingGamepad = false;
    } else if (standardButton || movedStick) {
        usingArcade = false;
        usingGamepad = true;
    } else if (!usingArcade && (Vector2LengthSqr(mouse) >= 4.0f || GetMouseWheelMove() != 0.0f)) {
        usingGamepad = false;
    }

    if (usingGamepad) {
        char name[128] = {0};
        const char *raw = gamepad >= 0 ? GetGamepadName(gamepad) : NULL;
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
    int index = direction - GAMEPAD_BUTTON_LEFT_FACE_UP;
    return index >= 0 && index < 4 && (PadPressed(direction) || navigate[index]);
}

unsigned GameInputFrame(void) {
    return inputFrame;
}

bool UsingGamepad(void) {
    return usingArcade || (usingGamepad && gamepad >= 0);
}

bool UsingArcade(void) { return usingArcade; }

bool UsingAutomaticArcade(void) { return usingArcade && arcadeAutomatic; }

bool ArcadeRawButtonPressed(void) { return rawButtonPressed; }

const char *InputHint(const char *keyboardMouse, const char *gamepad) {
    if (!UsingGamepad()) return keyboardMouse;
    if (playStation && !usingArcade) return gamepad;
    // Pad hints use PlayStation names in brackets. Translate those tokens for Xbox.
    static char buffers[8][256];
    static unsigned index;
    char *line = buffers[index++ % 8];
    static const char *const labels[][4] = {
        {"[Cruz]", "[A]", "[Ataque]", "[Botão 1]"}, {"[Círculo]", "[B]", "[Esquiva]", "[Botão 2]"},
        {"[Quadrado]", "[X]", "[Interação]", "[Botão 3]"}, {"[Triângulo]", "[Y]", "[Extra]", "[Botão 5]"},
        {"[Options]", "[Menu]", "[Start]", "[Start / Botão 4]"}, {"[Share]", "[View]", "[Interação]", "[Botão 3]"},
        {"R1/R2", "RB/RT", "Ataque", "Botão 1"}, {"L1/L2", "LB/LT", "Esquiva", "Botão 2"},
        {"L1/R1", "LB/RB", "Interação", "Botão 3"}
    };
    int labelColumn = usingArcade ? (arcadeAutomatic ? 3 : 2) : 1;
    size_t used = 0;
    while (*gamepad && used < sizeof(buffers[0]) - 1) {
        bool replaced = false;
        for (int i = 0; i < (int)(sizeof(labels) / sizeof(labels[0])); i++) {
            size_t length = strlen(labels[i][0]);
            if (strncmp(gamepad, labels[i][0], length) != 0) continue;
            const char *label = labels[i][labelColumn];
            size_t output = strlen(label);
            if (used + output >= sizeof(buffers[0])) break;
            memcpy(line + used, label, output);
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
