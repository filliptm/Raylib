#include "content_catalog.h"

#include <stdio.h>
#include <string.h>

static const char *CHARACTER_IDS[CLASS_COUNT] = {
    "scrapper", "longshot", "mortar", "tank", "guardian"
};

static const char *CHARACTER_MODELS[CLASS_COUNT] = {
    "scrapper", "longshot", "mortar", "tank", "gaia_guardian"
};

static const CharacterRole CHARACTER_ROLES[CLASS_COUNT] = {
    CHARACTER_ROLE_DAMAGE,
    CHARACTER_ROLE_MARKSMAN,
    CHARACTER_ROLE_ARTILLERY,
    CHARACTER_ROLE_TANK,
    CHARACTER_ROLE_SUPPORT
};

const WeaponDef WEAPON_DEFAULTS[CLASS_COUNT] = {
    {
        .name = "SCRAPPER", .flavor = "Close-range spread", .maxHealth = 3800,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_PROJECTILE,
        .pellets = 5, .spreadDeg = 24.0f, .speed = 34.0f, .range = 13.0f,
        .damage = 320, .projRadius = 0.22f, .cooldown = 0.36f,
        .reloadPerAmmo = 1.35f, .superPerHit = 0.085f,
        .rangeScaled = false,
        .superName = "BUCKSHOT", .superKind = SUPER_PROJECTILE,
        .sPellets = 9, .sSpreadDeg = 34.0f, .sSpeed = 36.0f,
        .sRange = 15.0f, .sDamage = 460, .sProjRadius = 0.26f,
        .sPiercing = false
    },
    {
        .name = "LONGSHOT", .flavor = "Damage grows with distance", .maxHealth = 2800,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_PROJECTILE,
        .pellets = 1, .spreadDeg = 0.0f, .speed = 56.0f, .range = 26.0f,
        .damage = 1600, .projRadius = 0.20f, .cooldown = 0.52f,
        .reloadPerAmmo = 1.70f, .superPerHit = 0.30f,
        .rangeScaled = true,
        .superName = "RAILGUN", .superKind = SUPER_PROJECTILE,
        .sPellets = 1, .sSpreadDeg = 0.0f, .sSpeed = 70.0f,
        .sRange = 34.0f, .sDamage = 2400, .sProjRadius = 0.34f,
        .sPiercing = true
    },
    {
        .name = "MORTAR", .flavor = "Lobs over walls, splash damage", .maxHealth = 3200,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_LOB,
        .pellets = 1, .spreadDeg = 0.0f, .speed = 16.0f, .range = 13.0f,
        .damage = 1000, .projRadius = 2.6f, .cooldown = 0.55f,
        .reloadPerAmmo = 1.60f, .superPerHit = 0.30f,
        .rangeScaled = false,
        .superName = "BARRAGE", .superKind = SUPER_PROJECTILE,
        .sPellets = 3, .sSpreadDeg = 26.0f, .sSpeed = 16.0f,
        .sRange = 15.0f, .sDamage = 1100, .sProjRadius = 2.9f,
        .sPiercing = false
    },
    {
        .name = "TANK", .flavor = "Tanky brawler with a charge", .maxHealth = 5600,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_PROJECTILE,
        .pellets = 4, .spreadDeg = 18.0f, .speed = 30.0f, .range = 7.5f,
        .damage = 440, .projRadius = 0.24f, .cooldown = 0.42f,
        .reloadPerAmmo = 1.25f, .superPerHit = 0.095f,
        .rangeScaled = false,
        .superName = "CHARGE", .superKind = SUPER_DASH,
        .sPellets = 0, .sSpreadDeg = 0.0f, .sSpeed = 0.0f,
        .sRange = 0.0f, .sDamage = 1200, .sProjRadius = 0.0f,
        .sPiercing = false
    },
    {
        .name = "GUARDIAN",
        .flavor = "Restorative rain and resonant waves",
        .maxHealth = 3400,
        .maxAmmo = DEFAULT_MAX_AMMO,
        .mainKind = ATTACK_RAIN,
        .pellets = 0,
        .spreadDeg = 0.0f,
        .speed = 0.0f,
        .range = 17.0f,
        .damage = 100,
        .healing = 100,
        .projRadius = 3.4f,
        .cooldown = 0.42f,
        .reloadPerAmmo = 1.55f,
        .superPerHit = 0.04f,
        .rangeScaled = false,
        .duration = 1.35f,
        .tickRate = 0.15f,
        .growTime = 0.594f,
        .superName = "RESONANCE",
        .superKind = SUPER_SOUND_WAVE,
        .sPellets = 0,
        .sSpreadDeg = 90.0f,
        .sSpeed = 0.0f,
        .sRange = 14.0f,
        .sDamage = 180,
        .sHealing = 220,
        .sProjRadius = 0.0f,
        .sPiercing = false,
        .sDuration = 2.10f,
        .sTickRate = 0.35f,
        .sVisualDuration = 0.70f
    }
};

const char *CLASS_NAMES[CLASS_COUNT] = {
    "SCRAPPER", "LONGSHOT", "MORTAR", "TANK", "GUARDIAN"
};
const char *BOT_MODE_NAMES[BOT_MODE_COUNT] = {
    "STATIC", "ROAM", "FIGHT"
};
const Color TEAM_COLORS[2] = {
    { 70, 140, 235, 255 },
    { 232, 82, 82, 255 }
};
const Color TEAM_DARK[2] = {
    { 38, 82, 158, 255 },
    { 158, 42, 42, 255 }
};

void TuningSetDefaults(Tuning *tuning)
{
    *tuning = (Tuning){
        .moveSpeed = DEFAULT_MOVE_SPEED,
        .moveAccel = DEFAULT_MOVE_ACCEL,
        .dashSpeed = 26.0f,
        .bushReveal = DEFAULT_BUSH_REVEAL_RANGE,
        .fireReveal = DEFAULT_FIRE_REVEAL_TIME,
        .playerRespawn = DEFAULT_PLAYER_RESPAWN,
        .enemyRespawn = DEFAULT_ENEMY_RESPAWN,
        .matchResultHold = DEFAULT_MATCH_RESULT_HOLD,
        .timeScale = 1.0f,
        .crateHealth = DEFAULT_CRATE_HEALTH,
        .superMult = 1.0f,
        .modelCharacter = true,
        .toon = true,
        .toonBands = 3.0f,
        .toonOutline = 0.85f,
        .styleSaturation = 1.25f,
        .styleBrightness = 1.10f,
        .styleVignette = 0.85f,
        .postFx = true,
        .bloom = 0.85f,
        .selectedKit = CLASS_SHOTGUNNER,
        .gemGrab = true,
        .teamSize = 3,
        .gemsToWin = 10,
        .gemCountdown = 15.0f,
        .gemVentInterval = 6.5f,
        .grassHeight = 2.0f,
        .windStrength = 0.30f,
        .windSpeed = 1.7f,
        .grassBendRadius = 2.1f,
        .grassBendStrength = 1.55f,
        .concealDither = 0.35f,
        .botMode = BOT_STATIC,
        .botCount = 3,
        .botKit = CLASS_SHOTGUNNER,
        .botMixedKits = true,
        .aiRetreatHealth = 0.30f,
        .aiSupportHealth = 0.85f,
        .aiSupportSuperHealth = 0.60f,
        .aiProbeAhead = 1.60f
    };
}

static AbilityBehavior MainBehavior(MainAttackKind kind)
{
    switch (kind)
    {
        case ATTACK_LOB: return ABILITY_BEHAVIOR_LOB;
        case ATTACK_RAIN: return ABILITY_BEHAVIOR_RAIN;
        default: return ABILITY_BEHAVIOR_PROJECTILE;
    }
}

static AbilityBehavior SuperBehavior(SuperKind kind)
{
    switch (kind)
    {
        case SUPER_DASH: return ABILITY_BEHAVIOR_DASH;
        case SUPER_HEALING_BURST: return ABILITY_BEHAVIOR_HEALING_BURST;
        case SUPER_SOUND_WAVE: return ABILITY_BEHAVIOR_SOUND_WAVE;
        default: return ABILITY_BEHAVIOR_PROJECTILE;
    }
}

static void BuildMain(const WeaponDef *source, const char *characterId,
                      AbilityDefinition *ability)
{
    *ability = (AbilityDefinition){ 0 };
    snprintf(ability->id, sizeof(ability->id), "%s.main", characterId);
    snprintf(ability->name, sizeof(ability->name), "%s", "MAIN ATTACK");
    ability->behavior = MainBehavior(source->mainKind);
    ability->range = source->range;
    ability->radius = source->projRadius;
    ability->damage = source->damage;
    ability->healing = source->healing;
    ability->cooldown = source->cooldown;
    ability->reloadPerAmmo = source->reloadPerAmmo;
    ability->superPerHit = source->superPerHit;
    if (ability->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
        ability->behavior == ABILITY_BEHAVIOR_LOB)
    {
        ability->data.projectile = (ProjectileAbility){
            source->pellets, source->spreadDeg, source->speed, false, source->rangeScaled
        };
    }
    else
    {
        ability->data.area = (AreaAbility){
            source->duration, source->tickRate, source->growTime, source->spreadDeg, 0.0f
        };
    }
}

static void BuildSuper(const WeaponDef *source, const char *characterId,
                       AbilityDefinition *ability)
{
    *ability = (AbilityDefinition){ 0 };
    snprintf(ability->id, sizeof(ability->id), "%s.super", characterId);
    snprintf(ability->name, sizeof(ability->name), "%s", source->superName);
    ability->behavior = (source->superKind == SUPER_PROJECTILE &&
                         source->mainKind == ATTACK_LOB)
                      ? ABILITY_BEHAVIOR_LOB : SuperBehavior(source->superKind);
    ability->range = source->sRange;
    ability->radius = source->sProjRadius;
    ability->damage = source->sDamage;
    ability->healing = source->sHealing;
    if (ability->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
        ability->behavior == ABILITY_BEHAVIOR_LOB)
    {
        ability->data.projectile = (ProjectileAbility){
            source->sPellets, source->sSpreadDeg, source->sSpeed,
            source->sPiercing, false
        };
    }
    else if (ability->behavior == ABILITY_BEHAVIOR_DASH)
        ability->data.dash.duration = 0.45f;
    else
    {
        ability->data.area = (AreaAbility){
            source->sDuration, source->sTickRate, 0.0f,
            source->sSpreadDeg, source->sVisualDuration
        };
    }
}

void ContentCatalogRebuildTyped(ContentCatalog *catalog)
{
    catalog->abilityCount = CLASS_COUNT*2;
    for (int classId = 0; classId < CLASS_COUNT; classId++)
    {
        const WeaponDef *source = &catalog->weapons[classId];
        int mainId = classId*2;
        int superId = mainId + 1;
        CharacterDefinition *character = &catalog->characters[classId];
        *character = (CharacterDefinition){ 0 };
        snprintf(character->id, sizeof(character->id), "%s", CHARACTER_IDS[classId]);
        snprintf(character->displayName, sizeof(character->displayName), "%s", source->name);
        snprintf(character->flavor, sizeof(character->flavor), "%s", source->flavor);
        snprintf(character->modelAsset, sizeof(character->modelAsset), "%s",
                 CHARACTER_MODELS[classId]);
        character->role = CHARACTER_ROLES[classId];
        character->maxHealth = source->maxHealth;
        character->maxAmmo = source->maxAmmo;
        character->mainAbility = mainId;
        character->superAbility = superId;
        BuildMain(source, character->id, &catalog->abilities[mainId]);
        BuildSuper(source, character->id, &catalog->abilities[superId]);
    }
}

void ContentCatalogResetAll(ContentCatalog *catalog)
{
    memcpy(catalog->weapons, WEAPON_DEFAULTS, sizeof(catalog->weapons));
    ContentCatalogRebuildTyped(catalog);
}

void ContentCatalogResetCharacter(ContentCatalog *catalog, BrawlerClass character)
{
    if (!catalog || character < 0 || character >= CLASS_COUNT) return;
    catalog->weapons[character] = WEAPON_DEFAULTS[character];
    ContentCatalogRebuildTyped(catalog);
}

const CharacterDefinition *ContentCharacter(const ContentCatalog *catalog,
                                            BrawlerClass character)
{
    if (!catalog || character < 0 || character >= CLASS_COUNT) return 0;
    return &catalog->characters[character];
}

const AbilityDefinition *ContentMainAbility(const ContentCatalog *catalog,
                                            BrawlerClass character)
{
    const CharacterDefinition *definition = ContentCharacter(catalog, character);
    return definition ? &catalog->abilities[definition->mainAbility] : 0;
}

const AbilityDefinition *ContentSuperAbility(const ContentCatalog *catalog,
                                             BrawlerClass character)
{
    const CharacterDefinition *definition = ContentCharacter(catalog, character);
    return definition ? &catalog->abilities[definition->superAbility] : 0;
}
