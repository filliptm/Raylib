#ifndef RAYLIB_COMPAT_H
#define RAYLIB_COMPAT_H

#include "raylib.h"

// Compatibility layer for missing raylib functions in tests
#ifndef RAYLIB_H

typedef struct RayCollision {
    bool hit;
    float distance;
    Vector3 point;
    Vector3 normal;
} RayCollision;

// Stub implementations for testing
static inline RayCollision GetRayCollisionBox(Ray ray, BoundingBox box) {
    RayCollision collision = {0};
    // Simple stub - always miss for tests
    collision.hit = false;
    collision.distance = 0.0f;
    collision.point = (Vector3){0, 0, 0};
    collision.normal = (Vector3){0, 0, 0};
    return collision;
}

#endif

#endif // RAYLIB_COMPAT_H