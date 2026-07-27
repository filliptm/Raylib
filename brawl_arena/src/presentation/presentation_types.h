#ifndef BRAWL_PRESENTATION_TYPES_H
#define BRAWL_PRESENTATION_TYPES_H

#include "core_types.h"

typedef struct Particle {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
    ParticleType type;
    bool active;
} Particle;

typedef struct Shockwave {
    Vector3 position;
    float maxRadius;
    float life;
    float maxLife;
    Color color;
    bool active;
} Shockwave;

typedef struct FxLight {
    Vector3 position;
    Color color;
    float radius;
    float life;
    float maxLife;
    bool active;
} FxLight;

typedef struct FloatText {
    Vector3 world;
    char text[16];
    Color color;
    float life;
    float maxLife;
    float rise;
    float scale;
    bool active;
} FloatText;

typedef struct PresentationState {
    Camera3D camera;
    Vector3 camFocus;
    Particle particles[MAX_PARTICLES];
    FloatText texts[MAX_FLOATTEXTS];
    FxLight lights[MAX_FX_LIGHTS];
    Shockwave waves[MAX_SHOCKWAVES];
    float shake;
} PresentationState;

#endif
