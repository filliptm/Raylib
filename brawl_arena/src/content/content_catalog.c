#include "content_catalog.h"

#include <math.h>
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

static const CharacterShowcaseDefinition SHOWCASE_DEFAULT = {
    .yawDegrees = 180.0f,
    .scale = 0.90f,
    .offset = { 0.0f, 0.0f },
    .cameraPosition = { 0.0f, 2.7f, -7.6f },
    .cameraTarget = { 0.0f, 1.4f, 0.0f },
    .verticalFov = 40.0f
};

const WeaponDef WEAPON_DEFAULTS[CLASS_COUNT] = {
    {
        .name = "SCRAPPER", .flavor = "Returning ripsaws and a restorative shell",
        .mobilityName = "MAGNETIC SCRAP SHELL", .maxHealth = 3800,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_RETURNING,
        .pellets = 1, .spreadDeg = 0.0f, .speed = 26.0f, .range = 13.0f,
        .damage = 700, .projRadius = 0.42f, .cooldown = 0.40f,
        .reloadPerAmmo = 1.45f, .superPerHit = 0.14f,
        .rangeScaled = false,
        .returnSpeed = 32.0f,
        .secondaryKind = SECONDARY_SHIELD,
        .secondaryCapacity = 1200,
        .secondaryMoveMultiplier = 0.65f,
        .secondaryHealRatio = 0.30f,
        .secondaryRechargeDelay = 3.0f,
        .secondaryRechargeRate = 300.0f,
        .secondaryBreakLockout = 5.0f,
        .superName = "WRECKING DISC", .superKind = SUPER_RETURNING,
        .sPellets = 1, .sSpreadDeg = 0.0f, .sSpeed = 22.0f,
        .sRange = 18.0f, .sDamage = 1100, .sProjRadius = 0.75f,
        .sPiercing = true, .sReturnSpeed = 30.0f,
        .sOutboundPull = 1.25f, .sReturnKnockback = 2.25f
    },
    {
        .name = "LONGSHOT", .flavor = "Damage grows with distance",
        .mobilityName = "MAG-LINE GRAPPLE", .maxHealth = 2800,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_PROJECTILE,
        .pellets = 2, .spreadDeg = 0.0f, .speed = 56.0f, .range = 26.0f,
        .damage = 800, .projRadius = 0.20f, .cooldown = 0.52f,
        .reloadPerAmmo = 1.70f, .superPerHit = 0.15f,
        .rangeScaled = true,
        .secondaryKind = SECONDARY_GRAPPLE,
        .mobilityCooldown = 7.50f,
        .mobilityDuration = 0.45f,
        .secondaryRange = 10.0f,
        .secondaryDelay = 0.25f,
        .superName = "RAILGUN", .superKind = SUPER_PROJECTILE,
        .sPellets = 1, .sSpreadDeg = 0.0f, .sSpeed = 70.0f,
        .sRange = 34.0f, .sDamage = 2400, .sProjRadius = 0.34f,
        .sPiercing = true
    },
    {
        .name = "MORTAR", .flavor = "Lobs over walls, splash damage",
        .mobilityName = "CONCUSSION MINE", .maxHealth = 3200,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_LOB,
        .pellets = 1, .spreadDeg = 0.0f, .speed = 16.0f, .range = 13.0f,
        .damage = 1000, .projRadius = 2.6f, .cooldown = 0.55f,
        .reloadPerAmmo = 1.60f, .superPerHit = 0.30f,
        .rangeScaled = false,
        .secondaryKind = SECONDARY_MINE,
        .mobilityCooldown = 8.0f,
        .secondaryDelay = 0.55f,
        .secondaryRadius = 3.2f,
        .secondaryTriggerRadius = 2.4f,
        .secondaryDamage = 400,
        .secondaryKnockback = 4.5f,
        .superName = "BARRAGE", .superKind = SUPER_PROJECTILE,
        .sPellets = 3, .sSpreadDeg = 26.0f, .sSpeed = 16.0f,
        .sRange = 15.0f, .sDamage = 1100, .sProjRadius = 2.9f,
        .sPiercing = false
    },
    {
        .name = "TANK", .flavor = "Reclaiming rounds and shoulder jets",
        .mobilityName = "SHOULDER JETS", .maxHealth = 5600,
        .maxAmmo = DEFAULT_MAX_AMMO, .mainKind = ATTACK_PROJECTILE,
        .pellets = 4, .spreadDeg = 18.0f, .speed = 30.0f, .range = 7.5f,
        .damage = 440, .projRadius = 0.24f, .cooldown = 0.42f,
        .reloadPerAmmo = 1.25f, .superPerHit = 0.095f,
        .selfHealRatio = 0.20f, .rangeScaled = false,
        .secondaryKind = SECONDARY_DASH,
        .mobilityCooldown = 2.50f, .mobilityDuration = 0.18f,
        .mobilitySpeed = 22.0f,
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
        .healthRegenDelay = 3.0f,
        .healthRegenInterval = 1.0f,
        .healthRegenRatio = 0.13f,
        .playerRespawn = DEFAULT_PLAYER_RESPAWN,
        .enemyRespawn = DEFAULT_ENEMY_RESPAWN,
        .classSwapLockout = 3.0f,
        .inputBuffer = 0.12f,
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
        .styleExposure = 1.0f,
        .styleTonemap = 0.65f,
        .backdropBrightness = 1.0f,
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
        case ATTACK_RETURNING: return ABILITY_BEHAVIOR_RETURNING;
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
        case SUPER_RETURNING: return ABILITY_BEHAVIOR_RETURNING;
        default: return ABILITY_BEHAVIOR_PROJECTILE;
    }
}

static void BuildMain(const WeaponDef *source, const char *characterId,
                      AbilityDefinition *ability)
{
    *ability = (AbilityDefinition){ 0 };
    snprintf(ability->id, sizeof(ability->id), "%s.main", characterId);
    snprintf(ability->name, sizeof(ability->name), "%s",
             strcmp(characterId, "scrapper") == 0 ? "RIPSAW" : "MAIN ATTACK");
    ability->behavior = MainBehavior(source->mainKind);
    ability->range = source->range;
    ability->radius = source->projRadius;
    ability->damage = source->damage;
    ability->healing = source->healing;
    ability->cooldown = source->cooldown;
    // The rebuilt catalog is the one choke point live authoring edits pass through,
    // so values that would divide by zero or stall catch-up loops are floored here.
    ability->reloadPerAmmo = source->reloadPerAmmo < 0.05f ? 0.05f : source->reloadPerAmmo;
    ability->superPerHit = source->superPerHit;
    ability->selfHealRatio = source->selfHealRatio;
    if (ability->behavior == ABILITY_BEHAVIOR_RETURNING)
    {
        ability->data.returning = (ReturningAbility){
            .outboundSpeed = source->speed,
            .returnSpeed = source->returnSpeed,
            .breaksCrates = false
        };
    }
    else if (ability->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
             ability->behavior == ABILITY_BEHAVIOR_LOB)
    {
        ability->data.projectile = (ProjectileAbility){
            source->pellets, source->spreadDeg, source->speed, false, source->rangeScaled
        };
    }
    else
    {
        float tickRate = source->tickRate < 0.05f ? 0.05f : source->tickRate;
        ability->data.area = (AreaAbility){
            source->duration, tickRate, source->growTime, source->spreadDeg, 0.0f
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
    if (ability->behavior == ABILITY_BEHAVIOR_RETURNING)
    {
        ability->data.returning = (ReturningAbility){
            .outboundSpeed = source->sSpeed,
            .returnSpeed = source->sReturnSpeed,
            .outboundPull = source->sOutboundPull,
            .returnKnockback = source->sReturnKnockback,
            .breaksCrates = true
        };
    }
    else if (ability->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
             ability->behavior == ABILITY_BEHAVIOR_LOB)
    {
        ability->data.projectile = (ProjectileAbility){
            source->sPellets, source->sSpreadDeg, source->sSpeed,
            source->sPiercing, false
        };
    }
    else if (ability->behavior == ABILITY_BEHAVIOR_DASH)
    {
        ability->data.dash = (DashAbility){
            .duration = 0.45f,
            .speed = 0.0f, // the shared gameplay dash-speed tuning drives Charge
            .knockback = 3.0f,
            .breaksCrates = true
        };
    }
    else
    {
        float tickRate = source->sTickRate < 0.05f ? 0.05f : source->sTickRate;
        ability->data.area = (AreaAbility){
            source->sDuration, tickRate, 0.0f,
            source->sSpreadDeg, source->sVisualDuration
        };
    }
}

static void BuildSecondary(const WeaponDef *source, const char *characterId,
                           AbilityDefinition *ability)
{
    *ability = (AbilityDefinition){ 0 };
    snprintf(ability->id, sizeof(ability->id), "%s.secondary", characterId);
    snprintf(ability->name, sizeof(ability->name), "%s",
             source->mobilityName ? source->mobilityName : "SECONDARY");
    ability->cooldown = source->mobilityCooldown;
    if (source->secondaryKind == SECONDARY_DASH)
    {
        ability->behavior = ABILITY_BEHAVIOR_DASH;
        ability->range = source->mobilitySpeed*source->mobilityDuration;
        ability->data.dash = (DashAbility){
            .duration = source->mobilityDuration,
            .speed = source->mobilitySpeed,
            .knockback = 0.0f,
            .breaksCrates = false
        };
    }
    else if (source->secondaryKind == SECONDARY_SHIELD)
    {
        ability->behavior = ABILITY_BEHAVIOR_SHIELD;
        ability->data.shield = (ShieldAbility){
            .capacity = source->secondaryCapacity,
            .moveMultiplier = source->secondaryMoveMultiplier,
            .healRatio = source->secondaryHealRatio,
            .rechargeDelay = source->secondaryRechargeDelay,
            .rechargeRate = source->secondaryRechargeRate,
            .breakLockout = source->secondaryBreakLockout
        };
    }
    else if (source->secondaryKind == SECONDARY_GRAPPLE)
    {
        ability->behavior = ABILITY_BEHAVIOR_GRAPPLE;
        ability->range = source->secondaryRange;
        ability->data.grapple = (GrappleAbility){
            .launchDelay = source->secondaryDelay,
            .pullDuration = source->mobilityDuration
        };
    }
    else if (source->secondaryKind == SECONDARY_MINE)
    {
        ability->behavior = ABILITY_BEHAVIOR_MINE;
        ability->radius = source->secondaryRadius;
        ability->damage = source->secondaryDamage;
        ability->data.mine = (MineAbility){
            .armTime = source->secondaryDelay,
            .triggerRadius = source->secondaryTriggerRadius,
            .knockback = source->secondaryKnockback
        };
    }
}

void ContentCatalogRebuildTyped(ContentCatalog *catalog)
{
    memset(catalog->abilities, 0, sizeof(catalog->abilities));
    catalog->abilityCount = 0;
    for (int classId = 0; classId < CLASS_COUNT; classId++)
    {
        const WeaponDef *source = &catalog->weapons[classId];
        int mainId = catalog->abilityCount++;
        int superId = catalog->abilityCount++;
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
        character->mobilityAbility = -1;
        BuildMain(source, character->id, &catalog->abilities[mainId]);
        BuildSuper(source, character->id, &catalog->abilities[superId]);

        bool secondaryReady = false;
        switch (source->secondaryKind)
        {
            case SECONDARY_DASH:
                secondaryReady =
                    source->mobilityCooldown > 0.0f &&
                    source->mobilityDuration > 0.0f &&
                    source->mobilitySpeed > 0.0f;
                break;
            case SECONDARY_SHIELD:
                secondaryReady = source->secondaryCapacity > 0;
                break;
            case SECONDARY_GRAPPLE:
                secondaryReady =
                    source->mobilityCooldown > 0.0f &&
                    source->mobilityDuration > 0.0f &&
                    source->secondaryRange > 0.0f &&
                    source->secondaryDelay >= 0.0f;
                break;
            case SECONDARY_MINE:
                secondaryReady =
                    source->mobilityCooldown > 0.0f &&
                    source->secondaryDelay > 0.0f &&
                    source->secondaryRadius > 0.0f &&
                    source->secondaryTriggerRadius > 0.0f &&
                    source->secondaryDamage > 0;
                break;
            default:
                break;
        }
        if (source->secondaryKind != SECONDARY_NONE && secondaryReady &&
            catalog->abilityCount < MAX_ABILITIES)
        {
            character->mobilityAbility = catalog->abilityCount++;
            BuildSecondary(source, character->id,
                           &catalog->abilities[character->mobilityAbility]);
        }
    }
}

void ContentCatalogResetAll(ContentCatalog *catalog)
{
    memcpy(catalog->weapons, WEAPON_DEFAULTS, sizeof(catalog->weapons));
    catalog->showcase = SHOWCASE_DEFAULT;
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

const AbilityDefinition *ContentMobilityAbility(const ContentCatalog *catalog,
                                                BrawlerClass character)
{
    const CharacterDefinition *definition = ContentCharacter(catalog, character);
    if (!definition || definition->mobilityAbility < 0) return 0;
    return ContentAbility(catalog, definition->mobilityAbility);
}

const AbilityDefinition *ContentSecondaryAbility(const ContentCatalog *catalog,
                                                 BrawlerClass character)
{
    return ContentMobilityAbility(catalog, character);
}

const AbilityDefinition *ContentAbility(const ContentCatalog *catalog, int abilityId)
{
    if (!catalog || abilityId < 0 || abilityId >= catalog->abilityCount) return 0;
    return &catalog->abilities[abilityId];
}

const CharacterShowcaseDefinition *ContentCharacterShowcase(
    const ContentCatalog *catalog)
{
    return catalog ? &catalog->showcase : 0;
}

bool ContentShowcaseValid(const CharacterShowcaseDefinition *showcase)
{
    if (!showcase) return false;
    return isfinite(showcase->yawDegrees) &&
           showcase->scale >= 0.50f && showcase->scale <= 1.50f &&
           fabsf(showcase->offset.x) <= 8.0f &&
           fabsf(showcase->offset.y) <= 4.0f &&
           fabsf(showcase->cameraPosition.x) <= 20.0f &&
           showcase->cameraPosition.y >= 0.2f &&
           showcase->cameraPosition.y <= 12.0f &&
           showcase->cameraPosition.z >= -20.0f &&
           showcase->cameraPosition.z <= -2.0f &&
           fabsf(showcase->cameraTarget.x) <= 8.0f &&
           showcase->cameraTarget.y >= 0.2f &&
           showcase->cameraTarget.y <= 4.0f &&
           fabsf(showcase->cameraTarget.z) <= 8.0f &&
           showcase->verticalFov >= 20.0f &&
           showcase->verticalFov <= 80.0f;
}
