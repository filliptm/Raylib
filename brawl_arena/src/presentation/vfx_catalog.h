#ifndef BRAWL_VFX_CATALOG_H
#define BRAWL_VFX_CATALOG_H

#include "assets.h"

#define VFX_MAX_RECIPE_LAYERS 3

typedef enum VfxOrientation {
    VFX_ORIENT_BILLBOARD = 0,
    VFX_ORIENT_GROUND,
    VFX_ORIENT_BEAM
} VfxOrientation;

typedef enum VfxBlend {
    VFX_BLEND_ALPHA = 0,
    VFX_BLEND_ADDITIVE
} VfxBlend;

typedef enum VfxAnchor {
    VFX_ANCHOR_START = 0,
    VFX_ANCHOR_END
} VfxAnchor;

typedef struct VfxLayerDefinition {
    VfxAtlasId atlas;
    int firstFrame;
    int frameCount;
    float framesPerSecond;
    float delay;
    float duration;
    float startScale;
    float endScale;
    float startAlpha;
    float endAlpha;
    float yOffset;
    float rotationDegrees;
    float rotationSpeed;
    VfxOrientation orientation;
    VfxBlend blend;
    VfxAnchor anchor;
    bool loop;
    bool useEventColor;
    Color color;
} VfxLayerDefinition;

typedef struct VfxRecipeDefinition {
    const char *name;
    float defaultSize;
    int priority;
    int layerCount;
    VfxLayerDefinition layers[VFX_MAX_RECIPE_LAYERS];
} VfxRecipeDefinition;

const VfxRecipeDefinition *VfxCatalogGet(VfxEffectId effect);
const char *VfxAtlasPath(VfxAtlasId atlas);
void VfxAtlasGrid(VfxAtlasId atlas, int *columns, int *rows, int *frames);
bool VfxCatalogValidate(char *message, int messageCapacity);

#endif
