#include "player_touch.h"

#include "content_catalog.h"
#include "raymath.h"

#include <math.h>
#include <stddef.h>

#define TOUCH_NONE (-1)
#define TOUCH_DEAD_ZONE 0.14f
#define TOUCH_AIM_ACTIVATION 0.22f

static float ClampMinimum(float value, float minimum)
{
    return value < minimum ? minimum : value;
}

MobileControlLayout PlayerTouchLayout(int width, int height, AppSafeInsets insets)
{
    float left = ClampMinimum(insets.left, 0.0f) + 14.0f;
    float top = ClampMinimum(insets.top, 0.0f) + 12.0f;
    float right = width - ClampMinimum(insets.right, 0.0f) - 14.0f;
    float bottom = height - ClampMinimum(insets.bottom, 0.0f) - 12.0f;
    float shortSide = fminf((float)width, (float)height);
    float mainRadius = Clamp(shortSide*0.155f, 54.0f, 72.0f);
    float actionRadius = Clamp(shortSide*0.105f, 38.0f, 50.0f);
    float edge = mainRadius + 22.0f;

    MobileControlLayout layout = {
        .safe = { left, top, fmaxf(1.0f, right - left),
                  fmaxf(1.0f, bottom - top) },
        .moveHome = { left + edge, bottom - edge },
        .attackHome = { right - edge, bottom - edge },
        .mainRadius = mainRadius,
        .actionRadius = actionRadius
    };

    layout.superHome = (Vector2){
        layout.attackHome.x,
        fmaxf(top + actionRadius, layout.attackHome.y - mainRadius - actionRadius - 18.0f)
    };
    layout.secondaryHome = (Vector2){
        fmaxf(left + actionRadius, layout.attackHome.x - mainRadius - actionRadius - 20.0f),
        fmaxf(top + actionRadius, layout.attackHome.y - actionRadius*0.50f)
    };
    layout.pause = (Rectangle){
        right - 50.0f, top, 50.0f, 50.0f
    };
    return layout;
}

static void ResetStick(MobileStickState *stick)
{
    *stick = (MobileStickState){ .touchId = TOUCH_NONE };
}

void PlayerTouchReset(App *app)
{
    if (!app) return;
    ResetStick(&app->mobileControls.move);
    ResetStick(&app->mobileControls.attack);
    ResetStick(&app->mobileControls.superAbility);
    ResetStick(&app->mobileControls.secondary);
    app->mobileControls.pausePressed = false;
    app->mobileControls.idleAge = 0.0f;
}

static int TouchIndexForId(int touchId)
{
    if (touchId == TOUCH_NONE) return -1;
    int count = GetTouchPointCount();
    for (int i = 0; i < count; i++)
        if (GetTouchPointId(i) == touchId) return i;
    return -1;
}

static bool StickOwns(const MobileStickState *stick, int touchId)
{
    return stick->active && stick->touchId == touchId;
}

static bool TouchClaimed(const MobileControlsState *controls, int touchId)
{
    return StickOwns(&controls->move, touchId) ||
           StickOwns(&controls->attack, touchId) ||
           StickOwns(&controls->superAbility, touchId) ||
           StickOwns(&controls->secondary, touchId);
}

static void BeginStick(MobileStickState *stick, int touchId, Vector2 position,
                       Vector2 home, bool floating)
{
    stick->touchId = touchId;
    stick->active = true;
    stick->pressed = true;
    stick->released = false;
    stick->dragged = false;
    stick->dragStarted = false;
    stick->origin = floating ? position : home;
    stick->position = position;
    stick->value = (Vector2){ 0 };
}

static void UpdateStick(MobileStickState *stick, float radius)
{
    stick->pressed = false;
    stick->released = false;
    stick->dragStarted = false;
    if (!stick->active) return;

    int index = TouchIndexForId(stick->touchId);
    if (index < 0)
    {
        stick->active = false;
        stick->released = true;
        stick->touchId = TOUCH_NONE;
        return;
    }

    stick->position = GetTouchPosition(index);
    Vector2 delta = Vector2Subtract(stick->position, stick->origin);
    float length = Vector2Length(delta);
    if (length > radius)
    {
        Vector2 direction = Vector2Scale(delta, 1.0f/length);
        stick->origin = Vector2Subtract(stick->position,
                                        Vector2Scale(direction, radius));
        delta = Vector2Scale(direction, radius);
        length = radius;
    }
    stick->value = radius > 0.0f ? Vector2Scale(delta, 1.0f/radius)
                                 : (Vector2){ 0 };
    if (length < radius*TOUCH_DEAD_ZONE)
        stick->value = (Vector2){ 0 };
    if (!stick->dragged && length >= radius*TOUCH_AIM_ACTIVATION)
    {
        stick->dragged = true;
        stick->dragStarted = true;
    }
}

static bool PointInCircle(Vector2 point, Vector2 center, float radius)
{
    return Vector2Distance(point, center) <= radius;
}

Vector3 PlayerTouchCameraIntent(Camera3D camera, Vector2 stick)
{
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    forward.y = 0.0f;
    if (Vector3Length(forward) < 0.001f)
        forward = (Vector3){ 0.0f, 0.0f, 1.0f };
    forward = Vector3Normalize(forward);
    Vector3 right = Vector3Normalize(
        Vector3CrossProduct(forward, (Vector3){ 0.0f, 1.0f, 0.0f }));
    Vector3 intent = Vector3Add(Vector3Scale(right, stick.x),
                                Vector3Scale(forward, -stick.y));
    intent.y = 0.0f;
    if (Vector3Length(intent) > 1.0f) intent = Vector3Normalize(intent);
    return intent;
}

Vector3 PlayerFullSpeedMoveIntent(Vector3 intent, float deadZone)
{
    intent.y = 0.0f;
    float length = Vector3Length(intent);
    if (length <= fmaxf(0.0f, deadZone)) return (Vector3){ 0 };
    return Vector3Scale(intent, 1.0f/length);
}

void PlayerTouchApplyAttackInput(const MobileStickState *stick,
                                 PlayerInput *input)
{
    if (!stick || !input) return;
    input->attackPressed |= stick->dragStarted;
    input->attackAimed |= stick->dragged;
    if (!stick->released) return;

    if (stick->dragged) input->attackReleased = true;
    else input->autoAttackPressed = true;
}

static void ApplyAim(const App *app, Vector2 stick, float range, PlayerInput *input)
{
    if (app->session.playerIdx < 0 ||
        app->session.playerIdx >= app->session.brawlerCount ||
        Vector2Length(stick) < TOUCH_DEAD_ZONE)
        return;

    const Brawler *player = &app->session.brawlers[app->session.playerIdx];
    Vector3 direction = PlayerTouchCameraIntent(app->presentation.camera, stick);
    if (Vector3Length(direction) < 0.001f) return;
    direction = Vector3Normalize(direction);
    input->aimPoint = Vector3Add(
        player->position,
        Vector3Scale(direction, range > 0.1f ? range : 12.0f));
}

static void ClaimNewTouches(App *app, MobileControlLayout layout)
{
    MobileControlsState *controls = &app->mobileControls;
    int count = GetTouchPointCount();
    for (int i = 0; i < count; i++)
    {
        int touchId = GetTouchPointId(i);
        Vector2 point = GetTouchPosition(i);
        if (TouchClaimed(controls, touchId)) continue;

        if (CheckCollisionPointRec(point, layout.pause))
        {
            controls->pausePressed = true;
            continue;
        }
        if (!controls->superAbility.active &&
            PointInCircle(point, layout.superHome, layout.actionRadius*1.35f))
        {
            BeginStick(&controls->superAbility, touchId, point,
                       layout.superHome, false);
            continue;
        }
        if (!controls->secondary.active &&
            PointInCircle(point, layout.secondaryHome, layout.actionRadius*1.35f))
        {
            BeginStick(&controls->secondary, touchId, point,
                       layout.secondaryHome, false);
            continue;
        }
        if (point.y < layout.safe.y + layout.safe.height*0.32f) continue;

        float midpoint = layout.safe.x + layout.safe.width*0.5f;
        if (point.x < midpoint && !controls->move.active)
            BeginStick(&controls->move, touchId, point, layout.moveHome, true);
        else if (point.x >= midpoint && !controls->attack.active)
            BeginStick(&controls->attack, touchId, point, layout.attackHome, true);
    }
}

void PlayerTouchCapture(App *app, PlayerInput *input)
{
    if (!app || !input) return;
    MobileControlsState *controls = &app->mobileControls;
    MobileControlLayout layout = PlayerTouchLayout(
        GetScreenWidth(), GetScreenHeight(), AppPlatformSafeInsets());

    controls->pausePressed = false;
    UpdateStick(&controls->move, layout.mainRadius);
    UpdateStick(&controls->attack, layout.mainRadius);
    UpdateStick(&controls->superAbility, layout.actionRadius);
    UpdateStick(&controls->secondary, layout.actionRadius);
    ClaimNewTouches(app, layout);

    // Newly claimed sticks have zero values until their first drag. Attack remains
    // preview-free until it crosses the aim threshold; release before that point is
    // a direct auto-aim request.
    input->moveIntent = controls->move.active
        ? PlayerFullSpeedMoveIntent(
            PlayerTouchCameraIntent(
                app->presentation.camera, controls->move.value),
            TOUCH_DEAD_ZONE)
        : (Vector3){ 0 };
    PlayerTouchApplyAttackInput(&controls->attack, input);
    input->superHeld |= controls->superAbility.active;
    input->secondaryPressed |= controls->secondary.pressed;
    input->secondaryHeld |= controls->secondary.active;
    input->secondaryReleased |= controls->secondary.released;
    input->mobilityPressed |= input->secondaryPressed;
    input->pausePressed = controls->pausePressed;

    const Brawler *player =
        app->session.playerIdx >= 0 &&
        app->session.playerIdx < app->session.brawlerCount
        ? &app->session.brawlers[app->session.playerIdx] : NULL;
    const AbilityDefinition *mainAbility =
        player ? ContentMainAbility(&app->content, player->cls) : NULL;
    const AbilityDefinition *superAbility =
        player ? ContentSuperAbility(&app->content, player->cls) : NULL;
    const AbilityDefinition *secondaryAbility =
        player ? ContentSecondaryAbility(&app->content, player->cls) : NULL;
    if (controls->secondary.active || controls->secondary.released)
        ApplyAim(app, controls->secondary.value,
                 secondaryAbility ? secondaryAbility->range : 12.0f, input);
    else if (controls->superAbility.active || controls->superAbility.released)
        ApplyAim(app, controls->superAbility.value,
                 superAbility ? superAbility->range : 12.0f, input);
    else if (controls->attack.active || controls->attack.released)
        ApplyAim(app, controls->attack.value,
                 mainAbility ? mainAbility->range : 12.0f, input);

    bool active = controls->move.active || controls->attack.active ||
                  controls->superAbility.active || controls->secondary.active;
    controls->idleAge = active ? 0.0f : controls->idleAge + GetFrameTime();
}
