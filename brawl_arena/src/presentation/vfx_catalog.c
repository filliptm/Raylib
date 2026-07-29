#include "vfx_catalog.h"

#include <stdio.h>

#define WHITE_TINT (Color){ 255, 255, 255, 255 }

#define SHAPE_BILLBOARD(frame, duration_, from_, to_, alpha0_, alpha1_, anchor_, event_) \
    { \
        .atlas = VFX_ATLAS_SHAPES, .firstFrame = (frame), .frameCount = 1, \
        .framesPerSecond = 1.0f, .duration = (duration_), \
        .startScale = (from_), .endScale = (to_), \
        .startAlpha = (alpha0_), .endAlpha = (alpha1_), .yOffset = 0.75f, \
        .orientation = VFX_ORIENT_BILLBOARD, .blend = VFX_BLEND_ADDITIVE, \
        .anchor = (anchor_), .useEventColor = (event_), .color = WHITE_TINT \
    }

#define SHAPE_GROUND(frame, duration_, from_, to_, alpha0_, alpha1_, anchor_, event_) \
    { \
        .atlas = VFX_ATLAS_SHAPES, .firstFrame = (frame), .frameCount = 1, \
        .framesPerSecond = 1.0f, .duration = (duration_), \
        .startScale = (from_), .endScale = (to_), \
        .startAlpha = (alpha0_), .endAlpha = (alpha1_), .yOffset = 0.012f, \
        .orientation = VFX_ORIENT_GROUND, .blend = VFX_BLEND_ADDITIVE, \
        .anchor = (anchor_), .useEventColor = (event_), .color = WHITE_TINT \
    }

#define SHAPE_ALPHA(frame, delay_, duration_, from_, to_, alpha0_, alpha1_, anchor_) \
    { \
        .atlas = VFX_ATLAS_SHAPES, .firstFrame = (frame), .frameCount = 1, \
        .framesPerSecond = 1.0f, .delay = (delay_), .duration = (duration_), \
        .startScale = (from_), .endScale = (to_), \
        .startAlpha = (alpha0_), .endAlpha = (alpha1_), .yOffset = 0.65f, \
        .rotationSpeed = 24.0f, .orientation = VFX_ORIENT_BILLBOARD, \
        .blend = VFX_BLEND_ALPHA, .anchor = (anchor_), .color = { 185, 194, 210, 255 } \
    }

#define SHAPE_BEAM(frame, duration_, width_, alpha0_, alpha1_, event_) \
    { \
        .atlas = VFX_ATLAS_SHAPES, .firstFrame = (frame), .frameCount = 1, \
        .framesPerSecond = 1.0f, .duration = (duration_), \
        .startScale = (width_), .endScale = (width_)*0.55f, \
        .startAlpha = (alpha0_), .endAlpha = (alpha1_), \
        .orientation = VFX_ORIENT_BEAM, .blend = VFX_BLEND_ADDITIVE, \
        .anchor = VFX_ANCHOR_START, .useEventColor = (event_), .color = WHITE_TINT \
    }

#define ANIM_BILLBOARD(atlas_, frames_, fps_, delay_, duration_, from_, to_, alpha0_, alpha1_, anchor_, event_, blend_) \
    { \
        .atlas = (atlas_), .firstFrame = 0, .frameCount = (frames_), \
        .framesPerSecond = (fps_), .delay = (delay_), .duration = (duration_), \
        .startScale = (from_), .endScale = (to_), \
        .startAlpha = (alpha0_), .endAlpha = (alpha1_), .yOffset = 0.72f, \
        .orientation = VFX_ORIENT_BILLBOARD, .blend = (blend_), \
        .anchor = (anchor_), .useEventColor = (event_), .color = WHITE_TINT \
    }

#define ANIM_GROUND(atlas_, frames_, fps_, duration_, from_, to_, alpha0_, alpha1_, anchor_, event_) \
    { \
        .atlas = (atlas_), .firstFrame = 0, .frameCount = (frames_), \
        .framesPerSecond = (fps_), .duration = (duration_), \
        .startScale = (from_), .endScale = (to_), \
        .startAlpha = (alpha0_), .endAlpha = (alpha1_), .yOffset = 0.026f, \
        .orientation = VFX_ORIENT_GROUND, .blend = VFX_BLEND_ADDITIVE, \
        .anchor = (anchor_), .useEventColor = (event_), .color = WHITE_TINT \
    }

static const char *ATLAS_PATHS[VFX_ATLAS_COUNT] = {
    "build/assets/vfx/shapes.png",
    "build/assets/vfx/explosion.png",
    "build/assets/vfx/water.png",
    "build/assets/vfx/energy_loop.png",
    "build/assets/vfx/air_burst.png",
    "build/assets/vfx/divine_impact.png",
    "build/assets/vfx/smoke_loop.png"
};

static const int ATLAS_COLUMNS[VFX_ATLAS_COUNT] = { 4, 8, 8, 8, 8, 8, 8 };
static const int ATLAS_ROWS[VFX_ATLAS_COUNT] = { 2, 4, 4, 4, 4, 2, 8 };
static const int ATLAS_FRAMES[VFX_ATLAS_COUNT] = { 8, 32, 32, 32, 32, 16, 64 };

// Kenney shapes atlas frame indices.
enum {
    SHAPE_MUZZLE = 0,
    SHAPE_TRACE,
    SHAPE_FLAME,
    SHAPE_SMOKE,
    SHAPE_SPARK,
    SHAPE_MAGIC,
    SHAPE_SCORCH,
    SHAPE_CIRCLE
};

static const VfxRecipeDefinition RECIPES[VFX_EFFECT_COUNT] = {
    [VFX_SCRAPPER_CAST] = {
        "scrapper_cast", 1.25f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.28f, 0.72f, 1.75f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.34f, 0.45f, 1.35f, 0.9f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_IMPACT] = {
        "scrapper_impact", 0.85f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.40f, 0.50f, 1.55f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.46f, 0.42f, 1.15f, 0.82f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_SUPER_CAST] = {
        "scrapper_super_cast", 1.35f, 2, 2, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.38f, 0.88f, 2.05f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 12, 30.0f, 0.0f, 0.40f, 0.52f, 1.35f, 0.85f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE)
        }
    },
    [VFX_SCRAPPER_SUPER_IMPACT] = {
        "scrapper_super_impact", 1.15f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 16, 30.0f, 0.0f, 0.54f, 0.58f, 1.55f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.58f, 0.50f, 1.55f, 0.9f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_RETURN] = {
        "scrapper_return", 1.15f, 1, 2, {
            SHAPE_BEAM(SHAPE_TRACE, 0.34f, 0.34f, 0.9f, 0.0f, true),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.36f, 0.42f, 1.20f, 0.8f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_CATCH] = {
        "scrapper_catch", 1.0f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.38f, 0.48f, 1.42f, 1.0f, 0.0f, VFX_ANCHOR_END, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.40f, 0.34f, 0.96f, 0.72f, 0.0f, VFX_ANCHOR_END, true)
        }
    },
    [VFX_SCRAPPER_SHIELD_START] = {
        "scrapper_shield_start", 1.25f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_ENERGY_LOOP, 16, 32.0f, 0.0f, 0.50f, 0.42f, 1.36f, 0.9f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_MAGIC, 0.54f, 0.46f, 1.35f, 0.82f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_SHIELD_HIT] = {
        "scrapper_shield_hit", 1.15f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 16, 42.0f, 0.0f, 0.38f, 0.36f, 1.18f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.30f, 0.40f, 1.30f, 1.0f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_SHIELD_BREAK] = {
        "scrapper_shield_break", 1.45f, 3, 3, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 24, 44.0f, 0.0f, 0.54f, 0.48f, 1.52f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.52f, 0.52f, 1.62f, 0.9f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.44f, 0.52f, 1.82f, 1.0f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_SCRAPPER_SHIELD_RESTORE] = {
        "scrapper_shield_restore", 1.30f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_ENERGY_LOOP, 16, 30.0f, 0.0f, 0.54f, 0.46f, 1.48f, 0.9f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_MAGIC, 0.62f, 0.48f, 1.44f, 0.86f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_LONGSHOT_CAST] = {
        "longshot_cast", 1.0f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.26f, 0.46f, 1.25f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_BEAM(SHAPE_TRACE, 0.30f, 0.32f, 1.0f, 0.0f, true)
        }
    },
    [VFX_LONGSHOT_IMPACT] = {
        "longshot_impact", 1.05f, 1, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 16, 36.0f, 0.0f, 0.45f, 0.38f, 1.08f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_BILLBOARD(SHAPE_TRACE, 0.38f, 0.38f, 1.05f, 0.82f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_LONGSHOT_SUPER_CAST] = {
        "longshot_super_cast", 1.35f, 3, 3, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.40f, 0.70f, 1.80f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_BEAM(SHAPE_TRACE, 0.48f, 0.52f, 1.0f, 0.0f, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.52f, 0.42f, 1.55f, 0.9f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_LONGSHOT_SUPER_IMPACT] = {
        "longshot_super_impact", 1.25f, 3, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 24, 50.0f, 0.0f, 0.48f, 0.35f, 1.20f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 12, 42.0f, 0.0f, 0.30f, 0.30f, 0.95f, 0.75f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE)
        }
    },
    [VFX_LONGSHOT_GRAPPLE_FIRE] = {
        "longshot_grapple_fire", 1.55f, 2, 1, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.30f, 0.72f, 1.82f, 1.0f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_LONGSHOT_GRAPPLE_HOOK] = {
        "longshot_grapple_hook", 1.35f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 16, 44.0f, 0.0f, 0.38f, 0.36f, 1.35f, 1.0f, 0.0f, VFX_ANCHOR_END, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.48f, 0.44f, 1.38f, 0.92f, 0.0f, VFX_ANCHOR_END, true)
        }
    },
    [VFX_LONGSHOT_GRAPPLE_PULL] = {
        "longshot_grapple_pull", 1.30f, 2, 2, {
            SHAPE_BEAM(SHAPE_TRACE, 0.48f, 0.46f, 0.96f, 0.0f, true),
            ANIM_BILLBOARD(VFX_ATLAS_ENERGY_LOOP, 16, 36.0f, 0.0f, 0.48f, 0.34f, 1.18f, 0.86f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE)
        }
    },
    [VFX_LONGSHOT_GRAPPLE_LAND] = {
        "longshot_grapple_land", 1.45f, 2, 2, {
            ANIM_GROUND(VFX_ATLAS_AIR_BURST, 24, 42.0f, 0.58f, 0.56f, 1.62f, 0.94f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.52f, 0.52f, 1.48f, 0.84f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_MORTAR_CAST] = {
        "mortar_cast", 1.0f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.32f, 0.62f, 1.55f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_ALPHA(SHAPE_SMOKE, 0.04f, 0.78f, 0.45f, 1.55f, 0.66f, 0.0f, VFX_ANCHOR_START)
        }
    },
    [VFX_MORTAR_IMPACT] = {
        "mortar_impact", 2.6f, 2, 3, {
            ANIM_BILLBOARD(VFX_ATLAS_EXPLOSION, 32, 45.0f, 0.0f, 0.72f, 0.45f, 1.00f, 1.0f, 0.0f, VFX_ANCHOR_START, false, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_SCORCH, 2.7f, 0.65f, 0.90f, 0.45f, 0.0f, VFX_ANCHOR_START, false),
            ANIM_BILLBOARD(VFX_ATLAS_SMOKE_LOOP, 64, 40.0f, 0.16f, 1.45f, 0.35f, 0.90f, 0.42f, 0.0f, VFX_ANCHOR_START, false, VFX_BLEND_ALPHA)
        }
    },
    [VFX_MORTAR_SUPER_CAST] = {
        "mortar_super_cast", 1.4f, 2, 3, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.20f, 0.55f, 1.50f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 12, 40.0f, 0.0f, 0.30f, 0.30f, 0.95f, 0.75f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_ALPHA(SHAPE_SMOKE, 0.05f, 0.60f, 0.45f, 1.35f, 0.62f, 0.0f, VFX_ANCHOR_START)
        }
    },
    [VFX_MORTAR_SUPER_IMPACT] = {
        "mortar_super_impact", 2.9f, 3, 3, {
            ANIM_BILLBOARD(VFX_ATLAS_EXPLOSION, 32, 42.0f, 0.0f, 0.76f, 0.55f, 1.15f, 1.0f, 0.0f, VFX_ANCHOR_START, false, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_SCORCH, 3.2f, 0.72f, 1.10f, 0.55f, 0.0f, VFX_ANCHOR_START, false),
            ANIM_BILLBOARD(VFX_ATLAS_SMOKE_LOOP, 64, 38.0f, 0.14f, 1.60f, 0.42f, 1.05f, 0.46f, 0.0f, VFX_ANCHOR_START, false, VFX_BLEND_ALPHA)
        }
    },
    [VFX_MORTAR_MINE_PLACE] = {
        "mortar_mine_place", 1.45f, 2, 3, {
            SHAPE_BEAM(SHAPE_TRACE, 0.34f, 0.30f, 0.92f, 0.0f, true),
            SHAPE_GROUND(SHAPE_MAGIC, 0.52f, 0.44f, 1.48f, 0.92f, 0.0f, VFX_ANCHOR_END, true),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.38f, 0.38f, 1.22f, 0.82f, 0.0f, VFX_ANCHOR_END, true)
        }
    },
    [VFX_MORTAR_MINE_ARM] = {
        "mortar_mine_arm", 1.55f, 2, 2, {
            ANIM_GROUND(VFX_ATLAS_ENERGY_LOOP, 24, 38.0f, 0.66f, 0.46f, 1.52f, 0.94f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.46f, 0.42f, 1.58f, 1.0f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_MORTAR_MINE_DETONATE] = {
        "mortar_mine_detonate", 3.20f, 3, 3, {
            ANIM_BILLBOARD(VFX_ATLAS_EXPLOSION, 32, 46.0f, 0.0f, 0.82f, 0.62f, 1.46f, 1.0f, 0.0f, VFX_ANCHOR_START, false, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_SCORCH, 2.9f, 0.82f, 1.15f, 0.54f, 0.0f, VFX_ANCHOR_START, false),
            ANIM_BILLBOARD(VFX_ATLAS_SMOKE_LOOP, 48, 38.0f, 0.12f, 1.34f, 0.44f, 1.18f, 0.48f, 0.0f, VFX_ANCHOR_START, false, VFX_BLEND_ALPHA)
        }
    },
    [VFX_TANK_CAST] = {
        "tank_cast", 1.15f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_MUZZLE, 0.30f, 0.76f, 1.85f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.38f, 0.46f, 1.35f, 0.88f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_TANK_IMPACT] = {
        "tank_impact", 1.0f, 1, 2, {
            SHAPE_BILLBOARD(SHAPE_SPARK, 0.44f, 0.50f, 1.55f, 1.0f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.48f, 0.42f, 1.30f, 0.82f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_TANK_RECLAIM] = {
        "tank_reclaim", 0.85f, 2, 2, {
            SHAPE_BEAM(SHAPE_TRACE, 0.58f, 0.28f, 1.0f, 0.0f, true),
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 16, 34.0f, 0.04f, 0.50f, 0.34f, 1.00f, 0.92f, 0.0f, VFX_ANCHOR_END, true, VFX_BLEND_ADDITIVE)
        }
    },
    [VFX_TANK_JETS_START] = {
        "tank_jets_start", 1.05f, 2, 2, {
            SHAPE_BEAM(SHAPE_FLAME, 0.38f, 0.60f, 1.0f, 0.0f, true),
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 16, 40.0f, 0.0f, 0.40f, 0.34f, 1.20f, 0.82f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE)
        }
    },
    [VFX_TANK_JETS_TRAIL] = {
        "tank_jets_trail", 0.78f, 0, 2, {
            SHAPE_BEAM(SHAPE_FLAME, 0.28f, 0.42f, 0.82f, 0.0f, true),
            SHAPE_ALPHA(SHAPE_SMOKE, 0.0f, 0.52f, 0.32f, 1.12f, 0.46f, 0.0f, VFX_ANCHOR_END)
        }
    },
    [VFX_TANK_CHARGE_START] = {
        "tank_charge_start", 1.45f, 3, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 16, 30.0f, 0.0f, 0.56f, 0.54f, 1.55f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.62f, 0.48f, 1.75f, 0.95f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_TANK_CHARGE_TRAIL] = {
        "tank_charge_trail", 1.0f, 1, 2, {
            SHAPE_BEAM(SHAPE_TRACE, 0.32f, 0.58f, 0.88f, 0.0f, true),
            SHAPE_ALPHA(SHAPE_SMOKE, 0.0f, 0.58f, 0.38f, 1.30f, 0.50f, 0.0f, VFX_ANCHOR_END)
        }
    },
    [VFX_TANK_CHARGE_IMPACT] = {
        "tank_charge_impact", 1.55f, 3, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 28, 38.0f, 0.0f, 0.74f, 0.58f, 1.65f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.76f, 0.48f, 1.75f, 0.95f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GUARDIAN_RAIN_CAST] = {
        "guardian_rain_cast", 1.3f, 2, 3, {
            SHAPE_BEAM(SHAPE_TRACE, 0.62f, 0.26f, 0.85f, 0.0f, true),
            SHAPE_GROUND(SHAPE_MAGIC, 0.90f, 0.45f, 1.45f, 0.92f, 0.0f, VFX_ANCHOR_END, true),
            ANIM_BILLBOARD(VFX_ATLAS_WATER, 24, 34.0f, 0.04f, 0.70f, 0.44f, 1.30f, 0.90f, 0.0f, VFX_ANCHOR_END, false, VFX_BLEND_ADDITIVE)
        }
    },
    [VFX_GUARDIAN_RAIN_PULSE] = {
        "guardian_rain_pulse", 1.0f, 1, 2, {
            ANIM_GROUND(VFX_ATLAS_ENERGY_LOOP, 24, 36.0f, 0.66f, 0.58f, 1.40f, 0.80f, 0.0f, VFX_ANCHOR_START, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.62f, 0.50f, 1.45f, 0.86f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GUARDIAN_RAIN_HEAL] = {
        "guardian_rain_heal", 1.0f, 1, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 16, 30.0f, 0.0f, 0.54f, 0.38f, 1.15f, 0.90f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_MAGIC, 0.58f, 0.38f, 1.05f, 0.72f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GUARDIAN_RAIN_DAMAGE] = {
        "guardian_rain_damage", 1.0f, 1, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 18, 34.0f, 0.0f, 0.54f, 0.38f, 1.18f, 0.92f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_SCORCH, 0.60f, 0.38f, 1.08f, 0.70f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GUARDIAN_RESONANCE_CAST] = {
        "guardian_resonance_cast", 1.45f, 3, 3, {
            ANIM_BILLBOARD(VFX_ATLAS_AIR_BURST, 32, 34.0f, 0.0f, 0.94f, 0.54f, 1.65f, 1.0f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_BEAM(SHAPE_TRACE, 0.72f, 0.48f, 0.88f, 0.0f, true),
            SHAPE_GROUND(SHAPE_CIRCLE, 0.90f, 0.50f, 1.90f, 0.95f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GUARDIAN_RESONANCE_HEAL] = {
        "guardian_resonance_heal", 1.0f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_ENERGY_LOOP, 32, 30.0f, 0.0f, 1.06f, 0.66f, 1.10f, 0.86f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_MAGIC, 1.00f, 0.56f, 1.18f, 0.68f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GUARDIAN_RESONANCE_DAMAGE] = {
        "guardian_resonance_damage", 1.0f, 2, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_ENERGY_LOOP, 32, 30.0f, 0.0f, 1.06f, 0.66f, 1.12f, 0.84f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_CIRCLE, 1.00f, 0.56f, 1.22f, 0.66f, 0.0f, VFX_ANCHOR_START, true)
        }
    },
    [VFX_GENERIC_HEAL] = {
        "generic_heal", 0.95f, 1, 2, {
            ANIM_BILLBOARD(VFX_ATLAS_DIVINE_IMPACT, 16, 30.0f, 0.0f, 0.54f, 0.34f, 1.00f, 0.82f, 0.0f, VFX_ANCHOR_START, true, VFX_BLEND_ADDITIVE),
            SHAPE_GROUND(SHAPE_MAGIC, 0.62f, 0.38f, 1.00f, 0.64f, 0.0f, VFX_ANCHOR_START, true)
        }
    }
};

const VfxRecipeDefinition *VfxCatalogGet(VfxEffectId effect)
{
    if (effect <= VFX_NONE || effect >= VFX_EFFECT_COUNT) return NULL;
    const VfxRecipeDefinition *recipe = &RECIPES[effect];
    return (recipe->name && recipe->layerCount > 0) ? recipe : NULL;
}

const char *VfxAtlasPath(VfxAtlasId atlas)
{
    if (atlas < 0 || atlas >= VFX_ATLAS_COUNT) return NULL;
    return ATLAS_PATHS[atlas];
}

void VfxAtlasGrid(VfxAtlasId atlas, int *columns, int *rows, int *frames)
{
    if (columns) *columns = 0;
    if (rows) *rows = 0;
    if (frames) *frames = 0;
    if (atlas < 0 || atlas >= VFX_ATLAS_COUNT) return;
    if (columns) *columns = ATLAS_COLUMNS[atlas];
    if (rows) *rows = ATLAS_ROWS[atlas];
    if (frames) *frames = ATLAS_FRAMES[atlas];
}

bool VfxCatalogValidate(char *message, int messageCapacity)
{
    for (int effect = VFX_NONE + 1; effect < VFX_EFFECT_COUNT; effect++)
    {
        const VfxRecipeDefinition *recipe = VfxCatalogGet((VfxEffectId)effect);
        if (!recipe)
        {
            if (message && messageCapacity > 0)
                snprintf(message, (size_t)messageCapacity,
                         "effect %d has no recipe", effect);
            return false;
        }
        if (recipe->layerCount > VFX_MAX_RECIPE_LAYERS ||
            recipe->defaultSize <= 0.0f)
        {
            if (message && messageCapacity > 0)
                snprintf(message, (size_t)messageCapacity,
                         "%s has invalid recipe metadata", recipe->name);
            return false;
        }
        for (int layerIndex = 0; layerIndex < recipe->layerCount; layerIndex++)
        {
            const VfxLayerDefinition *layer = &recipe->layers[layerIndex];
            int columns = 0, rows = 0, frames = 0;
            VfxAtlasGrid(layer->atlas, &columns, &rows, &frames);
            if (columns <= 0 || rows <= 0 ||
                layer->firstFrame < 0 || layer->frameCount <= 0 ||
                layer->firstFrame + layer->frameCount > frames ||
                layer->duration <= 0.0f ||
                layer->startScale < 0.0f || layer->endScale < 0.0f)
            {
                if (message && messageCapacity > 0)
                    snprintf(message, (size_t)messageCapacity,
                             "%s layer %d is invalid", recipe->name, layerIndex);
                return false;
            }
        }
    }
    if (message && messageCapacity > 0)
        snprintf(message, (size_t)messageCapacity,
                 "%d recipes valid", VFX_EFFECT_COUNT - 1);
    return true;
}
