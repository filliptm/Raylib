#ifndef GEMS_H
#define GEMS_H

#include "game_types.h"

// Resets the vent, clears the floor and puts the match back to its opening state.
void MatchReset(GameContext game);

// Gem spawning, pickup, the team tallies and the countdown to a win.
void MatchUpdate(GameContext game, float dt);

// Scatters everything a brawler was carrying. Called when one goes down.
void GemsDropFrom(GameContext game, int idx);

// Drops a single gem at a world position, popped outward on `impulse`.
void GemSpawnAt(GameContext game, Vector3 position, Vector3 impulse);

// True once a real match has been decided. Gameplay freezes on this: a finished match
// that carries on playing itself reads as a bug, not a result.
bool MatchIsOver(GameContext game);

// Nearest loose gem a brawler could go and collect, or -1.
int GemsNearestLoose(GameContext game, Vector3 from, float maxDistance);

#endif // GEMS_H
