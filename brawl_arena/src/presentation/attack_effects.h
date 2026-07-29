#ifndef BRAWL_ATTACK_EFFECTS_H
#define BRAWL_ATTACK_EFFECTS_H

#include "app_types.h"
#include "assets.h"

// Runtime for authored attack documents: spawns pooled flipbook particles from a
// document's layers at an anchor event, integrates them, and draws them in the
// world effects passes.

// Spawns every layer bound to `anchor`. `followBrawler` is the caster index for
// self-anchored layers (-1 otherwise). Yaw orients patterns and offsets.
void AttackFxSpawn(App *w, const AttackPresentation *doc, int anchor,
                   Vector3 origin, float yaw, int followBrawler);

void AttackFxUpdate(App *w, float dt);
void AttackFxDraw(App *w, Assets *a);

#endif
