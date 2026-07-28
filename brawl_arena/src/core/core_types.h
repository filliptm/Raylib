#ifndef BRAWL_CORE_TYPES_H
#define BRAWL_CORE_TYPES_H

#include "raylib.h"
#include <stdbool.h>

#define MAX_ARENA_WIDTH 64
#define MAX_ARENA_HEIGHT 64
#define MAX_MAPS 8
#define MAX_MAP_PROPS 64
#define MAP_ID_SIZE 32
#define MAP_NAME_SIZE 64
#define MAP_ASSET_ID_SIZE 48

#define WALL_HEIGHT 2.4f
#define CRATE_HEIGHT 1.9f
#define BUSH_HEIGHT 1.1f
#define GRASS_PER_TILE 29
#define MAX_GRASS_INSTANCES 4096

#define MAX_BRAWLERS 8
#define MAX_PROJECTILES 512
#define MAX_PARTICLES 1024
#define MAX_VFX_INSTANCES 192
#define MAX_FLOATTEXTS 64
#define CHARACTER_TARGET_H 3.1f
#define MAX_GEMS 40
#define MAX_SPAWNS 8
#define MAX_SHOCKWAVES 24
#define MAX_ABILITY_FIELDS 24
#define MAX_FX_LIGHTS 64
#define MAX_SHADER_LIGHTS 8
#define MAX_GAME_EVENTS 1024
#define MAX_STATUS_EFFECTS 4

#define BRAWLER_RADIUS 0.65f
#define DEFAULT_MOVE_SPEED 11.0f
#define DEFAULT_MOVE_ACCEL 30.0f
#define DEFAULT_MAX_AMMO 3
#define DEFAULT_CRATE_HEALTH 1000
#define DEFAULT_MATCH_RESULT_HOLD 4.5f
#define DEFAULT_BUSH_REVEAL_RANGE 3.2f
#define DEFAULT_FIRE_REVEAL_TIME 1.0f
#define DEFAULT_PLAYER_RESPAWN 3.0f
#define DEFAULT_ENEMY_RESPAWN 4.0f

typedef enum { TEAM_PLAYER = 0, TEAM_ENEMY = 1 } Team;

typedef enum {
    CLASS_SHOTGUNNER = 0,
    CLASS_SNIPER,
    CLASS_LOBBER,
    CLASS_BRUISER,
    CLASS_HEALER,
    CLASS_COUNT
} BrawlerClass;

#define MAX_ABILITIES (CLASS_COUNT*3)

typedef enum {
    AI_IDLE = 0,
    AI_CHASE,
    AI_ATTACK,
    AI_RETREAT
} AIState;

typedef enum {
    BOT_STATIC = 0,
    BOT_ROAM,
    BOT_FIGHT,
    BOT_MODE_COUNT
} BotMode;

typedef enum {
    ATTACK_PROJECTILE = 0,
    ATTACK_LOB,
    ATTACK_RAIN,
    ATTACK_KIND_COUNT
} MainAttackKind;

typedef enum {
    SUPER_PROJECTILE = 0,
    SUPER_DASH,
    SUPER_HEALING_BURST,
    SUPER_SOUND_WAVE,
    SUPER_KIND_COUNT
} SuperKind;

typedef enum {
    PARTICLE_SPARK = 0,
    PARTICLE_MUZZLE,
    PARTICLE_SMOKE,
    PARTICLE_DEBRIS
} ParticleType;

// Stable simulation-to-presentation identifiers. Gameplay emits these IDs without
// knowing which textures, layers, blend modes, or fallback geometry render them.
typedef enum {
    VFX_NONE = 0,
    VFX_SCRAPPER_CAST,
    VFX_SCRAPPER_IMPACT,
    VFX_SCRAPPER_SUPER_CAST,
    VFX_SCRAPPER_SUPER_IMPACT,
    VFX_LONGSHOT_CAST,
    VFX_LONGSHOT_IMPACT,
    VFX_LONGSHOT_SUPER_CAST,
    VFX_LONGSHOT_SUPER_IMPACT,
    VFX_MORTAR_CAST,
    VFX_MORTAR_IMPACT,
    VFX_MORTAR_SUPER_CAST,
    VFX_MORTAR_SUPER_IMPACT,
    VFX_TANK_CAST,
    VFX_TANK_IMPACT,
    VFX_TANK_RECLAIM,
    VFX_TANK_JETS_START,
    VFX_TANK_JETS_TRAIL,
    VFX_TANK_CHARGE_START,
    VFX_TANK_CHARGE_TRAIL,
    VFX_TANK_CHARGE_IMPACT,
    VFX_GUARDIAN_RAIN_CAST,
    VFX_GUARDIAN_RAIN_PULSE,
    VFX_GUARDIAN_RAIN_HEAL,
    VFX_GUARDIAN_RAIN_DAMAGE,
    VFX_GUARDIAN_RESONANCE_CAST,
    VFX_GUARDIAN_RESONANCE_HEAL,
    VFX_GUARDIAN_RESONANCE_DAMAGE,
    VFX_GENERIC_HEAL,
    VFX_EFFECT_COUNT
} VfxEffectId;

// Stable attachment points shared by simulation events and presentation. Gameplay
// supplies the semantic socket; the renderer resolves it against the current rig pose
// and falls back to a kit-independent approximate pose when a model is unavailable.
typedef enum {
    VFX_SOCKET_NONE = 0,
    VFX_SOCKET_CENTER,
    VFX_SOCKET_CHEST,
    VFX_SOCKET_LEFT_HAND,
    VFX_SOCKET_RIGHT_HAND,
    VFX_SOCKET_LEFT_SHOULDER,
    VFX_SOCKET_RIGHT_SHOULDER,
    VFX_SOCKET_LEFT_FOOT,
    VFX_SOCKET_RIGHT_FOOT,
    VFX_SOCKET_COUNT
} VfxSocket;

// Presentation-only one-shot actions. These IDs are emitted by deterministic
// gameplay but do not alter hit timing, cooldowns, movement, or any other rule.
typedef enum {
    CHARACTER_ACTION_NONE = 0,
    CHARACTER_ACTION_MAIN,
    CHARACTER_ACTION_SUPER,
    CHARACTER_ACTION_CAST,
    CHARACTER_ACTION_MOBILITY,
    CHARACTER_ACTION_COUNT
} CharacterActionId;

extern const char *CLASS_NAMES[CLASS_COUNT];
extern const char *BOT_MODE_NAMES[BOT_MODE_COUNT];
extern const Color TEAM_COLORS[2];
extern const Color TEAM_DARK[2];

#endif
