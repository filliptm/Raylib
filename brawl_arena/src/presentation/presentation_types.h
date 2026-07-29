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
    // Crossfade out of the previous clip: its pose freezes at the switch moment
    // and blends away over CHARACTER_CROSSFADE_DURATION. -1 when no fade runs.
    int fadeClip;
    float fadeTime;     // seconds into fadeClip at the moment of the switch
    float fadeAge;      // seconds since the switch
} CharacterAnimState;

// Live particle spawned by an authored attack document layer.
#define MAX_ATTACK_PARTICLES 512
typedef struct AttackParticle {
    bool active;
    int atlas;
    int frame;
    int frameCount;
    float fps;
    Vector3 position;
    Vector3 velocity;
    float gravity;
    float drag;
    float delay;            // seconds before the particle appears
    float age;
    float duration;
    float scaleStart, scaleEnd;
    Color colorStart, colorEnd;
    int blend;              // AttackBlendMode
    float rotation;         // degrees
    float rotateSpeed;
    bool ground;
    int shape;              // AttackShape; solids render in the lit pass
    float yaw;              // travel heading, orients solid shapes
    float emissive;
    int follow;             // brawler index for caster-anchored layers, else -1
    Vector3 followOffset;
} AttackParticle;

// Per-projectile-slot presentation state for authored visuals: a short position
// history for trails plus the emission clock for projectile-anchored layers.
#define ATTACK_TRAIL_POINTS 24
typedef struct AttackTrailPoint {
    Vector3 position;
    float age;
    bool used;
} AttackTrailPoint;

typedef struct AttackTrail {
    AttackTrailPoint points[ATTACK_TRAIL_POINTS];
    int head;
    bool wasActive;
    float pointTimer;
    float emitTimer;
} AttackTrail;

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
    AttackParticle attackParticles[MAX_ATTACK_PARTICLES];
    AttackTrail attackTrails[MAX_PROJECTILES];
    int droppedVfx;
    int vfxEventsConsumed;
    int vfxLayersSpawned;
    VfxEffectId lastVfxEffect;
    float shake;
    float shakePhase;
} PresentationState;

#endif
