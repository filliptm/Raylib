#include "ui_theme.h"
#include <math.h>

static const UiTheme HELIOS = {
    .voidBg = { 5, 11, 20, 255 },
    .deepBg = { 7, 17, 31, 255 },
    .deck = { 14, 27, 46, 255 },
    .deckRaised = { 18, 35, 58, 255 },
    .hull = { 26, 48, 73, 255 },
    .hullBright = { 41, 67, 94, 255 },
    .line = { 59, 88, 116, 255 },
    .paper = { 243, 247, 251, 255 },
    .mist = { 168, 183, 200, 255 },
    .muted = { 130, 149, 170, 255 },
    .ion = { 100, 185, 255, 255 },
    .safety = { 255, 157, 66, 255 },
    .ready = { 246, 207, 101, 255 },
    .reactor = { 184, 140, 255, 255 },
    .ally = { 85, 213, 154, 255 },
    .enemy = { 255, 101, 119, 255 },
    .shadow = { 0, 3, 10, 190 },
    .scrim = { 2, 6, 12, 210 }
};

const UiTheme *UiThemeHelios(void)
{
    return &HELIOS;
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
