#ifndef GEMS_H
#define GEMS_H

#include "types.h"

// Resets the vent, clears the floor and puts the match back to its opening state.
void MatchReset(World *w);

// Gem spawning, pickup, the team tallies and the countdown to a win.
void MatchUpdate(World *w, float dt);

// Scatters everything a brawler was carrying. Called when one goes down.
void GemsDropFrom(World *w, int idx);

// Drops a single gem at a world position, popped outward on `impulse`.
void GemSpawnAt(World *w, Vector3 position, Vector3 impulse);

// Nearest loose gem a brawler could go and collect, or -1.
int GemsNearestLoose(World *w, Vector3 from, float maxDistance);

#endif // GEMS_H
