#ifndef WEAPONS_H
#define WEAPONS_H

#include "types.h"

// Fires the brawler's main attack, or its super when `super` is true.
// `aimDist` only matters for arcing weapons, which land at a chosen distance.
void WeaponsFire(World *w, int idx, bool super, float aimDist);

// Advances ordinary projectiles and the fixed pool of persistent ability fields.
void ProjectilesUpdate(World *w, float dt);
void ProjectilesDraw(World *w);

// Where an arcing shot would land, clamped to the weapon's range.
Vector3 WeaponsArcLanding(const Brawler *b, float aimDist);

// Restore the live weapon table from the pristine baseline.
void WeaponsResetAll(void);
void WeaponsResetKit(BrawlerClass cls);

#endif // WEAPONS_H
