#ifndef BRAWL_UI_SKIN_H
#define BRAWL_UI_SKIN_H

#include "raylib.h"
#include <stdbool.h>

typedef struct UiSkinTexture {
    Texture2D texture;
    bool ready;
} UiSkinTexture;

typedef struct UiSkin {
    UiSkinTexture panel;
    UiSkinTexture button;
    UiSkinTexture featurePanel;
    UiSkinTexture progress;
    UiSkinTexture progressShadow;
    UiSkinTexture orbitalRing;
    UiSkinTexture radarDisc;
    bool complete;
} UiSkin;

typedef enum UiDecoration {
    UI_DECORATION_ORBITAL_RING = 0,
    UI_DECORATION_RADAR_DISC
} UiDecoration;

bool UiSkinLoad(UiSkin *skin);
void UiSkinUnload(UiSkin *skin);

NPatchInfo UiSkinNinePatchInfo(int width, int height,
                               int left, int top, int right, int bottom);
bool UiSkinDrawPanel(const UiSkin *skin, Rectangle bounds, Color fill,
                     Color edge, bool raised, bool feature);
bool UiSkinDrawButton(const UiSkin *skin, Rectangle bounds, Color fill,
                      Color edge, bool raised);
bool UiSkinDrawProgress(const UiSkin *skin, Rectangle bounds, float value,
                        Color track, Color fill, bool segmented, int segments,
                        float gap);
bool UiSkinDrawDecoration(const UiSkin *skin, UiDecoration decoration,
                          Rectangle bounds, Color tint);

#endif
