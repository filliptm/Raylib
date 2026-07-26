#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"

void PlayerUpdate(World *w, float dt);

// Ground point under the mouse cursor, on the y = 0 plane.
Vector3 PlayerMouseGroundPoint(World *w);

#endif // PLAYER_H
