#ifndef AI_H
#define AI_H

#include "game_types.h"

// Returns a normalized, body-clear movement direction toward a world-space goal.
// The route is rebuilt from live wall/crate state, so destroyed cover opens immediately.
// Steers `b` toward `goal`, using a direct body-clear sweep when possible and a
// cached BFS route over the arena's navigable bitmap otherwise.
Vector3 AINavigationDirection(GameContext game, Brawler *b, Vector3 goal);

void AIUpdate(GameContext game, float dt);

#endif // AI_H
