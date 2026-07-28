#include "camera.h"

#include "arena.h"
#include "raymath.h"
#include <math.h>

static const Vector3 CAMERA_OFFSET = { 0.0f, 31.0f, -22.0f };

void CameraInit(App *world)
{
    PresentationState *view = &world->presentation;
    view->camFocus = ArenaSpawnFor(&world->session.arena, TEAM_PLAYER, 0);
    view->camera.position = Vector3Add(view->camFocus, CAMERA_OFFSET);
    view->camera.target = view->camFocus;
    view->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    view->camera.fovy = 46.0f;
    view->camera.projection = CAMERA_PERSPECTIVE;
}

void CameraUpdate(App *world, float deltaTime)
{
    PresentationState *view = &world->presentation;
    Brawler *player = &world->session.brawlers[world->session.playerIdx];

    Vector3 desired = player->alive ? player->position : view->camFocus;
    if (player->alive)
    {
        Vector3 lead = Vector3Subtract(world->controller.aimPoint, player->position);
        lead.y = 0.0f;
        float length = Vector3Length(lead);
        if (length > 0.001f)
        {
            float clamped = fminf(length, 8.0f)*0.28f;
            lead = Vector3Scale(Vector3Normalize(lead), clamped);
            desired = Vector3Add(desired, lead);
        }
    }

    view->camFocus = Vector3Lerp(view->camFocus, desired, SmoothFactor(6.0f, deltaTime));

    // Shake is layered fixed-frequency noise rather than fresh randomness each
    // frame, so its character does not change with the display's refresh rate.
    Vector3 shake = { 0 };
    if (view->shake > 0.0f)
    {
        view->shakePhase += deltaTime;
        float strength = view->shake*0.22f*0.625f;
        float t = view->shakePhase;
        shake.x = (sinf(t*143.0f) + 0.6f*sinf(t*97.0f))*strength;
        shake.y = (sinf(t*127.0f + 1.7f) + 0.6f*sinf(t*89.0f))*strength;
        shake.z = (sinf(t*151.0f + 3.1f) + 0.6f*sinf(t*103.0f))*strength;
    }

    view->camera.position =
        Vector3Add(Vector3Add(view->camFocus, CAMERA_OFFSET), shake);
    view->camera.target = Vector3Add(view->camFocus, shake);
}
