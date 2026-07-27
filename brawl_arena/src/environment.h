#ifndef BRAWL_ENVIRONMENT_H
#define BRAWL_ENVIRONMENT_H

#include "assets.h"
#include "types.h"

// Keep parallel floor surfaces on explicit, shared height layers. Imported inlays are
// fitted just below gameplay ground, passive glows/shadows sit above them, and active
// targeting/effect geometry gets a separate higher layer so the depth buffer never has
// to choose between nearly coplanar surfaces.
#define ARENA_DECK_Y       (-0.055f)
#define ARENA_INLAY_TOP_Y  (-0.020f)
#define ARENA_DECAL_Y        0.065f
#define ARENA_PREVIEW_Y      0.120f

// Draws the Helios-9 station deck, collision-aligned cover, hydroponic beds, and
// non-gameplay set dressing. The Arena tile grid remains the gameplay authority.
void EnvironmentDraw(const World *w, Assets *a);

#endif // BRAWL_ENVIRONMENT_H
