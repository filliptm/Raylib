#include "player.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "effects.h"
#include "command_center.h"
#include "raymath.h"
#include <math.h>

#define TAP_THRESHOLD 0.14f     // shorter than this counts as a tap-to-autoaim shot

Vector3 PlayerMouseGroundPoint(World *w)
{
    Ray ray = GetScreenToWorldRay(GetMousePosition(), w->camera);

    // Intersect the ray with the y = 0 plane.
    if (fabsf(ray.direction.y) < 0.0001f)
        return (Vector3){ ray.position.x, 0.0f, ray.position.z };

    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) t = 0.0f;

    return (Vector3){
        ray.position.x + ray.direction.x * t,
        0.0f,
        ray.position.z + ray.direction.z * t
    };
}

void PlayerUpdate(World *w, float dt)
{
    int idx = w->playerIdx;
    Brawler *b = &w->brawlers[idx];

    // Class switching is instant, so the feel of each kit is easy to compare.
    for (int c = 0; c < CLASS_COUNT; c++)
    {
        if (IsKeyPressed(KEY_ONE + c))
        {
            Vector3 pos = b->alive ? b->position : ArenaSpawnFor(&w->arena, TEAM_PLAYER, b->spawnSlot);
            float keepSuper = b->superCharge;
            int keepGems = b->gems, keepSlot = b->spawnSlot;
            BrawlerSpawn(w, idx, TEAM_PLAYER, (BrawlerClass)c, pos, true);
            w->brawlers[idx].superCharge = keepSuper;
            w->brawlers[idx].gems = keepGems;      // swapping kit is not a death
            w->brawlers[idx].spawnSlot = keepSlot;
            FxFloatText(w, pos, WEAPONS[c].name, (Color){ 255, 255, 255, 255 });
        }
    }

    w->aimPoint = PlayerMouseGroundPoint(w);

    if (!b->alive)
    {
        w->charging = false;
        w->aimingSuper = false;
        b->moveIntent = (Vector3){ 0 };
        return;
    }

    //--- Movement --------------------------------------------------------------
    // Build the movement basis from the camera rather than from world axes. The camera
    // looks down +Z, so screen-right is -X; hardcoding +X for D inverted A and D.
    Vector3 camForward = Vector3Subtract(w->camera.target, w->camera.position);
    camForward.y = 0.0f;
    if (Vector3Length(camForward) < 0.001f) camForward = (Vector3){ 0.0f, 0.0f, 1.0f };
    camForward = Vector3Normalize(camForward);

    Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, (Vector3){ 0.0f, 1.0f, 0.0f }));

    float fwdInput = 0.0f, rightInput = 0.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    fwdInput   += 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  fwdInput   -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) rightInput += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  rightInput -= 1.0f;

    Vector3 move = Vector3Add(Vector3Scale(camForward, fwdInput), Vector3Scale(camRight, rightInput));
    move.y = 0.0f;
    if (Vector3Length(move) > 0.001f) move = Vector3Normalize(move);
    b->moveIntent = move;

    //--- Aiming ----------------------------------------------------------------
    Vector3 toAim = Vector3Subtract(w->aimPoint, b->position);
    toAim.y = 0.0f;
    float aimLen = Vector3Length(toAim);

    if (aimLen > 0.2f)
        b->aimAngle = atan2f(toAim.x, toAim.z);

    w->aimDist = aimLen;

    if (b->dashTimer > 0.0f)
    {
        w->charging = false;
        w->aimingSuper = false;
        return;
    }

    // Clicks meant for the command center must not also fire the weapon.
    if (CommandCenterCapturesMouse())
    {
        w->charging = false;
        w->aimingSuper = false;
        return;
    }

    //--- Super: hold right mouse to aim, release to fire ------------------------
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && b->superCharge >= 1.0f)
    {
        w->aimingSuper = true;
    }
    else if (w->aimingSuper)
    {
        w->aimingSuper = false;
        if (BrawlerTrySuper(w, idx, aimLen)) BrawlerFaceShot(w, idx, b->aimAngle, 0.4f);
    }

    //--- Main attack: hold to preview, release to fire; a quick tap auto-aims ----
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        w->charging = true;
        w->chargeTime = 0.0f;
    }

    if (w->charging) w->chargeTime += dt;

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && w->charging)
    {
        w->charging = false;

        if (w->chargeTime < TAP_THRESHOLD)
        {
            // Tap fires at the nearest visible enemy, mirroring mobile's auto-aim.
            int target = BrawlerNearestVisibleEnemy(w, idx);
            if (target >= 0)
            {
                Vector3 t = w->brawlers[target].position;
                Vector3 d = Vector3Subtract(t, b->position);
                d.y = 0.0f;

                float travel = Vector3Length(d) / WEAPONS[b->cls].speed;
                Vector3 lead = Vector3Add(t, Vector3Scale(w->brawlers[target].velocity, travel * 0.7f));
                Vector3 ld = Vector3Subtract(lead, b->position);

                b->aimAngle = atan2f(ld.x, ld.z);
                if (BrawlerTryAttack(w, idx, Vector3Length((Vector3){ ld.x, 0.0f, ld.z })))
                    BrawlerFaceShot(w, idx, b->aimAngle, 0.45f);
            }
            else
            {
                if (BrawlerTryAttack(w, idx, aimLen)) BrawlerFaceShot(w, idx, b->aimAngle, 0.25f);
            }
        }
        else
        {
            if (BrawlerTryAttack(w, idx, aimLen)) BrawlerFaceShot(w, idx, b->aimAngle, 0.25f);
        }
    }

    // Space is a straight auto-aim shot, handy while repositioning.
    if (IsKeyPressed(KEY_SPACE))
    {
        int target = BrawlerNearestVisibleEnemy(w, idx);
        if (target >= 0)
        {
            Vector3 d = Vector3Subtract(w->brawlers[target].position, b->position);
            d.y = 0.0f;
            b->aimAngle = atan2f(d.x, d.z);
            if (BrawlerTryAttack(w, idx, Vector3Length(d)))
                BrawlerFaceShot(w, idx, b->aimAngle, 0.45f);
        }
        else if (BrawlerTryAttack(w, idx, aimLen)) BrawlerFaceShot(w, idx, b->aimAngle, 0.25f);
    }
}
