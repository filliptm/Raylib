#ifndef BRAWL_STUDIO_H
#define BRAWL_STUDIO_H

#include "app_types.h"
#include "assets.h"

// The VFX Studio screen: an isolated stage where one character loops an ability
// cast under slow motion, with an orbit camera and live authoring panels.
// StudioFrame owns input, the mini-sim tick, and the camera; it returns the
// simulated dt the caller should feed presentation consumers.
float StudioFrame(App *w, float realDt);

// 2D overlay: stage controls, the attack editor, and the timeline. Assets are
// needed for the atlas browser and the rebuild-and-reload flow.
void StudioDraw(App *w, Assets *assets);

#endif
