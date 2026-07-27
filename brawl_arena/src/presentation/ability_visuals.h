#ifndef ABILITY_VISUALS_H
#define ABILITY_VISUALS_H

#include "app_types.h"
#include "assets.h"

// Draws active area abilities and the player's current aim preview.
// Call inside BeginMode3D after arena geometry and brawlers.
void AbilityVisualsDraw(App *world, Assets *assets);

#endif // ABILITY_VISUALS_H
