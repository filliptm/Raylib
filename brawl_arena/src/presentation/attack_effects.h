#ifndef BRAWL_ATTACK_EFFECTS_H
#define BRAWL_ATTACK_EFFECTS_H

#include "app_types.h"
#include "assets.h"

// Runtime for authored attack documents: spawns pooled flipbook particles from a
// document's layers at an anchor event, integrates them, and draws them in the
// world effects passes.

// Spawns every layer bound to `anchor`. `followBrawler` is the caster index for
// self-anchored layers and the target index for mark anchors (-1 otherwise).
// `fieldRadius` scales fitField layers; `lifeOverride` (>0) replaces the duration
// of looping layers so they live exactly as long as their field or mark.
void AttackFxSpawn(App *w, const AttackPresentation *doc, int anchor,
                   Vector3 origin, float yaw, int followBrawler,
                   float fieldRadius, float lifeOverride, Color eventColor);

void AttackFxUpdate(App *w, float dt);
void AttackFxDraw(App *w, Assets *a);

// Lit, depth-writing pass for mesh-shaped layers (shield walls, orbs, discs).
// Call inside the solid section of the world pass, before transparents.
void AttackFxDrawSolid(App *w, Assets *a);

// Authored projectile visual block for a live projectile, or NULL when the
// ability is unauthored. Used by the projectile renderer for tint/scale/glow
// overrides and by the light submitter.
const AttackProjectileVisual *AttackProjectileVisualFor(const App *w,
                                                        const Projectile *p);

#endif
