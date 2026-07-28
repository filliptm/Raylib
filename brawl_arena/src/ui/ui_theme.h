#ifndef BRAWL_UI_THEME_H
#define BRAWL_UI_THEME_H

#include "raylib.h"

typedef struct UiTheme {
    Color voidBg;
    Color deepBg;
    Color deck;
    Color deckRaised;
    Color hull;
    Color hullBright;
    Color line;
    Color paper;
    Color mist;
    Color muted;
    Color ion;
    Color safety;
    Color ready;
    Color reactor;
    Color ally;
    Color enemy;
    Color shadow;
    Color scrim;
} UiTheme;

const UiTheme *UiThemeHelios(void);
Color UiThemeHighContrast(Color color, bool enabled);
float UiThemeContrastRatio(Color foreground, Color background);

#endif
