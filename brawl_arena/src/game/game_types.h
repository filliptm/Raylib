#ifndef BRAWL_GAME_TYPES_H
#define BRAWL_GAME_TYPES_H

#include "content_types.h"

typedef enum {
    TILE_FLOOR = 0,
    TILE_WALL,
    TILE_CRATE,
    TILE_BUSH
} TileType;

typedef struct Tile {
    TileType type;
    int health;
    float hitFlash;
} Tile;

typedef struct Arena {
    char mapId[MAP_ID_SIZE];
    char mapName[MAP_NAME_SIZE];
    int width;
    int height;
    float tileSize;
    Tile tiles[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    // Body-clearance routing bitmap, derived from tiles. Rebuilt for the whole map
    // at load and refreshed around a crate when it breaks; navVersion bumps so
    // cached routes know to recompute. Deterministic: derived only from tile state.
    bool navigable[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    unsigned int navVersion;
    MapVisualCell visual[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    MapPropDefinition props[MAX_MAP_PROPS];
    int propCount;
    Vector3 playerSpawns[MAX_SPAWNS];
    int playerSpawnCount;
    Vector3 enemySpawns[MAX_SPAWNS];
    int enemySpawnCount;
    Vector3 gemVent;
} Arena;

typedef struct StatusEffect {
    float remaining;
    float tickTimer;
    float tickRate;
    int damage;
    int healing;
    int source;
    Team sourceTeam;
    bool active;
} StatusEffect;

typedef struct Brawler {
    Vector3 position;
    Vector3 velocity;
    Vector3 moveIntent;
    float aimAngle;
    float moveFacing;
    float renderYaw;
    float shotYaw;
    float aimHold;
    float bobPhase;
    int health;
    int maxHealth;
    float ammo;
    float superCharge;
    float attackCd;
    float hitFlash;
    float revealTimer;
    float lastCombatTime;
    float lastHealthRegenPulseTime;
    float spawnScale;
    float mobilityCooldown;
    bool shieldActive;
    bool shieldRearmRequired;
    float shieldCharge;
    float shieldRechargeDelay;
    float shieldBrokenTimer;
    int shieldAbility;
    float dashTimer;
    float dashVfxTimer;
    Vector3 dashDir;
    int dashAbility;
    int dashHitMask;
    float grappleDelayTimer;
    float grappleTimer;
    Vector3 grappleAnchor;
    int grappleAbility;
    StatusEffect statuses[MAX_STATUS_EFFECTS];
    bool inBush;
    bool visible;
    bool alive;
    float respawnTimer;
    Team team;
    BrawlerClass cls;
    bool isPlayer;
    int spawnSlot;
    int gems;
    AIState aiState;
    float aiTimer;
    float aiReactTimer;
    int aiTarget;
    Vector3 aiWander;
    // Cached route: packed goal/waypoint tiles plus the arena nav version and age
    // they were computed against. -1 goal means no route is cached.
    int aiNavGoal;
    int aiNavWaypoint;
    float aiNavAge;
    unsigned int aiNavVersion;
    float strafeDir;
    // Strafe flips own their cadence; sharing aiTimer with the wander re-pick made
    // state transitions inherit stale durations.
    float aiStrafeTimer;
    bool deliberateAim;
} Brawler;

// dashHitMask/hitMask pack brawler indices into bit positions of an int; this trips
// at compile time if the roster capacity ever outgrows that representation.
typedef char BrawlerHitMaskCapacityCheck[(MAX_BRAWLERS <= 32) ? 1 : -1];

typedef enum {
    PROJECTILE_MOTION_STRAIGHT = 0,
    PROJECTILE_MOTION_RETURNING
} ProjectileMotion;

typedef struct Projectile {
    Vector3 position;
    Vector3 velocity;
    Vector3 origin;
    float traveled;
    float range;
    float returnSpeed;
    float outboundPull;
    float returnKnockback;
    int damage;
    int healing;
    float selfHealRatio;
    float radius;
    Team team;
    int owner;
    bool arcing;
    float arcT;
    float arcDur;
    Vector3 arcStart;
    Vector3 arcEnd;
    float arcHeight;
    bool piercing;
    bool breaksWalls;
    bool isSuper;
    bool rangeScaled;
    bool outbound;
    bool breaksCrates;
    ProjectileMotion motion;
    BrawlerClass ownerClass;
    int hitMask;
    int abilityIndex;       // ability that fired this projectile, -1 if untracked
    Color color;
    bool active;
} Projectile;

typedef enum {
    ABILITY_FIELD_RAIN = 0,
    ABILITY_FIELD_SOUND_WAVE,
    ABILITY_FIELD_MINE
} AbilityFieldType;

typedef struct AbilityField {
    Vector3 position;
    float angle;
    float radius;
    float range;
    float spread;
    float growTime;
    float life;
    float maxLife;
    float tickTimer;
    float tickRate;
    float armTime;
    float triggerRadius;
    float knockback;
    int damage;
    int healing;
    Team team;
    int owner;
    AbilityFieldType type;
    bool armed;
    bool active;
} AbilityField;

typedef struct Gem {
    Vector3 position;
    Vector3 velocity;
    float spin;
    float bobPhase;
    float settleTimer;
    float pickupDelay;
    bool active;
} Gem;

typedef enum {
    MATCH_PLAYING = 0,
    MATCH_COUNTDOWN,
    MATCH_OVER
} MatchPhase;

typedef struct Match {
    MatchPhase phase;
    float ventTimer;
    int teamGems[2];
    int countdownTeam;
    float countdown;
    int winner;
    float overTimer;
} Match;

typedef enum {
    GAME_EVENT_MUZZLE = 0,
    GAME_EVENT_IMPACT,
    GAME_EVENT_EXPLOSION,
    GAME_EVENT_DEATH,
    GAME_EVENT_CRATE_BREAK,
    GAME_EVENT_FLOAT_TEXT,
    GAME_EVENT_LIGHT,
    GAME_EVENT_SHOCKWAVE,
    GAME_EVENT_PARTICLE,
    GAME_EVENT_VFX,
    GAME_EVENT_CHARACTER_ACTION,
    GAME_EVENT_MATCH_SHAKE,
    // Authored-attack anchors: presentation resolves the ability's document.
    GAME_EVENT_ATTACK_CAST,
    GAME_EVENT_ATTACK_IMPACT
} GameEventType;

typedef struct GameEvent {
    GameEventType type;
    Vector3 position;
    Vector3 endPosition;
    Vector3 velocity;
    Color color;
    float angle;
    float radius;
    float life;
    float size;
    int count;
    ParticleType particleType;
    VfxEffectId vfxId;
    VfxSocket startSocket;
    VfxSocket endSocket;
    CharacterActionId characterAction;
    int sourceBrawler;
    int targetBrawler;
    int abilityIndex;       // -1 unless the event is bound to an ability document
    char text[16];
} GameEvent;

typedef struct GameEventQueue {
    GameEvent items[MAX_GAME_EVENTS];
    int count;
    int dropped;
} GameEventQueue;

typedef struct GameRandom {
    unsigned int state;
} GameRandom;

typedef struct GameSession {
    Arena arena;
    Brawler brawlers[MAX_BRAWLERS];
    int brawlerCount;
    Projectile projectiles[MAX_PROJECTILES];
    AbilityField abilityFields[MAX_ABILITY_FIELDS];
    Gem gems[MAX_GEMS];
    Match match;
    GameEventQueue events;
    GameRandom random;
    float time;
    int playerIdx;
    int kills;
    int deaths;
    bool sandbox;
} GameSession;

// The complete mutation surface available to deterministic simulation code.
// It deliberately contains no controller, camera, rendering, UI, or application-flow
// state. Passing this instead of App makes that boundary enforceable.
typedef struct GameContext {
    GameSession *session;
    const Tuning *tuning;
    const ContentCatalog *content;
} GameContext;

#endif
