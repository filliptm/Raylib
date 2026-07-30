#ifndef BRAWL_UI_THEME_H
#define BRAWL_UI_THEME_H

#include "raylib.h"

typedef struct UiTheme {
    Color ink;
    Color inkSoft;
    Color surface;
    Color surfaceRaised;
    Color surfaceMuted;
    Color surfaceStrong;
    Color border;
    Color paper;
    Color textSecondary;
    Color textMuted;
    Color blue;
    Color yellow;
    Color gold;
    Color purple;
    Color ally;
    Color enemy;
    Color shadow;
    Color scrim;
} UiTheme;

const UiTheme *UiThemeArenaInk(void);
Color UiThemeHighContrast(Color color, bool enabled);
float UiThemeContrastRatio(Color foreground, Color background);

#endif
