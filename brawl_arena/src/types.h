/*******************************************************************************************
*   BRAWL ARENA - Shared types and tuning constants
*
*   Every module operates on a single `World` struct passed by pointer, so there is
*   no hidden global state and systems stay independently testable.
********************************************************************************************/
#ifndef TYPES_H
#define TYPES_H

#include "raylib.h"
#include <stdbool.h>

//------------------------------------------------------------------------------------
// Arena / world sizing
//------------------------------------------------------------------------------------
#define ARENA_W 33
#define ARENA_H 23
#define TILE_SIZE 2.0f

#define WALL_HEIGHT 2.4f
#define CRATE_HEIGHT 1.9f
#define BUSH_HEIGHT 1.1f
#define GRASS_PER_TILE 29       // blade clumps scattered over each bush tile
#define MAX_GRASS_INSTANCES 4096

//------------------------------------------------------------------------------------
// Pool sizes
//------------------------------------------------------------------------------------
#define MAX_BRAWLERS 8
#define MAX_PROJECTILES 512
#define MAX_PARTICLES 1024
#define MAX_FLOATTEXTS 64
#define CHARACTER_TARGET_H 3.1f   // world height every character model is normalised to
#define MAX_GEMS 40
#define MAX_SPAWNS 8
#define MAX_SHOCKWAVES 24
#define MAX_ABILITY_FIELDS 24
#define MAX_FX_LIGHTS 64
#define MAX_SHADER_LIGHTS 8     // how many make it to the GPU each frame

//------------------------------------------------------------------------------------
// Gameplay tuning
//------------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------------
// Tiles
//------------------------------------------------------------------------------------
typedef enum {
    TILE_FLOOR = 0,
    TILE_WALL,          // indestructible
    TILE_CRATE,         // destructible by supers
    TILE_BUSH           // walkable, hides whoever stands in it
} TileType;

typedef struct Tile {
    TileType type;
    int health;         // crates only
    float hitFlash;
} Tile;

typedef struct Arena {
    Tile tiles[ARENA_H][ARENA_W];

    // One spawn per brawler slot, per side. Index 0 of the player side is the human.
    Vector3 playerSpawns[MAX_SPAWNS];
    int playerSpawnCount;
    Vector3 enemySpawns[MAX_SPAWNS];
    int enemySpawnCount;

    Vector3 gemVent;        // where gems well up, at the contested centre
} Arena;

//------------------------------------------------------------------------------------
// Brawlers
//------------------------------------------------------------------------------------
typedef enum { TEAM_PLAYER = 0, TEAM_ENEMY = 1 } Team;

typedef enum {
    CLASS_SHOTGUNNER = 0,   // close-range pellet spread
    CLASS_SNIPER,           // long-range single shot, damage scales with distance
    CLASS_LOBBER,           // arcing projectile that clears walls, splash on landing
    CLASS_BRUISER,          // short-range burst, high HP, dash super
    CLASS_HEALER,           // rain field pulses on both teams; sound-wave super adds HoT/DoT
    CLASS_COUNT
} BrawlerClass;

typedef enum {
    AI_IDLE = 0,
    AI_CHASE,
    AI_ATTACK,
    AI_RETREAT
} AIState;

// Which screen the application is on. The match is just one of them, so every gameplay
// system has to be gated on it or menu clicks leak into the arena.
typedef enum {
    SCREEN_MENU = 0,
    SCREEN_BRAWLERS,        // character select
    SCREEN_MATCH,
    SCREEN_COUNT
} AppScreen;

// How the bots behave. Static is the default: inert targets you can shoot at while
// you dial in weapon feel, without being shot back at.
typedef enum {
    BOT_STATIC = 0,     // stand still, never fire
    BOT_ROAM,           // patrol the arena, never fire
    BOT_FIGHT,          // full combat AI
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

// A weapon is the whole identity of a class: its main attack and its super.
typedef struct WeaponDef {
    const char *name;
    const char *flavor;
    int maxHealth;
    int maxAmmo;

    // Main attack
    MainAttackKind mainKind;
    int pellets;
    float spreadDeg;
    float speed;
    float range;
    int damage;             // per pellet
    int healing;            // per pellet when it hits an injured ally
    float projRadius;
    float cooldown;
    float reloadPerAmmo;    // seconds to regain one ammo pip
    float superPerHit;      // super meter gained per pellet landed (0..1)
    bool rangeScaled;       // damage ramps up with distance travelled
    float duration;         // persistent main-attack lifetime, when applicable
    float tickRate;         // persistent main-attack pulse interval
    float growTime;         // seconds for a persistent area to reach full size

    // Super
    const char *superName;
    SuperKind superKind;
    int sPellets;
    float sSpreadDeg;
    float sSpeed;
    float sRange;
    int sDamage;
    int sHealing;           // healing delivered by a healing-burst super
    float sProjRadius;
    bool sPiercing;
    float sDuration;        // timed effect duration
    float sTickRate;        // timed effect pulse interval
    float sVisualDuration;  // cast visualization lifetime
} WeaponDef;

typedef struct Brawler {
    Vector3 position;
    Vector3 velocity;
    Vector3 moveIntent;     // unit direction the controller wants to go, set every frame
    float aimAngle;         // radians, where the weapon points
    float moveFacing;       // radians, where the body leans
    float renderYaw;        // smoothed facing actually used for drawing
    float shotYaw;          // direction of the last shot, held briefly after firing
    float aimHold;          // seconds left facing shotYaw before returning to movement
    float bobPhase;

    int health;
    int maxHealth;
    float ammo;             // fractional so reload can animate smoothly
    float superCharge;      // 0..1
    float attackCd;

    float hitFlash;
    float revealTimer;      // >0 means visible even inside a bush
    float spawnScale;       // pop-in animation

    float dashTimer;
    Vector3 dashDir;
    int dashHitMask;

    // Timed effect applied by the Guardian's sound wave. Its source team decides
    // whether each pulse heals this brawler or damages them.
    float resonanceTimer;
    float resonanceTickTimer;
    float resonanceTickRate;
    int resonanceDamage;
    int resonanceHealing;
    int resonanceSource;
    Team resonanceTeam;

    bool inBush;
    bool visible;           // recomputed every frame, from the player's viewpoint
    bool alive;
    float respawnTimer;

    Team team;
    BrawlerClass cls;
    bool isPlayer;
    int spawnSlot;          // which of its team's spawn points this brawler returns to
    int gems;               // gems currently carried

    // AI scratch state
    AIState aiState;
    float aiTimer;
    float aiReactTimer;
    int aiTarget;
    Vector3 aiWander;
    float strafeDir;
} Brawler;

//------------------------------------------------------------------------------------
// Projectiles
//------------------------------------------------------------------------------------
typedef struct Projectile {
    Vector3 position;
    Vector3 velocity;
    Vector3 origin;
    float traveled;
    float range;

    int damage;
    int healing;
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
    int hitMask;            // brawlers already affected, so piercing shots hit once each

    Color color;
    bool active;
} Projectile;

//------------------------------------------------------------------------------------
// Effects
//------------------------------------------------------------------------------------
typedef enum {
    ABILITY_FIELD_RAIN = 0,
    ABILITY_FIELD_SOUND_WAVE
} AbilityFieldType;

// Persistent ability geometry. Rain fields also own their gameplay pulses; sound-wave
// fields are the short-lived cast visualization after their targets have been marked.
typedef struct AbilityField {
    Vector3 position;
    float angle;
    float radius;
    float range;
    float spread;
    float growTime;
    float life, maxLife;
    float tickTimer, tickRate;
    int damage;
    int healing;
    Team team;
    int owner;
    AbilityFieldType type;
    bool active;
} AbilityField;

typedef enum { PARTICLE_SPARK = 0, PARTICLE_MUZZLE, PARTICLE_SMOKE, PARTICLE_DEBRIS } ParticleType;

typedef struct Particle {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life, maxLife;
    float size;
    ParticleType type;
    bool active;
} Particle;

// A gem lying on the floor. Dropped gems pop outward on a short arc before settling.
typedef struct Gem {
    Vector3 position;
    Vector3 velocity;
    float spin;
    float bobPhase;
    float settleTimer;      // >0 while still arcing out of a kill
    float pickupDelay;      // brief grace so a death drop is not instantly re-grabbed
    bool active;
} Gem;

typedef enum {
    MATCH_PLAYING = 0,
    MATCH_COUNTDOWN,        // a team holds the target and the clock is running
    MATCH_OVER
} MatchPhase;

typedef struct Match {
    MatchPhase phase;
    float ventTimer;        // until the next gem surfaces
    int teamGems[2];        // recomputed each frame from what brawlers carry
    int countdownTeam;      // team the countdown belongs to, -1 when none
    float countdown;
    int winner;             // -1 until someone takes it
    float overTimer;
} Match;

// Expanding ground ring left by a detonation. Gives a blast a readable size and edge
// instead of just a cloud of sparks.
typedef struct Shockwave {
    Vector3 position;
    float maxRadius;
    float life, maxLife;
    Color color;
    bool active;
} Shockwave;

// Short-lived point lights fed to the scene shader: muzzle flashes, blasts, supers.
typedef struct FxLight {
    Vector3 position;
    Color color;
    float radius;
    float life, maxLife;
    bool active;
} FxLight;

typedef struct FloatText {
    Vector3 world;
    char text[16];
    Color color;
    float life, maxLife;
    float rise;
    float scale;
    bool active;
} FloatText;

//------------------------------------------------------------------------------------
// Effective tuning read by gameplay and edited from the command center. Normal startup
// populates these values from the canonical project configuration; compiled values are
// only the recovery seed used before that load.
//------------------------------------------------------------------------------------
typedef struct Tuning {
    // Movement
    float moveSpeed;
    float moveAccel;
    float dashSpeed;

    // Stealth
    float bushReveal;
    float fireReveal;

    // Match flow
    float playerRespawn;
    float enemyRespawn;
    float matchResultHold;
    float timeScale;
    int crateHealth;

    // Cheats / sandbox helpers
    float superMult;        // multiplier on super charge gained per hit
    bool godMode;
    bool infiniteAmmo;
    bool showDebug;         // range rings and line-of-sight overlay

    // Presentation
    bool modelCharacter;    // draw the rigged model instead of the primitive brawler
    bool toon;              // cel-banded light + ink outlines: the illustrated look
    float toonBands;        // light bands, 2..4
    float toonOutline;      // ink outline strength, 0..1

    // Viewport stylization. Every effect is a 0..1 strength (0 = off) so any mix of
    // them composes; they all live in the single post-pass shader.
    float stylePixelate;
    float stylePainterly;
    float styleHalftone;
    float stylePosterize;
    float styleGrain;
    float styleCA;          // chromatic aberration
    float styleSaturation;  // 0..2, 1 = neutral
    float styleBrightness;  // 0.6..1.5, 1 = neutral
    float styleVignette;    // 0..1.5

    bool postFx;            // bloom + vignette pass
    float bloom;            // bloom strength

    // Profile: real numbers so the menu shows something earned rather than decorative.
    int selectedKit;
    int statWins;
    int statLosses;
    int statKos;

    // Match rules
    bool gemGrab;           // off = the free-form sandbox this build started as
    int teamSize;
    int gemsToWin;
    float gemCountdown;
    float gemVentInterval;

    // Grass
    float grassHeight;      // blade height in world units; ~2.0 matches a brawler
    float windStrength;
    float windSpeed;
    float grassBendRadius;  // how far from a brawler blades start pushing aside
    float grassBendStrength;
    float concealDither;    // 0 solid, 1 invisible: how ghosted a hidden brawler looks

    // Bots
    BotMode botMode;
    int botCount;
    BrawlerClass botKit;
    bool botMixedKits;
    float aiRetreatHealth;
    float aiSupportHealth;
    float aiSupportSuperHealth;
    float aiProbeAhead;
} Tuning;

void TuningSetDefaults(Tuning *t);

// Canonical project defaults are kept beside the effective runtime values so the
// command center can show provenance, discard a draft, or promote selected changes.
typedef struct ConfigState {
    Tuning projectTuning;
    WeaponDef projectWeapons[CLASS_COUNT];
    bool projectLoaded;
    bool recoveryDefaults;
    bool legacyImported;
    char status[160];
} ConfigState;

//------------------------------------------------------------------------------------
// World
//------------------------------------------------------------------------------------
typedef struct World {
    Arena arena;
    Brawler brawlers[MAX_BRAWLERS];
    int brawlerCount;
    Projectile projectiles[MAX_PROJECTILES];
    AbilityField abilityFields[MAX_ABILITY_FIELDS];
    Particle particles[MAX_PARTICLES];
    FloatText texts[MAX_FLOATTEXTS];
    FxLight lights[MAX_FX_LIGHTS];
    Shockwave waves[MAX_SHOCKWAVES];
    Gem gems[MAX_GEMS];
    Match match;

    Camera3D camera;
    Vector3 camFocus;
    float shake;

    float time;
    int playerIdx;

    // Player aiming state, owned by player.c and read by render.c for the preview
    Vector3 aimPoint;       // ground point under the cursor
    float aimDist;
    bool charging;
    float chargeTime;
    bool aimingSuper;

    int kills;
    int deaths;

    Tuning tune;
    ConfigState config;
    bool matchRestartPending;   // a rule changed that only a rebuild can apply

    // Screen flow. `fade` runs 0..1..0 across a transition and `pending` is the screen
    // to swap to at the darkest point, so a switch never shows a half-built scene.
    AppScreen screen;
    AppScreen pending;
    float fade;
    bool fadingOut;
    bool matchResultBanked;     // guards against counting one result twice
    bool quitRequested;

    // True while the match is a tuning sandbox rather than a real game. Session state,
    // not a saved setting: entering the sandbox must not rewrite the mode PLAY uses.
    bool sandbox;
} World;

// Live weapon table, edited by the command center. WEAPON_DEFAULTS is compiled recovery
// data; normal resets use the canonical project snapshot in World.config.
extern WeaponDef WEAPONS[CLASS_COUNT];
extern const WeaponDef WEAPON_DEFAULTS[CLASS_COUNT];

extern const char *CLASS_NAMES[CLASS_COUNT];
extern const char *BOT_MODE_NAMES[BOT_MODE_COUNT];

// Team colors, shared by render and hud
extern const Color TEAM_COLORS[2];
extern const Color TEAM_DARK[2];

#endif // TYPES_H
