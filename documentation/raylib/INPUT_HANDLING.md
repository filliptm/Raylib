# Input Handling in Raylib

This comprehensive guide covers all aspects of handling user input in Raylib, including keyboard, mouse, gamepad, and touch input.

## Table of Contents
1. [Input Fundamentals](#input-fundamentals)
2. [Keyboard Input](#keyboard-input)
3. [Mouse Input](#mouse-input)
4. [Gamepad Input](#gamepad-input)
5. [Touch Input](#touch-input)
6. [Gesture Recognition](#gesture-recognition)
7. [Input Mapping](#input-mapping)
8. [Advanced Input Techniques](#advanced-input-techniques)
9. [Input Recording and Replay](#input-recording-and-replay)
10. [Best Practices](#best-practices)

## Input Fundamentals

### Input States
Raylib provides three states for input detection:
- **Pressed**: Triggered once when input starts
- **Down**: Continuous while input is held
- **Released**: Triggered once when input ends

### Basic Input Loop
```c
#include "raylib.h"

int main(void)
{
    InitWindow(800, 450, "raylib - input basics");
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        // Input handling
        if (IsKeyPressed(KEY_SPACE)) {
            // Triggered once when space is pressed
        }
        
        if (IsKeyDown(KEY_W)) {
            // Continuous while W is held
        }
        
        if (IsKeyReleased(KEY_ESCAPE)) {
            // Triggered once when ESC is released
        }
        
        BeginDrawing();
            ClearBackground(RAYWHITE);
            // Draw game
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
```

## Keyboard Input

### Basic Keyboard Functions
```c
// Check key states
bool IsKeyPressed(int key);      // Just pressed
bool IsKeyDown(int key);         // Being held
bool IsKeyReleased(int key);     // Just released
bool IsKeyUp(int key);           // Not being pressed

// Get pressed key (useful for text input)
int GetKeyPressed(void);         // Get key code
int GetCharPressed(void);        // Get unicode character

// Special functions
void SetExitKey(int key);        // Change exit key (default: ESC)
```

### Key Codes
```c
// Common keys
KEY_SPACE, KEY_ENTER, KEY_TAB, KEY_BACKSPACE, KEY_ESCAPE
KEY_DELETE, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP
KEY_PAGE_UP, KEY_PAGE_DOWN, KEY_HOME, KEY_END
KEY_CAPS_LOCK, KEY_SCROLL_LOCK, KEY_NUM_LOCK
KEY_PRINT_SCREEN, KEY_PAUSE

// Function keys
KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6
KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12

// Modifier keys
KEY_LEFT_SHIFT, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_LEFT_SUPER
KEY_RIGHT_SHIFT, KEY_RIGHT_CONTROL, KEY_RIGHT_ALT, KEY_RIGHT_SUPER

// Alphanumeric
KEY_ZERO to KEY_NINE (0-9)
KEY_A to KEY_Z (A-Z)

// Keypad
KEY_KP_0 to KEY_KP_9
KEY_KP_DECIMAL, KEY_KP_DIVIDE, KEY_KP_MULTIPLY
KEY_KP_SUBTRACT, KEY_KP_ADD, KEY_KP_ENTER
```

### Text Input System
```c
typedef struct {
    char text[256];
    int letterCount;
    bool isActive;
    int cursorPos;
    int selectionStart;
    int selectionEnd;
} TextInput;

void UpdateTextInput(TextInput *input) {
    if (!input->isActive) return;
    
    // Get typed characters
    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (input->letterCount < 255)) {
            input->text[input->letterCount] = (char)key;
            input->text[input->letterCount + 1] = '\0';
            input->letterCount++;
        }
        key = GetCharPressed();
    }
    
    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE) && input->letterCount > 0) {
        input->letterCount--;
        input->text[input->letterCount] = '\0';
    }
    
    // Handle enter
    if (IsKeyPressed(KEY_ENTER)) {
        input->isActive = false;
    }
}

void DrawTextInput(TextInput *input, int x, int y) {
    DrawRectangle(x - 2, y - 2, 204, 24, input->isActive ? BLUE : GRAY);
    DrawRectangle(x, y, 200, 20, RAYWHITE);
    DrawText(input->text, x + 5, y + 2, 20, MAROON);
    
    // Draw cursor
    if (input->isActive && ((GetTime() * 2) - (int)(GetTime() * 2) > 0.5f)) {
        int textWidth = MeasureText(input->text, 20);
        DrawLine(x + 5 + textWidth, y + 2, x + 5 + textWidth, y + 18, MAROON);
    }
}
```

### Advanced Keyboard Handling
```c
// Multi-key combinations
typedef struct {
    int key;
    int modifier;
} KeyCombo;

bool IsKeyComboPressed(KeyCombo combo) {
    return IsKeyPressed(combo.key) && IsKeyDown(combo.modifier);
}

// Key repeat system
typedef struct {
    int key;
    float repeatDelay;
    float repeatRate;
    float timer;
    bool isRepeating;
} KeyRepeat;

bool UpdateKeyRepeat(KeyRepeat *repeat, float deltaTime) {
    if (IsKeyPressed(repeat->key)) {
        repeat->timer = 0;
        repeat->isRepeating = false;
        return true;
    }
    
    if (IsKeyDown(repeat->key)) {
        repeat->timer += deltaTime;
        
        if (!repeat->isRepeating && repeat->timer >= repeat->repeatDelay) {
            repeat->isRepeating = true;
            repeat->timer = 0;
            return true;
        }
        
        if (repeat->isRepeating && repeat->timer >= repeat->repeatRate) {
            repeat->timer = 0;
            return true;
        }
    } else {
        repeat->timer = 0;
        repeat->isRepeating = false;
    }
    
    return false;
}
```

## Mouse Input

### Basic Mouse Functions
```c
// Button states
bool IsMouseButtonPressed(int button);
bool IsMouseButtonDown(int button);
bool IsMouseButtonReleased(int button);
bool IsMouseButtonUp(int button);

// Position
int GetMouseX(void);
int GetMouseY(void);
Vector2 GetMousePosition(void);
Vector2 GetMouseDelta(void);      // Movement since last frame
void SetMousePosition(int x, int y);

// Wheel
float GetMouseWheelMove(void);     // Vertical wheel movement
Vector2 GetMouseWheelMoveV(void);  // Horizontal and vertical

// Cursor
void SetMouseCursor(int cursor);
void SetMouseOffset(int offsetX, int offsetY);
void SetMouseScale(float scaleX, float scaleY);
```

### Mouse Buttons and Cursors
```c
// Mouse buttons
MOUSE_BUTTON_LEFT    (MOUSE_LEFT_BUTTON)
MOUSE_BUTTON_RIGHT   (MOUSE_RIGHT_BUTTON)
MOUSE_BUTTON_MIDDLE  (MOUSE_MIDDLE_BUTTON)
MOUSE_BUTTON_SIDE    
MOUSE_BUTTON_EXTRA   
MOUSE_BUTTON_FORWARD 
MOUSE_BUTTON_BACK    

// Mouse cursors
MOUSE_CURSOR_DEFAULT
MOUSE_CURSOR_ARROW
MOUSE_CURSOR_IBEAM
MOUSE_CURSOR_CROSSHAIR
MOUSE_CURSOR_POINTING_HAND
MOUSE_CURSOR_RESIZE_EW     // Horizontal resize
MOUSE_CURSOR_RESIZE_NS     // Vertical resize
MOUSE_CURSOR_RESIZE_NWSE   // Diagonal resize
MOUSE_CURSOR_RESIZE_NESW   // Diagonal resize
MOUSE_CURSOR_RESIZE_ALL
MOUSE_CURSOR_NOT_ALLOWED
```

### Drag and Drop
```c
typedef struct {
    bool isDragging;
    Vector2 dragStart;
    Vector2 dragOffset;
    Rectangle bounds;
} DraggableObject;

void UpdateDraggable(DraggableObject *obj) {
    Vector2 mousePos = GetMousePosition();
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && 
        CheckCollisionPointRec(mousePos, obj->bounds)) {
        obj->isDragging = true;
        obj->dragStart = mousePos;
        obj->dragOffset = (Vector2){
            mousePos.x - obj->bounds.x,
            mousePos.y - obj->bounds.y
        };
    }
    
    if (obj->isDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            obj->bounds.x = mousePos.x - obj->dragOffset.x;
            obj->bounds.y = mousePos.y - obj->dragOffset.y;
        } else {
            obj->isDragging = false;
        }
    }
}

// File drag and drop
void HandleFileDrop() {
    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();
        
        for (int i = 0; i < droppedFiles.count; i++) {
            TraceLog(LOG_INFO, "Dropped file: %s", droppedFiles.paths[i]);
            
            // Process file based on extension
            if (IsFileExtension(droppedFiles.paths[i], ".png")) {
                // Load as texture
            } else if (IsFileExtension(droppedFiles.paths[i], ".ogg")) {
                // Load as sound
            }
        }
        
        UnloadDroppedFiles(droppedFiles);
    }
}
```

### Mouse Gestures
```c
typedef struct {
    Vector2 startPos;
    Vector2 endPos;
    float startTime;
    bool isActive;
} MouseGesture;

typedef enum {
    GESTURE_NONE,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN
} GestureType;

GestureType DetectMouseGesture(MouseGesture *gesture, float maxTime, float minDistance) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        gesture->startPos = GetMousePosition();
        gesture->startTime = GetTime();
        gesture->isActive = true;
    }
    
    if (gesture->isActive && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        gesture->endPos = GetMousePosition();
        gesture->isActive = false;
        
        float deltaTime = GetTime() - gesture->startTime;
        if (deltaTime <= maxTime) {
            Vector2 diff = Vector2Subtract(gesture->endPos, gesture->startPos);
            float distance = Vector2Length(diff);
            
            if (distance >= minDistance) {
                if (fabsf(diff.x) > fabsf(diff.y)) {
                    return diff.x > 0 ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
                } else {
                    return diff.y > 0 ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
                }
            }
        }
    }
    
    return GESTURE_NONE;
}
```

## Gamepad Input

### Basic Gamepad Functions
```c
// Check availability
bool IsGamepadAvailable(int gamepad);
const char *GetGamepadName(int gamepad);

// Button input
bool IsGamepadButtonPressed(int gamepad, int button);
bool IsGamepadButtonDown(int gamepad, int button);
bool IsGamepadButtonReleased(int gamepad, int button);
bool IsGamepadButtonUp(int gamepad, int button);
int GetGamepadButtonPressed(void);

// Axis input
int GetGamepadAxisCount(int gamepad);
float GetGamepadAxisMovement(int gamepad, int axis);

// Mapping
int SetGamepadMappings(const char *mappings);
```

### Gamepad Enums
```c
// Gamepad buttons
GAMEPAD_BUTTON_LEFT_FACE_UP     // D-pad up
GAMEPAD_BUTTON_LEFT_FACE_RIGHT  // D-pad right
GAMEPAD_BUTTON_LEFT_FACE_DOWN   // D-pad down
GAMEPAD_BUTTON_LEFT_FACE_LEFT   // D-pad left

GAMEPAD_BUTTON_RIGHT_FACE_UP    // Triangle/Y
GAMEPAD_BUTTON_RIGHT_FACE_RIGHT // Circle/B
GAMEPAD_BUTTON_RIGHT_FACE_DOWN  // Cross/A
GAMEPAD_BUTTON_RIGHT_FACE_LEFT  // Square/X

GAMEPAD_BUTTON_LEFT_TRIGGER_1   // L1/LB
GAMEPAD_BUTTON_LEFT_TRIGGER_2   // L2/LT
GAMEPAD_BUTTON_RIGHT_TRIGGER_1  // R1/RB
GAMEPAD_BUTTON_RIGHT_TRIGGER_2  // R2/RT

GAMEPAD_BUTTON_MIDDLE_LEFT      // Select/Back
GAMEPAD_BUTTON_MIDDLE           // PS/Xbox button
GAMEPAD_BUTTON_MIDDLE_RIGHT     // Start/Menu

GAMEPAD_BUTTON_LEFT_THUMB       // L3
GAMEPAD_BUTTON_RIGHT_THUMB      // R3

// Gamepad axes
GAMEPAD_AXIS_LEFT_X         // Left stick X
GAMEPAD_AXIS_LEFT_Y         // Left stick Y
GAMEPAD_AXIS_RIGHT_X        // Right stick X
GAMEPAD_AXIS_RIGHT_Y        // Right stick Y
GAMEPAD_AXIS_LEFT_TRIGGER   // L2/LT [-1..1]
GAMEPAD_AXIS_RIGHT_TRIGGER  // R2/RT [-1..1]
```

### Gamepad Manager
```c
typedef struct {
    int gamepadId;
    bool connected;
    Vector2 leftStick;
    Vector2 rightStick;
    float leftTrigger;
    float rightTrigger;
    float deadzone;
    float rumbleTime;
} GamepadState;

#define MAX_GAMEPADS 4
GamepadState gamepads[MAX_GAMEPADS];

void InitGamepadManager() {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        gamepads[i].gamepadId = i;
        gamepads[i].connected = false;
        gamepads[i].deadzone = 0.2f;
    }
}

void UpdateGamepadManager() {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        gamepads[i].connected = IsGamepadAvailable(i);
        
        if (gamepads[i].connected) {
            // Update sticks with deadzone
            float leftX = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_X);
            float leftY = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_Y);
            
            if (Vector2Length((Vector2){leftX, leftY}) < gamepads[i].deadzone) {
                gamepads[i].leftStick = (Vector2){0, 0};
            } else {
                gamepads[i].leftStick = (Vector2){leftX, leftY};
            }
            
            // Same for right stick
            float rightX = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_X);
            float rightY = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_Y);
            
            if (Vector2Length((Vector2){rightX, rightY}) < gamepads[i].deadzone) {
                gamepads[i].rightStick = (Vector2){0, 0};
            } else {
                gamepads[i].rightStick = (Vector2){rightX, rightY};
            }
            
            // Update triggers
            gamepads[i].leftTrigger = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_TRIGGER);
            gamepads[i].rightTrigger = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_TRIGGER);
        }
    }
}
```

### Gamepad Vibration (Conceptual)
```c
// Note: Raylib doesn't have built-in vibration support
// This is a conceptual implementation

typedef struct {
    float leftMotor;   // 0.0 to 1.0
    float rightMotor;  // 0.0 to 1.0
    float duration;
} VibrationEffect;

void SetGamepadVibration(int gamepad, VibrationEffect effect) {
    // Platform-specific implementation would go here
    #ifdef _WIN32
        // XInput vibration
    #elif defined(__linux__)
        // Linux joystick vibration
    #endif
}

// Vibration patterns
void PlayVibrationPattern(int gamepad, const char *pattern) {
    if (strcmp(pattern, "explosion") == 0) {
        SetGamepadVibration(gamepad, (VibrationEffect){1.0f, 1.0f, 0.5f});
    } else if (strcmp(pattern, "hit") == 0) {
        SetGamepadVibration(gamepad, (VibrationEffect){0.5f, 0.0f, 0.2f});
    } else if (strcmp(pattern, "heartbeat") == 0) {
        // Would need to implement pulsing pattern
    }
}
```

## Touch Input

### Basic Touch Functions
```c
// Get touch points
int GetTouchX(void);                    // Touch point 0 X
int GetTouchY(void);                    // Touch point 0 Y
Vector2 GetTouchPosition(int index);    // Get specific touch point
int GetTouchPointId(int index);         // Get touch point identifier
int GetTouchPointCount(void);           // Number of touch points
```

### Multi-touch Handling
```c
typedef struct {
    int id;
    Vector2 position;
    Vector2 startPosition;
    float startTime;
    bool active;
} TouchPoint;

typedef struct {
    TouchPoint touches[10];
    int touchCount;
} TouchManager;

void UpdateTouchManager(TouchManager *manager) {
    int currentTouchCount = GetTouchPointCount();
    
    // Update existing touches and detect new ones
    for (int i = 0; i < currentTouchCount; i++) {
        int id = GetTouchPointId(i);
        Vector2 pos = GetTouchPosition(i);
        
        // Find existing touch or create new
        bool found = false;
        for (int j = 0; j < manager->touchCount; j++) {
            if (manager->touches[j].id == id) {
                manager->touches[j].position = pos;
                found = true;
                break;
            }
        }
        
        if (!found && manager->touchCount < 10) {
            manager->touches[manager->touchCount].id = id;
            manager->touches[manager->touchCount].position = pos;
            manager->touches[manager->touchCount].startPosition = pos;
            manager->touches[manager->touchCount].startTime = GetTime();
            manager->touches[manager->touchCount].active = true;
            manager->touchCount++;
        }
    }
    
    // Mark inactive touches
    for (int i = 0; i < manager->touchCount; i++) {
        bool stillActive = false;
        for (int j = 0; j < currentTouchCount; j++) {
            if (manager->touches[i].id == GetTouchPointId(j)) {
                stillActive = true;
                break;
            }
        }
        manager->touches[i].active = stillActive;
    }
    
    // Clean up inactive touches
    int writeIndex = 0;
    for (int i = 0; i < manager->touchCount; i++) {
        if (manager->touches[i].active) {
            if (writeIndex != i) {
                manager->touches[writeIndex] = manager->touches[i];
            }
            writeIndex++;
        }
    }
    manager->touchCount = writeIndex;
}
```

### Touch Gestures
```c
// Pinch-to-zoom
typedef struct {
    float scale;
    float baseDistance;
    bool active;
} PinchGesture;

void UpdatePinchGesture(PinchGesture *pinch, TouchManager *touches) {
    if (touches->touchCount == 2) {
        float distance = Vector2Distance(
            touches->touches[0].position,
            touches->touches[1].position
        );
        
        if (!pinch->active) {
            pinch->active = true;
            pinch->baseDistance = distance;
            pinch->scale = 1.0f;
        } else {
            pinch->scale = distance / pinch->baseDistance;
        }
    } else {
        pinch->active = false;
    }
}

// Touch button
typedef struct {
    Rectangle bounds;
    bool pressed;
    bool down;
    bool released;
} TouchButton;

void UpdateTouchButton(TouchButton *button, TouchManager *touches) {
    bool wasDown = button->down;
    button->down = false;
    
    for (int i = 0; i < touches->touchCount; i++) {
        if (CheckCollisionPointRec(touches->touches[i].position, button->bounds)) {
            button->down = true;
            break;
        }
    }
    
    button->pressed = !wasDown && button->down;
    button->released = wasDown && !button->down;
}
```

## Gesture Recognition

### Built-in Gestures
```c
// Enable gestures
SetGesturesEnabled(unsigned int flags);

// Gesture detection
bool IsGestureDetected(int gesture);
int GetGestureDetected(void);
float GetGestureHoldDuration(void);
Vector2 GetGestureDragVector(void);
float GetGestureDragAngle(void);
Vector2 GetGesturePinchVector(void);
float GetGesturePinchAngle(void);

// Gesture types
GESTURE_NONE
GESTURE_TAP
GESTURE_DOUBLETAP
GESTURE_HOLD
GESTURE_DRAG
GESTURE_SWIPE_RIGHT
GESTURE_SWIPE_LEFT
GESTURE_SWIPE_UP
GESTURE_SWIPE_DOWN
GESTURE_PINCH_IN
GESTURE_PINCH_OUT
```

### Custom Gesture System
```c
typedef struct {
    Vector2 points[100];
    int pointCount;
    float totalLength;
} GesturePath;

typedef struct {
    const char *name;
    Vector2 *template;
    int templateSize;
} GestureTemplate;

// Record gesture path
void RecordGesturePath(GesturePath *path) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && path->pointCount < 100) {
        Vector2 currentPos = GetMousePosition();
        
        if (path->pointCount == 0 || 
            Vector2Distance(currentPos, path->points[path->pointCount-1]) > 5.0f) {
            path->points[path->pointCount] = currentPos;
            
            if (path->pointCount > 0) {
                path->totalLength += Vector2Distance(
                    path->points[path->pointCount-1], 
                    currentPos
                );
            }
            
            path->pointCount++;
        }
    }
    
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        // Gesture complete, process it
    }
}

// Simple gesture matching (using normalized direction vectors)
float MatchGesture(GesturePath *path, GestureTemplate *template) {
    if (path->pointCount < 2 || template->templateSize < 2) return 0.0f;
    
    // Normalize both paths to same number of points
    int comparePoints = 20;
    Vector2 normalizedPath[20];
    Vector2 normalizedTemplate[20];
    
    // Sample path at regular intervals
    for (int i = 0; i < comparePoints; i++) {
        float t = (float)i / (comparePoints - 1);
        int pathIndex = (int)(t * (path->pointCount - 1));
        normalizedPath[i] = path->points[pathIndex];
        
        int templateIndex = (int)(t * (template->templateSize - 1));
        normalizedTemplate[i] = template->template[templateIndex];
    }
    
    // Compare directions
    float similarity = 0.0f;
    for (int i = 1; i < comparePoints; i++) {
        Vector2 pathDir = Vector2Normalize(
            Vector2Subtract(normalizedPath[i], normalizedPath[i-1])
        );
        Vector2 templateDir = Vector2Normalize(
            Vector2Subtract(normalizedTemplate[i], normalizedTemplate[i-1])
        );
        
        similarity += Vector2DotProduct(pathDir, templateDir);
    }
    
    return similarity / (comparePoints - 1);
}
```

## Input Mapping

### Action Mapping System
```c
typedef enum {
    ACTION_JUMP,
    ACTION_SHOOT,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_PAUSE,
    ACTION_COUNT
} GameAction;

typedef struct {
    int keyboard[2];     // Primary and secondary keys
    int gamepadButton;
    int mouseButton;
} ActionMapping;

typedef struct {
    ActionMapping actions[ACTION_COUNT];
    int activeGamepad;
} InputMapper;

InputMapper inputMapper = {
    .actions = {
        [ACTION_JUMP] = { {KEY_SPACE, KEY_W}, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, -1 },
        [ACTION_SHOOT] = { {KEY_LEFT_CONTROL, KEY_Z}, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, MOUSE_BUTTON_LEFT },
        [ACTION_MOVE_LEFT] = { {KEY_A, KEY_LEFT}, GAMEPAD_BUTTON_LEFT_FACE_LEFT, -1 },
        [ACTION_MOVE_RIGHT] = { {KEY_D, KEY_RIGHT}, GAMEPAD_BUTTON_LEFT_FACE_RIGHT, -1 },
        [ACTION_PAUSE] = { {KEY_ESCAPE, KEY_P}, GAMEPAD_BUTTON_MIDDLE_RIGHT, -1 }
    },
    .activeGamepad = 0
};

bool IsActionPressed(GameAction action) {
    ActionMapping *mapping = &inputMapper.actions[action];
    
    // Check keyboard
    if (IsKeyPressed(mapping->keyboard[0]) || IsKeyPressed(mapping->keyboard[1])) {
        return true;
    }
    
    // Check gamepad
    if (mapping->gamepadButton >= 0 && IsGamepadAvailable(inputMapper.activeGamepad)) {
        if (IsGamepadButtonPressed(inputMapper.activeGamepad, mapping->gamepadButton)) {
            return true;
        }
    }
    
    // Check mouse
    if (mapping->mouseButton >= 0 && IsMouseButtonPressed(mapping->mouseButton)) {
        return true;
    }
    
    return false;
}

float GetActionValue(GameAction action) {
    // For analog inputs
    switch (action) {
        case ACTION_MOVE_LEFT:
        case ACTION_MOVE_RIGHT: {
            float value = 0.0f;
            
            // Keyboard (digital)
            if (action == ACTION_MOVE_LEFT && IsKeyDown(KEY_A)) value = -1.0f;
            if (action == ACTION_MOVE_RIGHT && IsKeyDown(KEY_D)) value = 1.0f;
            
            // Gamepad (analog)
            if (IsGamepadAvailable(inputMapper.activeGamepad)) {
                float stickX = GetGamepadAxisMovement(inputMapper.activeGamepad, GAMEPAD_AXIS_LEFT_X);
                if (action == ACTION_MOVE_LEFT && stickX < -0.2f) value = stickX;
                if (action == ACTION_MOVE_RIGHT && stickX > 0.2f) value = stickX;
            }
            
            return value;
        }
        default:
            return IsActionPressed(action) ? 1.0f : 0.0f;
    }
}
```

### Rebindable Controls
```c
typedef struct {
    bool isRebinding;
    GameAction actionToRebind;
    int inputType; // 0: keyboard, 1: gamepad, 2: mouse
} RebindState;

void UpdateRebinding(RebindState *state, InputMapper *mapper) {
    if (!state->isRebinding) return;
    
    switch (state->inputType) {
        case 0: { // Keyboard
            int key = GetKeyPressed();
            if (key > 0) {
                mapper->actions[state->actionToRebind].keyboard[0] = key;
                state->isRebinding = false;
            }
            break;
        }
        
        case 1: { // Gamepad
            int button = GetGamepadButtonPressed();
            if (button >= 0) {
                mapper->actions[state->actionToRebind].gamepadButton = button;
                state->isRebinding = false;
            }
            break;
        }
        
        case 2: { // Mouse
            for (int i = 0; i < 7; i++) {
                if (IsMouseButtonPressed(i)) {
                    mapper->actions[state->actionToRebind].mouseButton = i;
                    state->isRebinding = false;
                    break;
                }
            }
            break;
        }
    }
}

// Save/Load input configuration
void SaveInputMapping(InputMapper *mapper, const char *filename) {
    SaveFileData(filename, mapper, sizeof(InputMapper));
}

void LoadInputMapping(InputMapper *mapper, const char *filename) {
    if (FileExists(filename)) {
        unsigned int dataSize = 0;
        unsigned char *data = LoadFileData(filename, &dataSize);
        if (data && dataSize == sizeof(InputMapper)) {
            memcpy(mapper, data, sizeof(InputMapper));
        }
        UnloadFileData(data);
    }
}
```

## Advanced Input Techniques

### Input Buffering
```c
typedef struct {
    GameAction action;
    float timestamp;
} BufferedInput;

typedef struct {
    BufferedInput buffer[32];
    int count;
    float bufferTime;  // How long inputs stay valid
} InputBuffer;

void AddToInputBuffer(InputBuffer *buffer, GameAction action) {
    if (buffer->count < 32) {
        buffer->buffer[buffer->count].action = action;
        buffer->buffer[buffer->count].timestamp = GetTime();
        buffer->count++;
    }
}

bool ConsumeBufferedInput(InputBuffer *buffer, GameAction action) {
    float currentTime = GetTime();
    
    for (int i = 0; i < buffer->count; i++) {
        if (buffer->buffer[i].action == action &&
            currentTime - buffer->buffer[i].timestamp < buffer->bufferTime) {
            // Remove consumed input
            for (int j = i; j < buffer->count - 1; j++) {
                buffer->buffer[j] = buffer->buffer[j + 1];
            }
            buffer->count--;
            return true;
        }
    }
    
    return false;
}

void CleanInputBuffer(InputBuffer *buffer) {
    float currentTime = GetTime();
    int writeIndex = 0;
    
    for (int i = 0; i < buffer->count; i++) {
        if (currentTime - buffer->buffer[i].timestamp < buffer->bufferTime) {
            if (writeIndex != i) {
                buffer->buffer[writeIndex] = buffer->buffer[i];
            }
            writeIndex++;
        }
    }
    
    buffer->count = writeIndex;
}
```

### Combo System
```c
typedef struct {
    GameAction *sequence;
    int length;
    const char *name;
    void (*execute)(void);
} Combo;

typedef struct {
    Combo *combos;
    int comboCount;
    GameAction history[16];
    float timestamps[16];
    int historyIndex;
    float comboTimeout;
} ComboSystem;

void AddInputToComboHistory(ComboSystem *system, GameAction action) {
    system->history[system->historyIndex] = action;
    system->timestamps[system->historyIndex] = GetTime();
    system->historyIndex = (system->historyIndex + 1) % 16;
}

bool CheckCombo(ComboSystem *system, Combo *combo) {
    float currentTime = GetTime();
    int matchCount = 0;
    
    // Start from most recent input and work backwards
    int historyPos = (system->historyIndex - 1 + 16) % 16;
    
    for (int i = combo->length - 1; i >= 0; i--) {
        // Check if input matches and is within timeout
        if (system->history[historyPos] == combo->sequence[i] &&
            currentTime - system->timestamps[historyPos] < system->comboTimeout) {
            matchCount++;
            historyPos = (historyPos - 1 + 16) % 16;
        } else {
            break;
        }
    }
    
    return matchCount == combo->length;
}

void UpdateComboSystem(ComboSystem *system) {
    // Check for completed combos
    for (int i = 0; i < system->comboCount; i++) {
        if (CheckCombo(system, &system->combos[i])) {
            TraceLog(LOG_INFO, "Combo executed: %s", system->combos[i].name);
            if (system->combos[i].execute) {
                system->combos[i].execute();
            }
            
            // Clear history to prevent repeat execution
            system->historyIndex = 0;
            break;
        }
    }
}
```

## Input Recording and Replay

### Input Recorder
```c
typedef struct {
    GameAction action;
    bool pressed;  // true for press, false for release
    float timestamp;
} RecordedInput;

typedef struct {
    RecordedInput *inputs;
    int capacity;
    int count;
    float startTime;
    bool isRecording;
    bool isReplaying;
    int replayIndex;
    float replayStartTime;
} InputRecorder;

void StartRecording(InputRecorder *recorder) {
    recorder->isRecording = true;
    recorder->count = 0;
    recorder->replayIndex = 0;
    recorder->startTime = GetTime();
}

void RecordInput(InputRecorder *recorder, GameAction action, bool pressed) {
    if (!recorder->isRecording || recorder->count >= recorder->capacity) return;
    
    recorder->inputs[recorder->count].action = action;
    recorder->inputs[recorder->count].pressed = pressed;
    recorder->inputs[recorder->count].timestamp = GetTime() - recorder->startTime;
    recorder->count++;
}

void StartReplay(InputRecorder *recorder) {
    recorder->isReplaying = true;
    recorder->isRecording = false;
    recorder->replayIndex = 0;
    recorder->replayStartTime = GetTime();
}

bool GetReplayInput(InputRecorder *recorder, GameAction action) {
    if (!recorder->isReplaying) return false;
    
    float replayTime = GetTime() - recorder->replayStartTime;
    bool isPressed = false;
    
    // Process all inputs up to current replay time
    while (recorder->replayIndex < recorder->count &&
           recorder->inputs[recorder->replayIndex].timestamp <= replayTime) {
        RecordedInput *input = &recorder->inputs[recorder->replayIndex];
        
        if (input->action == action) {
            isPressed = input->pressed;
        }
        
        recorder->replayIndex++;
    }
    
    // Check if replay is complete
    if (recorder->replayIndex >= recorder->count) {
        recorder->isReplaying = false;
    }
    
    return isPressed;
}

// Save/Load recordings
void SaveRecording(InputRecorder *recorder, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file) {
        fwrite(&recorder->count, sizeof(int), 1, file);
        fwrite(recorder->inputs, sizeof(RecordedInput), recorder->count, file);
        fclose(file);
    }
}

void LoadRecording(InputRecorder *recorder, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file) {
        fread(&recorder->count, sizeof(int), 1, file);
        recorder->count = (recorder->count > recorder->capacity) ? 
                         recorder->capacity : recorder->count;
        fread(recorder->inputs, sizeof(RecordedInput), recorder->count, file);
        fclose(file);
    }
}
```

## Best Practices

### Input State Management
```c
typedef struct {
    // Current frame state
    bool keyboard[512];
    bool mouse[7];
    bool gamepad[4][32];
    Vector2 mousePos;
    Vector2 mouseDelta;
    float mouseWheel;
    
    // Previous frame state (for edge detection)
    bool prevKeyboard[512];
    bool prevMouse[7];
    bool prevGamepad[4][32];
} InputState;

InputState inputState = {0};

void UpdateInputState() {
    // Save previous state
    memcpy(inputState.prevKeyboard, inputState.keyboard, sizeof(inputState.keyboard));
    memcpy(inputState.prevMouse, inputState.mouse, sizeof(inputState.mouse));
    memcpy(inputState.prevGamepad, inputState.gamepad, sizeof(inputState.gamepad));
    
    // Update current state
    for (int i = 0; i < 512; i++) {
        inputState.keyboard[i] = IsKeyDown(i);
    }
    
    for (int i = 0; i < 7; i++) {
        inputState.mouse[i] = IsMouseButtonDown(i);
    }
    
    inputState.mousePos = GetMousePosition();
    inputState.mouseDelta = GetMouseDelta();
    inputState.mouseWheel = GetMouseWheelMove();
    
    // Update gamepad states
    for (int g = 0; g < 4; g++) {
        if (IsGamepadAvailable(g)) {
            for (int b = 0; b < 32; b++) {
                inputState.gamepad[g][b] = IsGamepadButtonDown(g, b);
            }
        }
    }
}

bool WasKeyPressed(int key) {
    return inputState.keyboard[key] && !inputState.prevKeyboard[key];
}

bool WasKeyReleased(int key) {
    return !inputState.keyboard[key] && inputState.prevKeyboard[key];
}
```

### Input Debugging
```c
void DrawInputDebug(int x, int y) {
    // Keyboard state
    DrawText("Keyboard:", x, y, 20, BLACK);
    y += 25;
    
    for (int i = KEY_A; i <= KEY_Z; i++) {
        if (IsKeyDown(i)) {
            DrawText(TextFormat("%c ", i), x + ((i - KEY_A) % 10) * 30, 
                    y + ((i - KEY_A) / 10) * 25, 20, RED);
        }
    }
    y += 80;
    
    // Mouse state
    DrawText(TextFormat("Mouse: %.0f, %.0f", GetMouseX(), GetMouseY()), x, y, 20, BLACK);
    y += 25;
    
    for (int i = 0; i < 7; i++) {
        if (IsMouseButtonDown(i)) {
            DrawCircle(x + 20 + i * 30, y + 10, 10, RED);
        } else {
            DrawCircleLines(x + 20 + i * 30, y + 10, 10, BLACK);
        }
    }
    y += 40;
    
    // Gamepad state
    for (int g = 0; g < 4; g++) {
        if (IsGamepadAvailable(g)) {
            DrawText(TextFormat("Gamepad %d: %s", g, GetGamepadName(g)), x, y, 20, BLACK);
            y += 25;
            
            // Draw stick positions
            Vector2 leftStick = {
                GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_X),
                GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_Y)
            };
            DrawCircle(x + 50, y + 50, 40, LIGHTGRAY);
            DrawCircle(x + 50 + leftStick.x * 30, y + 50 + leftStick.y * 30, 10, RED);
            
            y += 100;
        }
    }
}
```

### Performance Tips

1. **Cache Input States**
   - Don't call input functions multiple times per frame
   - Store states at the beginning of the frame

2. **Use Appropriate Functions**
   - `IsKeyPressed()` for single actions
   - `IsKeyDown()` for continuous actions
   - `GetKeyPressed()` for text input

3. **Optimize Touch Handling**
   - Limit touch points to what you need
   - Use spatial partitioning for touch targets

4. **Input Prediction**
   ```c
   // For networked games
   void PredictInput(Vector2 *position, Vector2 velocity, float deltaTime) {
       position->x += velocity.x * deltaTime;
       position->y += velocity.y * deltaTime;
   }
   ```

Remember: Always provide alternative input methods for accessibility!