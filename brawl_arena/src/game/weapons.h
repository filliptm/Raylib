#ifndef WEAPONS_H
#define WEAPONS_H

#include "game_types.h"

// Fires the brawler's main attack, or its super when `super` is true.
// `aimDist` only matters for arcing weapons, which land at a chosen distance.
void WeaponsFire(GameContext game, int idx, bool super, float aimDist);
bool WeaponsPlaceMine(GameContext game, int owner,
                      const AbilityDefinition *ability);
bool WeaponsMineActive(GameContext game, int owner, bool *armed);

// Advances ordinary projectiles and the fixed pool of persistent ability fields.
void ProjectilesUpdate(GameContext game, float dt);

// Where an arcing shot would land, clamped to the weapon's range.
Vector3 WeaponsArcLanding(GameContext game, const Brawler *b, float aimDist);

#endif // WEAPONS_H
