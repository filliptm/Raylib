#ifndef BRAWL_ATTACK_TYPES_H
#define BRAWL_ATTACK_TYPES_H

#include "core_types.h"

// Authorable attack presentation: how an ability looks, decoupled from what it
// does. A document holds layered flipbook effects on cast/impact anchors, a
// projectile visual block, and (Phase E) composed character motions. Unauthored
// abilities keep the compiled legacy recipes.

#define MAX_ATTACK_LAYERS 8
#define MAX_ATTACK_MOTIONS 4
#define MAX_ATTACK_ATLASES 8   // superset of the shipped VFX atlas count

typedef enum {
    ATTACK_ANCHOR_CAST = 0,     // spawned once at the muzzle when the ability fires
    ATTACK_ANCHOR_SELF,         // follows the caster
    ATTACK_ANCHOR_PROJECTILE,   // emitted along a live projectile (Phase D)
    ATTACK_ANCHOR_IMPACT,       // spawned where the projectile lands
    ATTACK_ANCHOR_FIELD_START,  // a field ability lands / a cone spawns
    ATTACK_ANCHOR_FIELD_PULSE,  // every field tick, at the field's current radius
    ATTACK_ANCHOR_FIELD_END,    // the field expires
    ATTACK_ANCHOR_MARK_APPLIED, // a status mark lands on a target (follows it)
    ATTACK_ANCHOR_MARK_TICK,    // every status pulse on that target (follows it)
    ATTACK_ANCHOR_COUNT
} AttackAnchor;

typedef enum {
    ATTACK_PATTERN_SINGLE = 0,
    ATTACK_PATTERN_BURST,       // random directions inside the spread cone
    ATTACK_PATTERN_RING,        // evenly spaced full circle
    ATTACK_PATTERN_CONE,        // evenly fanned inside the spread cone
    ATTACK_PATTERN_COUNT
} AttackPattern;

typedef enum {
    ATTACK_BLEND_ALPHA = 0,
    ATTACK_BLEND_ADDITIVE,
    ATTACK_BLEND_COUNT
} AttackBlendMode;

// Sprite layers are camera-facing flipbook quads; the solid shapes are real lit
// meshes with depth, shading, and the toon ink outline - a travelling shield
// wall, an energy orb, or a flat disc.
typedef enum {
    ATTACK_SHAPE_SPRITE = 0,
    ATTACK_SHAPE_SHIELD,
    ATTACK_SHAPE_ORB,
    ATTACK_SHAPE_DISC,
    ATTACK_SHAPE_DOME,          // hemisphere bubble, for sanctuaries and fields
    ATTACK_SHAPE_COLUMN,        // tall open shell, for light shafts and beams
    ATTACK_SHAPE_COUNT
} AttackShape;

typedef enum {
    ATTACK_MOTION_NONE = 0,
    ATTACK_MOTION_RECOIL,
    ATTACK_MOTION_RAISE_RIGHT_ARM,
    ATTACK_MOTION_RAISE_LEFT_ARM,
    ATTACK_MOTION_SWING_RIGHT,
    ATTACK_MOTION_TWIST,
    ATTACK_MOTION_SLAM,
    ATTACK_MOTION_LEAN,
    ATTACK_MOTION_RAISE_BOTH,   // both arms lifted together
    ATTACK_MOTION_CONDUCT,      // both arms raised with a slow sway
    ATTACK_MOTION_COUNT
} AttackMotionKind;

typedef struct AttackEffectLayer {
    bool used;
    int anchor;                 // AttackAnchor
    int atlas;                  // flipbook atlas slot, clamped by presentation
    int frame;                  // first frame in the atlas
    int frameCount;             // 1 = still image
    float fps;                  // flipbook playback rate
    int pattern;                // AttackPattern
    int count;                  // particles per spawn
    float delay;                // seconds after the anchor event
    float duration;             // particle lifetime
    float forward, up, side;    // local offset from the anchor point
    float spreadDeg;
    float speed;
    float gravity;
    float drag;
    float scaleStart, scaleEnd;
    Color colorStart, colorEnd;
    int blend;                  // AttackBlendMode (sprites only)
    float rotateSpeed;          // degrees per second
    bool ground;                // lie flat on the ground instead of billboarding
    int shape;                  // AttackShape; non-sprite shapes ignore the atlas
    float emissive;             // solid shapes: 0 fully lit .. 1 self-lit
    // Field/mark binding: a looping layer lives exactly as long as its field or
    // mark instead of its own duration; fitField multiplies scale by the field's
    // live radius so rings and domes track growth precisely.
    bool loop;
    bool fitField;
    bool useEventColor;         // tint start/end RGB from the emitting event
} AttackEffectLayer;

typedef struct AttackMotion {
    bool used;
    int kind;                   // AttackMotionKind
    float delay;
    float duration;
    float amplitude;            // 1 = authored strength
} AttackMotion;

typedef struct AttackProjectileVisual {
    bool hideBody;              // suppress the built-in body/halo so layers own the look
    bool tintOverride;
    Color tint;
    float glow;                 // light intensity multiplier, 1 = default
    float visualScale;          // drawn size multiplier, 1 = default
    float spin;                 // visual revolutions per second
    float trailLength;          // seconds of trail history, 0 = none (Phase D)
} AttackProjectileVisual;

typedef struct AttackPresentation {
    bool authored;
    AttackEffectLayer layers[MAX_ATTACK_LAYERS];
    AttackMotion motions[MAX_ATTACK_MOTIONS];
    AttackProjectileVisual projectile;
} AttackPresentation;

#endif
