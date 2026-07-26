#ifndef RENDER_H
#define RENDER_H

#include "types.h"
#include "assets.h"

void RenderSetAssets(Assets *a);

// Bakes grass instance transforms for the current arena. Call after ArenaLoad().
void RenderBuildGrass(World *w);

void CameraInit(World *w);
void CameraUpdate(World *w, float dt);
void RenderWorld(World *w);

#endif // RENDER_H
