#ifndef BRAWL_UI_SKIN_H
#define BRAWL_UI_SKIN_H

#include "raylib.h"
#include <stdbool.h>

typedef struct UiSkin {
    bool ready;
} UiSkin;

typedef enum UiDecoration {
    UI_DECORATION_BURST = 0,
    UI_DECORATION_HALFTONE,
    UI_DECORATION_SPEED_LINES
} UiDecoration;

bool UiSkinLoad(UiSkin *skin);
void UiSkinUnload(UiSkin *skin);

bool UiSkinDrawPanel(const UiSkin *skin, Rectangle bounds, Color fill,
                     Color edge, bool raised, bool feature);
bool UiSkinDrawButton(const UiSkin *skin, Rectangle bounds, Color fill,
                      Color edge, bool raised);
bool UiSkinDrawProgress(const UiSkin *skin, Rectangle bounds, float value,
                        Color track, Color fill, bool segmented, int segments,
                        float gap);
bool UiSkinDrawDecoration(const UiSkin *skin, UiDecoration decoration,
                          Rectangle bounds, Color tint);
bool UiSkinDrawBackdrop(const UiSkin *skin, Rectangle viewport, Rectangle canvas,
                        Color blue, Color ink, Color yellow, Color red);

#endif
