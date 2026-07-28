#ifndef AI_H
#define AI_H

#include "game_types.h"

// Returns a normalized, body-clear movement direction toward a world-space goal.
// The route is rebuilt from live wall/crate state, so destroyed cover opens immediately.
Vector3 AINavigationDirection(GameContext game, Vector3 from, Vector3 goal, float side);

void AIUpdate(GameContext game, float dt);

#endif // AI_H
