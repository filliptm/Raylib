#ifndef BRAWL_STUDIO_H
#define BRAWL_STUDIO_H

#include "app_types.h"

// The VFX Studio screen: an isolated stage where one character loops an ability
// cast under slow motion, with an orbit camera and live authoring panels.
// StudioFrame owns input, the mini-sim tick, and the camera; it returns the
// simulated dt the caller should feed presentation consumers.
float StudioFrame(App *w, float realDt);

// 2D overlay: control panel plus (in later phases) the effect editor.
void StudioDraw(App *w);

#endif
