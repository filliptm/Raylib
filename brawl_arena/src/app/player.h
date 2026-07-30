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
    // Touch drag explicitly selected a direction, even if press-to-release time
    // was shorter than the desktop quick-tap threshold.
    bool attackAimed;
    bool superHeld;
    bool autoAttackPressed;
    bool secondaryPressed;
    bool secondaryHeld;
    bool secondaryReleased;
    // Legacy replay field: treated as a secondary press.
    bool mobilityPressed;
    bool actionsBlocked;        // UI owns the pointer this frame
    bool pausePressed;          // mobile pause/back affordance
} PlayerInput;

PlayerInput PlayerCaptureInput(App *w);
void PlayerUpdate(App *w, const PlayerInput *input, float dt);

// Ground point under the mouse cursor, on the y = 0 plane.
Vector3 PlayerMouseGroundPoint(const App *w);

#endif // PLAYER_H
