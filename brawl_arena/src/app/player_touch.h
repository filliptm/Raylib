#ifndef BRAWL_PLAYER_TOUCH_H
#define BRAWL_PLAYER_TOUCH_H

#include "player.h"
#include "platform.h"

typedef struct MobileControlLayout {
    Rectangle safe;
    Vector2 moveHome;
    Vector2 attackHome;
    Vector2 superHome;
    Vector2 secondaryHome;
    Rectangle pause;
    float mainRadius;
    float actionRadius;
} MobileControlLayout;

MobileControlLayout PlayerTouchLayout(int width, int height, AppSafeInsets insets);
Vector3 PlayerTouchCameraIntent(Camera3D camera, Vector2 stick);
void PlayerTouchReset(App *app);
void PlayerTouchCapture(App *app, PlayerInput *input);

#endif
