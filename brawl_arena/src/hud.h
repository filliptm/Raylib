#ifndef HUD_H
#define HUD_H

#include "types.h"

// Health bars floating over each brawler, drawn in screen space after the 3D pass.
void HudDrawBars(World *w);

// Bottom-left kit panel, score, respawn overlay and control hints.
void HudDrawPanel(World *w);

#endif // HUD_H
