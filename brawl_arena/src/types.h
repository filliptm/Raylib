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
#define MAX_FX_LIGHTS 64
#define MAX_SHADER_LIGHTS 8     // how many make it to the GPU each frame

//------------------------------------------------------------------------------------
// Gameplay tuning
//------------------------------------------------------------------------------------
#define BRAWLER_RADIUS 0.65f
#define MOVE_SPEED 11.0f
#define MOVE_ACCEL 30.0f
#define MAX_AMMO 3
#define CRATE_HEALTH 1000
#define BUSH_REVEAL_RANGE 3.2f      // enemies this close see you inside a bush
#define FIRE_REVEAL_TIME 1.0f       // attacking reveals you for this long
#define PLAYER_RESPAWN 3.0f
#define ENEMY_RESPAWN 4.0f

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
    Vector3 playerSpawn;
    Vector3 enemySpawns[8];
    int enemySpawnCount;
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
    CLASS_COUNT
} BrawlerClass;

typedef enum {
    AI_IDLE = 0,
    AI_CHASE,
    AI_ATTACK,
    AI_RETREAT
} AIState;

// How the bots behave. Static is the default: inert targets you can shoot at while
// you dial in weapon feel, without being shot back at.
typedef enum {
    BOT_STATIC = 0,     // stand still, never fire
    BOT_ROAM,           // patrol the arena, never fire
    BOT_FIGHT,          // full combat AI
    BOT_MODE_COUNT
} BotMode;

// A weapon is the whole identity of a class: its main attack and its super.
typedef struct WeaponDef {
    const char *name;
    const char *flavor;
    int maxHealth;

    // Main attack
    int pellets;
    float spreadDeg;
    float speed;
    float range;
    int damage;             // per pellet
    float projRadius;
    float cooldown;
    float reloadPerAmmo;    // seconds to regain one ammo pip
    float superPerHit;      // super meter gained per pellet landed (0..1)
    bool arcing;            // lobbed over walls, splashes where it lands
    bool rangeScaled;       // damage ramps up with distance travelled

    // Super
    const char *superName;
    int sPellets;
    float sSpreadDeg;
    float sSpeed;
    float sRange;
    int sDamage;
    float sProjRadius;
    bool sPiercing;
    bool sDash;             // super is a dash rather than a projectile
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

    bool inBush;
    bool visible;           // recomputed every frame, from the player's viewpoint
    bool alive;
    float respawnTimer;

    Team team;
    BrawlerClass cls;
    bool isPlayer;

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
    int hitMask;            // brawlers already damaged, so piercing shots hit once each

    Color color;
    bool active;
} Projectile;

//------------------------------------------------------------------------------------
// Effects
//------------------------------------------------------------------------------------
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
// Live tuning, all editable from the command center at runtime.
// The #defines above are the starting values; these are what gameplay actually reads.
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
    float timeScale;

    // Cheats / sandbox helpers
    float superMult;        // multiplier on super charge gained per hit
    bool godMode;
    bool infiniteAmmo;
    bool showDebug;         // range rings and line-of-sight overlay

    // Presentation
    bool postFx;            // bloom + vignette pass
    float bloom;            // bloom strength

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
} Tuning;

void TuningSetDefaults(Tuning *t);

//------------------------------------------------------------------------------------
// World
//------------------------------------------------------------------------------------
typedef struct World {
    Arena arena;
    Brawler brawlers[MAX_BRAWLERS];
    int brawlerCount;
    Projectile projectiles[MAX_PROJECTILES];
    Particle particles[MAX_PARTICLES];
    FloatText texts[MAX_FLOATTEXTS];
    FxLight lights[MAX_FX_LIGHTS];

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
} World;

// Live weapon table, edited by the command center. WEAPON_DEFAULTS is the pristine
// baseline used to reset a kit.
extern WeaponDef WEAPONS[CLASS_COUNT];
extern const WeaponDef WEAPON_DEFAULTS[CLASS_COUNT];

extern const char *CLASS_NAMES[CLASS_COUNT];
extern const char *BOT_MODE_NAMES[BOT_MODE_COUNT];

// Team colors, shared by render and hud
extern const Color TEAM_COLORS[2];
extern const Color TEAM_DARK[2];

#endif // TYPES_H
