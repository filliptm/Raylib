#include "player.h"

#include "command_center.h"
#include "content_catalog.h"
#include "raymath.h"
#include <math.h>

static bool g_gamepadAttackHeld = false;

static int ActiveGamepad(void)
{
    for (int gamepad = 0; gamepad < 4; gamepad++)
        if (IsGamepadAvailable(gamepad)) return gamepad;
    return -1;
}

Vector3 PlayerMouseGroundPoint(const App *w)
{
    Ray ray = GetScreenToWorldRay(GetMousePosition(), w->presentation.camera);
    if (fabsf(ray.direction.y) < 0.0001f)
        return (Vector3){ ray.position.x, 0.0f, ray.position.z };

    float distance = -ray.position.y/ray.direction.y;
    if (distance < 0.0f) distance = 0.0f;
    return (Vector3){
        ray.position.x + ray.direction.x*distance,
        0.0f,
        ray.position.z + ray.direction.z*distance
    };
}

PlayerInput PlayerCaptureInput(const App *w)
{
    PlayerInput input = { 0 };
    input.selectedClass = -1;
    input.aimPoint = PlayerMouseGroundPoint(w);

    Vector3 cameraForward = Vector3Subtract(w->presentation.camera.target,
                                            w->presentation.camera.position);
    cameraForward.y = 0.0f;
    if (Vector3Length(cameraForward) < 0.001f)
        cameraForward = (Vector3){ 0.0f, 0.0f, 1.0f };
    cameraForward = Vector3Normalize(cameraForward);
    Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(cameraForward, (Vector3){ 0.0f, 1.0f, 0.0f }));

    float forward = 0.0f, right = 0.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) forward += 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) forward -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) right += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) right -= 1.0f;
    input.moveIntent = Vector3Add(Vector3Scale(cameraForward, forward),
                                  Vector3Scale(cameraRight, right));
    input.moveIntent.y = 0.0f;
    if (Vector3Length(input.moveIntent) > 0.001f)
        input.moveIntent = Vector3Normalize(input.moveIntent);

    int gamepad = ActiveGamepad();
    if (gamepad >= 0)
    {
        float stickX = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
        float stickY = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(stickX) > 0.18f || fabsf(stickY) > 0.18f)
        {
            input.moveIntent = Vector3Add(Vector3Scale(cameraRight, stickX),
                                          Vector3Scale(cameraForward, -stickY));
            input.moveIntent.y = 0.0f;
            if (Vector3Length(input.moveIntent) > 1.0f)
                input.moveIntent = Vector3Normalize(input.moveIntent);
        }

        float aimX = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X);
        float aimY = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y);
        if ((fabsf(aimX) > 0.20f || fabsf(aimY) > 0.20f) &&
            w->session.brawlerCount > w->session.playerIdx)
        {
            const Brawler *player = &w->session.brawlers[w->session.playerIdx];
            const AbilityDefinition *ability =
                ContentMainAbility(&w->content, player->cls);
            Vector3 aim = Vector3Add(Vector3Scale(cameraRight, aimX),
                                     Vector3Scale(cameraForward, -aimY));
            if (Vector3Length(aim) > 0.001f) aim = Vector3Normalize(aim);
            input.aimPoint = Vector3Add(player->position,
                                        Vector3Scale(aim, ability ? ability->range : 12.0f));
        }

        bool attackHeld =
            GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.15f;
        input.attackPressed |= attackHeld && !g_gamepadAttackHeld;
        input.attackReleased |= !attackHeld && g_gamepadAttackHeld;
        g_gamepadAttackHeld = attackHeld;
        input.superHeld |= IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        input.autoAttackPressed |=
            IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        input.mobilityPressed |=
            IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    }

    for (int classId = 0; classId < CLASS_COUNT; classId++)
        if (IsKeyPressed(KEY_ONE + classId)) input.selectedClass = classId;

    input.attackPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    input.attackReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    input.superHeld = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
    input.autoAttackPressed = IsKeyPressed(KEY_SPACE);
    input.mobilityPressed = IsKeyPressed(KEY_LEFT_SHIFT) ||
                            IsKeyPressed(KEY_RIGHT_SHIFT);
    input.actionsBlocked = CommandCenterCapturesMouse();
    return input;
}
