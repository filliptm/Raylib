#include "camera.h"
#include <math.h>
#include <stdio.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static float Separation(Vector3 a, Vector3 b)
{
    float x = a.x - b.x;
    float y = a.y - b.y;
    float z = a.z - b.z;
    return sqrtf(x*x + y*y + z*z);
}

static bool Near(float a, float b)
{
    return fabsf(a - b) < 0.0001f;
}

int main(void)
{
    CHECK(Near(CameraEffectiveDistance(32.0f, false), 32.0f),
          "desktop camera distance no longer uses the authored value");
    CHECK(Near(CameraEffectiveDistance(32.0f, true), 25.6f),
          "mobile camera distance no longer provides the intended close framing");
    CHECK(Near(CameraEffectiveDistance(20.0f, true), 20.0f),
          "mobile camera distance escaped its readability floor");

    App world = { 0 };
    world.tune.matchCameraDistance = DEFAULT_MATCH_CAMERA_DISTANCE;

    CameraInit(&world);
    CHECK(Near(Separation(world.presentation.camera.position,
                          world.presentation.camera.target),
               DEFAULT_MATCH_CAMERA_DISTANCE),
          "default camera framing no longer matches the established offset");
    CHECK(Near(world.presentation.camera.position.y, 31.0f) &&
          Near(world.presentation.camera.position.z, -22.0f),
          "default camera pitch or placement changed");

    world.tune.matchCameraDistance = 20.0f;
    CameraUpdate(&world, 1.0f/60.0f);
    CHECK(Near(Separation(world.presentation.camera.position,
                          world.presentation.camera.target),
               20.0f),
          "live camera distance did not update");

    float scale = 20.0f/DEFAULT_MATCH_CAMERA_DISTANCE;
    CHECK(Near(world.presentation.camera.position.y, 31.0f*scale) &&
          Near(world.presentation.camera.position.z, -22.0f*scale),
          "camera distance adjustment changed the established pitch");

    world.session.brawlers[0].alive = true;
    world.session.brawlers[0].position = (Vector3){ 4.0f, 0.0f, -3.0f };
    world.controller.aimPoint = (Vector3){ 12.0f, 0.0f, -3.0f };
    CameraUpdate(&world, 1.0f/30.0f);
    CHECK(Near(Separation(world.presentation.camera.position,
                          world.presentation.camera.target),
               20.0f),
          "aim lead changed the authored camera distance");

    printf("Camera test passed: live distance preserves pitch, follow, and aim lead\n");
    return 0;
}
