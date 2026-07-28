#ifndef BRAWL_CONTENT_TYPES_H
#define BRAWL_CONTENT_TYPES_H

#include "core_types.h"

typedef enum {
    MAP_PALETTE_ORANGE = 0,
    MAP_PALETTE_PURPLE
} MapPalette;

typedef enum {
    MAP_VISUAL_DEFAULT = 0,
    MAP_VISUAL_DOOR,
    MAP_VISUAL_DISPLAY,
    MAP_VISUAL_FLOOR_ACCENT,
    MAP_VISUAL_CARGO,
    MAP_VISUAL_TECH
} MapVisualKind;

typedef struct MapVisualCell {
    unsigned char palette;
    unsigned char kind;
} MapVisualCell;

typedef struct MapPropDefinition {
    char asset[MAP_ASSET_ID_SIZE];
    Vector3 position;
    Vector3 scale;
    float yawDegrees;
    unsigned char palette;
    float emissive;
} MapPropDefinition;

typedef struct MapDefinition {
    int formatVersion;
    char id[MAP_ID_SIZE];
    char name[MAP_NAME_SIZE];
    int width;
    int height;
    float tileSize;
    char terrain[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    char gameplay[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    MapVisualCell visual[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    MapPropDefinition props[MAX_MAP_PROPS];
    int propCount;
} MapDefinition;

// Compatibility record for the stable tuning.cfg/gameplay.cfg schema. Runtime code
// consumes the tagged CharacterDefinition and AbilityDefinition records below.
typedef struct WeaponDef {
    const char *name;
    const char *flavor;
    const char *mobilityName;
    int maxHealth;
    int maxAmmo;
    MainAttackKind mainKind;
    int pellets;
    float spreadDeg;
    float speed;
    float range;
    int damage;
    int healing;
    float projRadius;
    float cooldown;
    float reloadPerAmmo;
    float superPerHit;
    float selfHealRatio;
    bool rangeScaled;
    float duration;
    float tickRate;
    float growTime;
    float returnSpeed;
    SecondaryKind secondaryKind;
    float mobilityCooldown;
    float mobilityDuration;
    float mobilitySpeed;
    int secondaryCapacity;
    float secondaryMoveMultiplier;
    float secondaryHealRatio;
    float secondaryRechargeDelay;
    float secondaryRechargeRate;
    float secondaryBreakLockout;
    const char *superName;
    SuperKind superKind;
    int sPellets;
    float sSpreadDeg;
    float sSpeed;
    float sRange;
    int sDamage;
    int sHealing;
    float sProjRadius;
    bool sPiercing;
    float sDuration;
    float sTickRate;
    float sVisualDuration;
    float sReturnSpeed;
    float sOutboundPull;
    float sReturnKnockback;
} WeaponDef;

typedef enum {
    CHARACTER_ROLE_DAMAGE = 0,
    CHARACTER_ROLE_MARKSMAN,
    CHARACTER_ROLE_ARTILLERY,
    CHARACTER_ROLE_TANK,
    CHARACTER_ROLE_SUPPORT
} CharacterRole;

typedef enum {
    ABILITY_BEHAVIOR_PROJECTILE = 0,
    ABILITY_BEHAVIOR_LOB,
    ABILITY_BEHAVIOR_RAIN,
    ABILITY_BEHAVIOR_DASH,
    ABILITY_BEHAVIOR_HEALING_BURST,
    ABILITY_BEHAVIOR_SOUND_WAVE,
    ABILITY_BEHAVIOR_RETURNING,
    ABILITY_BEHAVIOR_SHIELD
} AbilityBehavior;

typedef struct ProjectileAbility {
    int pellets;
    float spreadDegrees;
    float speed;
    bool piercing;
    bool rangeScaled;
} ProjectileAbility;

typedef struct AreaAbility {
    float duration;
    float tickRate;
    float growTime;
    float spreadDegrees;
    float visualDuration;
} AreaAbility;

typedef struct DashAbility {
    float duration;
    float speed;
    float knockback;
    bool breaksCrates;
} DashAbility;

typedef struct ReturningAbility {
    float outboundSpeed;
    float returnSpeed;
    float outboundPull;
    float returnKnockback;
    bool breaksCrates;
} ReturningAbility;

typedef struct ShieldAbility {
    int capacity;
    float moveMultiplier;
    float healRatio;
    float rechargeDelay;
    float rechargeRate;
    float breakLockout;
} ShieldAbility;

typedef struct AbilityDefinition {
    char id[48];
    char name[48];
    AbilityBehavior behavior;
    float range;
    float radius;
    int damage;
    int healing;
    float cooldown;
    float reloadPerAmmo;
    float superPerHit;
    float selfHealRatio;
    union {
        ProjectileAbility projectile;
        AreaAbility area;
        DashAbility dash;
        ReturningAbility returning;
        ShieldAbility shield;
    } data;
} AbilityDefinition;

typedef struct CharacterDefinition {
    char id[32];
    char displayName[48];
    char flavor[96];
    char modelAsset[48];
    CharacterRole role;
    int maxHealth;
    int maxAmmo;
    int mainAbility;
    int superAbility;
    int mobilityAbility;
} CharacterDefinition;

// One content-owned camera/transform contract for every non-interactive character
// preview. Candidate and screen changes swap only the model.
typedef struct CharacterShowcaseDefinition {
    float yawDegrees;
    float scale;
    Vector2 offset;
    Vector3 cameraPosition;
    Vector3 cameraTarget;
    float verticalFov;
} CharacterShowcaseDefinition;

typedef struct ContentCatalog {
    WeaponDef weapons[CLASS_COUNT];
    CharacterDefinition characters[CLASS_COUNT];
    CharacterShowcaseDefinition showcase;
    AbilityDefinition abilities[MAX_ABILITIES];
    int abilityCount;
    MapDefinition maps[MAX_MAPS];
    int mapCount;
    int selectedMap;
} ContentCatalog;

typedef struct Tuning {
    float moveSpeed;
    float moveAccel;
    float dashSpeed;
    float bushReveal;
    float fireReveal;
    float healthRegenDelay;
    float healthRegenInterval;
    float healthRegenRatio;
    float playerRespawn;
    float enemyRespawn;
    float matchResultHold;
    float timeScale;
    int crateHealth;
    float superMult;
    bool godMode;
    bool infiniteAmmo;
    bool showDebug;
    bool modelCharacter;
    bool toon;
    float toonBands;
    float toonOutline;
    float stylePixelate;
    float stylePainterly;
    float styleHalftone;
    float stylePosterize;
    float styleGrain;
    float styleCA;
    float styleSaturation;
    float styleBrightness;
    float styleVignette;
    bool postFx;
    float bloom;
    int selectedKit;
    int statWins;
    int statLosses;
    int statKos;
    bool gemGrab;
    int teamSize;
    int gemsToWin;
    float gemCountdown;
    float gemVentInterval;
    float grassHeight;
    float windStrength;
    float windSpeed;
    float grassBendRadius;
    float grassBendStrength;
    float concealDither;
    BotMode botMode;
    int botCount;
    BrawlerClass botKit;
    bool botMixedKits;
    float aiRetreatHealth;
    float aiSupportHealth;
    float aiSupportSuperHealth;
    float aiProbeAhead;
} Tuning;

void TuningSetDefaults(Tuning *tuning);

typedef struct ConfigState {
    Tuning projectTuning;
    WeaponDef projectWeapons[CLASS_COUNT];
    CharacterShowcaseDefinition projectShowcase;
    bool projectLoaded;
    bool recoveryDefaults;
    bool legacyImported;
    char status[160];
} ConfigState;

extern const WeaponDef WEAPON_DEFAULTS[CLASS_COUNT];

#endif
