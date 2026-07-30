#ifndef BRAWL_VFX_H
#define BRAWL_VFX_H

#include "game_types.h"
#include "presentation_types.h"
#include "assets.h"
#include "vfx_catalog.h"

// Imported ability art is authored conservatively inside its atlas cells. Present every
// recipe at four times its catalog scale for match-camera readability; gameplay
// dimensions and authoritative procedural telegraphs are intentionally unaffected.
#define VFX_RENDER_SCALE 4.0f

// Expands one typed simulation event into the recipe's presentation-owned layers.
// Returns the number of layers admitted to the fixed pool.
int VfxSpawnEvent(PresentationState *presentation, const GameEvent *event);
int VfxSpawnPreview(PresentationState *presentation, VfxEffectId effect,
                    Vector3 position, Vector3 endPosition, float angle,
                    float size, Color color);
void VfxUpdate(PresentationState *presentation, float dt);
void VfxDraw(const PresentationState *presentation, const Assets *assets,
             Camera3D camera, bool reducedMotion);

int VfxFrameForLayer(const VfxLayerDefinition *layer, float age);
int VfxActiveCount(const PresentationState *presentation);
int VfxLoadedAtlasCount(const Assets *assets);

#endif
