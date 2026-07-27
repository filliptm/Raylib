#ifndef PLAYER_H
#define PLAYER_H

#include "app_types.h"

// Platform state captured once per rendered frame. Replays/tests can supply this same
// value without linking gameplay behavior to raylib keyboard or mouse calls.
typedef struct PlayerInput {
    Vector3 moveIntent;
    Vector3 aimPoint;
    int selectedClass;          // -1 when no class hotkey was pressed
    bool attackPressed;
    bool attackReleased;
    bool superHeld;
    bool autoAttackPressed;
    bool actionsBlocked;        // UI owns the pointer this frame
} PlayerInput;

PlayerInput PlayerCaptureInput(const App *w);
void PlayerUpdate(App *w, const PlayerInput *input, float dt);

// Ground point under the mouse cursor, on the y = 0 plane.
Vector3 PlayerMouseGroundPoint(const App *w);

#endif // PLAYER_H
