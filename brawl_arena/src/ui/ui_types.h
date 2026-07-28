#ifndef BRAWL_UI_TYPES_H
#define BRAWL_UI_TYPES_H

#include "raylib.h"
#include <stdbool.h>

#define UI_REFERENCE_WIDTH 1280.0f
#define UI_REFERENCE_HEIGHT 800.0f
#define UI_MAX_FOCUS_NODES 96

typedef unsigned int UiId;

typedef enum UiInputModality {
    UI_INPUT_POINTER = 0,
    UI_INPUT_KEYBOARD,
    UI_INPUT_GAMEPAD
} UiInputModality;

typedef enum UiGlyphMode {
    UI_GLYPH_AUTO = 0,
    UI_GLYPH_KEYBOARD_MOUSE,
    UI_GLYPH_GAMEPAD,
    UI_GLYPH_MODE_COUNT
} UiGlyphMode;

typedef enum UiTextRole {
    UI_TEXT_DISPLAY = 0,
    UI_TEXT_TITLE,
    UI_TEXT_HEADING,
    UI_TEXT_BODY,
    UI_TEXT_EMPHASIS,
    UI_TEXT_LABEL,
    UI_TEXT_CAPTION,
    UI_TEXT_DATA,
    UI_TEXT_RESULT,
    UI_TEXT_ROLE_COUNT
} UiTextRole;

typedef enum UiAlign {
    UI_ALIGN_LEFT = 0,
    UI_ALIGN_CENTER,
    UI_ALIGN_RIGHT
} UiAlign;

typedef enum UiButtonStyle {
    UI_BUTTON_STANDARD = 0,
    UI_BUTTON_PRIMARY,
    UI_BUTTON_UTILITY,
    UI_BUTTON_DANGER
} UiButtonStyle;

typedef struct UiResponse {
    bool hovered;
    bool focused;
    bool held;
    bool activated;
} UiResponse;

typedef struct UiFrameLayout {
    int viewportWidth;
    int viewportHeight;
    float viewportScale;
    float preferenceScale;
    float scale;
    Vector2 origin;
    Rectangle safe;
    Rectangle content;
} UiFrameLayout;

typedef struct UiFocusNode {
    UiId id;
    Rectangle bounds;
    bool enabled;
} UiFocusNode;

#endif
