#ifndef HUD_H
#define HUD_H

#include "app_types.h"
#include "player.h"

// Health bars floating over each brawler, drawn in screen space after the 3D pass.
void HudDrawBars(App *w);

// Bottom-left kit panel, score, respawn overlay and control hints.
void HudDrawPanel(App *w);
void HudObservePlayerInput(App *w, const PlayerInput *input);
bool HudConsumeContinue(void);

#endif // HUD_H
