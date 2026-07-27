#ifndef RENDER_H
#define RENDER_H

#include "app_types.h"
#include "assets.h"

void RenderSetAssets(Assets *a);

// Draws one brawler from its own fields, inside an active BeginMode3D. Used by the
// match and by the menu podium alike.
// Draws one brawler from its own fields, inside an active BeginMode3D.
// bodyTint overrides the team colour; pass NULL in a match, where team colour is what
// actually needs reading.
void RenderBrawlerModel(Assets *a, Brawler *b, float time, float dither, const Color *bodyTint);

// Bakes grass instance transforms for the current arena. Call after ArenaLoad().
void RenderBuildGrass(App *w);

void RenderWorld(App *w);

#endif // RENDER_H
