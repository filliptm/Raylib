#include "ui_theme.h"
#include <math.h>

static const UiTheme ARENA_INK = {
    .ink = { 7, 16, 25, 255 },
    .inkSoft = { 10, 31, 54, 255 },
    .surface = { 14, 48, 88, 255 },
    .surfaceRaised = { 18, 76, 141, 255 },
    .surfaceMuted = { 38, 82, 124, 255 },
    .surfaceStrong = { 80, 139, 190, 255 },
    .border = { 151, 207, 239, 255 },
    .paper = { 255, 247, 219, 255 },
    .textSecondary = { 200, 221, 234, 255 },
    .textMuted = { 142, 173, 194, 255 },
    .blue = { 7, 108, 213, 255 },
    .yellow = { 255, 210, 30, 255 },
    .gold = { 255, 229, 91, 255 },
    .purple = { 129, 64, 240, 255 },
    .ally = { 32, 198, 122, 255 },
    .enemy = { 217, 43, 43, 255 },
    .shadow = { 0, 0, 0, 220 },
    .scrim = { 3, 10, 18, 218 }
};

const UiTheme *UiThemeArenaInk(void)
{
    return &ARENA_INK;
}

Color UiThemeHighContrast(Color color, bool enabled)
{
    if (!enabled) return color;
    int brightest = color.r;
    if (color.g > brightest) brightest = color.g;
    if (color.b > brightest) brightest = color.b;
    if (brightest < 150)
    {
        color.r = (unsigned char)((color.r + 255)/2);
        color.g = (unsigned char)((color.g + 255)/2);
        color.b = (unsigned char)((color.b + 255)/2);
    }
    return color;
}

static float LinearChannel(unsigned char channel)
{
    float value = channel/255.0f;
    return value <= 0.04045f ? value/12.92f :
        powf((value + 0.055f)/1.055f, 2.4f);
}

float UiThemeContrastRatio(Color foreground, Color background)
{
    float first = 0.2126f*LinearChannel(foreground.r) +
                  0.7152f*LinearChannel(foreground.g) +
                  0.0722f*LinearChannel(foreground.b);
    float second = 0.2126f*LinearChannel(background.r) +
                   0.7152f*LinearChannel(background.g) +
                   0.0722f*LinearChannel(background.b);
    float lighter = fmaxf(first, second);
    float darker = fminf(first, second);
    return (lighter + 0.05f)/(darker + 0.05f);
}
