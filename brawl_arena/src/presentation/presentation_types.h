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

typedef struct VfxInstance {
    Vector3 position;
    Vector3 endPosition;
    Color eventColor;
    VfxEffectId effect;
    float angle;
    float size;
    float age;
    float depthBias;
    int layerIndex;
    int priority;
    int sourceBrawler;
    int targetBrawler;
    VfxSocket startSocket;
    VfxSocket endSocket;
    bool active;
} VfxInstance;

typedef struct CharacterSocketPose {
    Vector3 positions[VFX_SOCKET_COUNT];
    bool valid[VFX_SOCKET_COUNT];
    bool rigged;
} CharacterSocketPose;

typedef struct CharacterActionState {
    CharacterActionId action;
    float age;
    float duration;
    bool active;
} CharacterActionState;

// Per-brawler locomotion clip state. Lives here rather than in render statics so a
// match reset clears it and concealed brawlers keep animating while undrawn.
typedef struct CharacterAnimState {
    float time;         // seconds into the current clip
    float clipAge;      // seconds since the clip last changed
    int clip;
    int cls;            // BrawlerClass owning `clip`; a kit swap forces a restart
    bool loop;
    bool valid;         // false until the first update after a reset
} CharacterAnimState;

typedef struct PresentationState {
    Camera3D camera;
    Vector3 camFocus;
    Particle particles[MAX_PARTICLES];
    FloatText texts[MAX_FLOATTEXTS];
    FxLight lights[MAX_FX_LIGHTS];
    Shockwave waves[MAX_SHOCKWAVES];
    VfxInstance vfx[MAX_VFX_INSTANCES];
    CharacterSocketPose sockets[MAX_BRAWLERS];
    CharacterActionState actions[MAX_BRAWLERS];
    CharacterAnimState anim[MAX_BRAWLERS];
    int droppedVfx;
    int vfxEventsConsumed;
    int vfxLayersSpawned;
    VfxEffectId lastVfxEffect;
    float shake;
} PresentationState;

#endif
