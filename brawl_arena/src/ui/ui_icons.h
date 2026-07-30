#ifndef BRAWL_UI_ICONS_H
#define BRAWL_UI_ICONS_H

#include "raylib.h"

typedef enum UiIcon {
    UI_ICON_BACK = 0,
    UI_ICON_CLOSE,
    UI_ICON_SETTINGS,
    UI_ICON_CONTROLS,
    UI_ICON_STUDIO,
    UI_ICON_PRACTICE,
    UI_ICON_QUIT,
    UI_ICON_PREVIOUS,
    UI_ICON_NEXT,
    UI_ICON_HEALTH,
    UI_ICON_DAMAGE,
    UI_ICON_RANGE,
    UI_ICON_RELOAD,
    UI_ICON_SUPER,
    UI_ICON_ALLY,
    UI_ICON_ENEMY,
    UI_ICON_GEM
} UiIcon;

void UiIconDraw(UiIcon icon, Vector2 center, float size, Color color);

#endif
