/*******************************************************************************************
*   BRAWL ARENA CONFIGURATION
*
*   config/gameplay.cfg is the tracked project truth. tuning.local.cfg is an ignored,
*   sparse authoring draft, and profile.cfg contains only personal/profile state.
*   Every load is transactional: a candidate is parsed and validated before it can
*   replace the effective runtime values.
********************************************************************************************/
#include "config.h"
#include "content_catalog.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 384
#define SAVE_DELAY 0.6f
#define CONFIG_FORMAT_VERSION 3

typedef enum {
    FIELD_FLOAT = 0,
    FIELD_INT,
    FIELD_BOOL,
    FIELD_ATTACK_KIND,
    FIELD_SUPER_KIND,
    FIELD_SECONDARY_KIND
} FieldType;

typedef enum {
    SCOPE_PROJECT = 0,
    SCOPE_LOCAL,
    SCOPE_PROFILE
} FieldScope;

typedef struct Field {
    char key[96];
    FieldType type;
    FieldScope scope;
    void *ptr;
    double minimum;
    double maximum;
    int kit;
    bool seen;
} Field;

static const char *KIT_KEYS[CLASS_COUNT] = {
    "scrapper", "longshot", "mortar", "tank", "guardian"
};

static const char *ATTACK_KIND_NAMES[ATTACK_KIND_COUNT] = {
    "projectile", "lob", "rain", "returning"
};

static const char *SUPER_KIND_NAMES[SUPER_KIND_COUNT] = {
    "projectile", "dash", "healing_burst", "sound_wave", "returning"
};

static const char *SECONDARY_KIND_NAMES[SECONDARY_KIND_COUNT] = {
    "none", "dash", "shield"
};

static bool g_dirty = false;
static float g_saveTimer = 0.0f;

//------------------------------------------------------------------------------------
// Paths
//------------------------------------------------------------------------------------
static const char *PathFromEnvironment(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return (value && value[0]) ? value : fallback;
}

static const char *ProjectPath(void)
{
    return PathFromEnvironment("BRAWL_PROJECT_CONFIG", PROJECT_CONFIG_PATH);
}

static const char *LocalPath(void)
{
    return PathFromEnvironment("BRAWL_TUNING", LOCAL_CONFIG_PATH);
}

static const char *ProfilePath(void)
{
    return PathFromEnvironment("BRAWL_PROFILE", PROFILE_CONFIG_PATH);
}

static const char *LegacyPath(void)
{
    const char *explicitPath = getenv("BRAWL_LEGACY_TUNING");
    if (explicitPath && explicitPath[0]) return explicitPath;

    // An isolated BRAWL_TUNING path is used by automated checks. It must never
    // accidentally import the developer's real legacy file into that test.
    const char *isolated = getenv("BRAWL_TUNING");
    return (isolated && isolated[0]) ? NULL : LEGACY_CONFIG_PATH;
}

static bool PathExists(const char *path)
{
    if (!path || !path[0]) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    fclose(file);
    return true;
}

static void SetStatus(App *w, const char *message)
{
    snprintf(w->config.status, sizeof(w->config.status), "%s", message ? message : "");
}

//------------------------------------------------------------------------------------
// Typed field registry
//------------------------------------------------------------------------------------
static int AddField(Field *fields, int count, const char *key, FieldType type,
                    FieldScope scope, void *ptr, double minimum, double maximum, int kit)
{
    if (count >= MAX_FIELDS)
    {
        // A silently dropped registration surfaces later as a baffling
        // "unknown project key" load failure; name the real cause instead.
        fprintf(stderr, "config: field registry overflow, %s dropped (raise MAX_FIELDS)\n",
                key);
        return count;
    }
    Field *field = &fields[count++];
    *field = (Field){ 0 };
    snprintf(field->key, sizeof(field->key), "%s", key);
    field->type = type;
    field->scope = scope;
    field->ptr = ptr;
    field->minimum = minimum;
    field->maximum = maximum;
    field->kit = kit;
    return count;
}

#define ADD_FLOAT(key, scope, member, lo, hi) \
    n = AddField(f, n, key, FIELD_FLOAT, scope, &(member), lo, hi, -1)
#define ADD_INT(key, scope, member, lo, hi) \
    n = AddField(f, n, key, FIELD_INT, scope, &(member), lo, hi, -1)
#define ADD_BOOL(key, scope, member) \
    n = AddField(f, n, key, FIELD_BOOL, scope, &(member), 0, 1, -1)

static int BuildFields(Tuning *t, UiPreferences *preferences,
                       CharacterShowcaseDefinition *showcase,
                       WeaponDef *kits, Field *f)
{
    int n = 0;

    ADD_FLOAT("gameplay.move_speed", SCOPE_PROJECT, t->moveSpeed, 0.5, 60.0);
    ADD_FLOAT("gameplay.move_acceleration", SCOPE_PROJECT, t->moveAccel, 1.0, 120.0);
    ADD_FLOAT("gameplay.dash_speed", SCOPE_PROJECT, t->dashSpeed, 1.0, 100.0);
    ADD_FLOAT("gameplay.bush_reveal_range", SCOPE_PROJECT, t->bushReveal, 0.0, 30.0);
    ADD_FLOAT("gameplay.fire_reveal_duration", SCOPE_PROJECT, t->fireReveal, 0.0, 20.0);
    ADD_FLOAT("gameplay.health_regen_delay", SCOPE_PROJECT,
              t->healthRegenDelay, 0.0, 30.0);
    ADD_FLOAT("gameplay.health_regen_interval", SCOPE_PROJECT,
              t->healthRegenInterval, 0.05, 10.0);
    ADD_FLOAT("gameplay.health_regen_max_ratio", SCOPE_PROJECT,
              t->healthRegenRatio, 0.0, 1.0);
    ADD_FLOAT("gameplay.player_respawn", SCOPE_PROJECT, t->playerRespawn, 0.1, 30.0);
    ADD_FLOAT("gameplay.enemy_respawn", SCOPE_PROJECT, t->enemyRespawn, 0.1, 30.0);
    ADD_FLOAT("gameplay.class_swap_lockout", SCOPE_PROJECT, t->classSwapLockout, 0.0, 30.0);
    ADD_FLOAT("gameplay.result_hold_duration", SCOPE_PROJECT, t->matchResultHold, 0.5, 30.0);
    ADD_FLOAT("gameplay.time_scale", SCOPE_PROJECT, t->timeScale, 0.01, 4.0);
    ADD_FLOAT("gameplay.super_gain_multiplier", SCOPE_PROJECT, t->superMult, 0.0, 10.0);
    ADD_INT("gameplay.crate_health", SCOPE_PROJECT, t->crateHealth, 1, 100000);

    ADD_BOOL("match.gem_grab", SCOPE_PROJECT, t->gemGrab);
    ADD_INT("match.team_size", SCOPE_PROJECT, t->teamSize, 1, MAX_BRAWLERS/2);
    ADD_INT("match.gems_to_win", SCOPE_PROJECT, t->gemsToWin, 1, 100);
    ADD_FLOAT("match.countdown", SCOPE_PROJECT, t->gemCountdown, 1.0, 120.0);
    ADD_FLOAT("match.gem_interval", SCOPE_PROJECT, t->gemVentInterval, 0.5, 120.0);

    ADD_FLOAT("ai.retreat_health_ratio", SCOPE_PROJECT, t->aiRetreatHealth, 0.0, 1.0);
    ADD_FLOAT("ai.support_health_ratio", SCOPE_PROJECT, t->aiSupportHealth, 0.0, 1.0);
    ADD_FLOAT("ai.support_super_health_ratio", SCOPE_PROJECT,
              t->aiSupportSuperHealth, 0.0, 1.0);
    ADD_FLOAT("ai.obstacle_probe_distance", SCOPE_PROJECT, t->aiProbeAhead, 0.1, 10.0);

    ADD_INT("practice.bot_mode", SCOPE_PROJECT, t->botMode, 0, BOT_MODE_COUNT - 1);
    ADD_INT("practice.bot_count", SCOPE_PROJECT, t->botCount, 0, MAX_BRAWLERS - 1);
    ADD_INT("practice.bot_kit", SCOPE_PROJECT, t->botKit, 0, CLASS_COUNT - 1);
    ADD_BOOL("practice.mixed_kits", SCOPE_PROJECT, t->botMixedKits);

    ADD_FLOAT("presentation.grass_height", SCOPE_PROJECT, t->grassHeight, 0.1, 8.0);
    ADD_FLOAT("presentation.wind_strength", SCOPE_PROJECT, t->windStrength, 0.0, 4.0);
    ADD_FLOAT("presentation.wind_speed", SCOPE_PROJECT, t->windSpeed, 0.0, 12.0);
    ADD_FLOAT("presentation.grass_bend_radius", SCOPE_PROJECT, t->grassBendRadius, 0.05, 12.0);
    ADD_FLOAT("presentation.grass_bend_strength", SCOPE_PROJECT, t->grassBendStrength, 0.0, 8.0);
    ADD_FLOAT("presentation.conceal_dither", SCOPE_PROJECT, t->concealDither, 0.0, 0.95);
    ADD_BOOL("presentation.rigged_characters", SCOPE_PROJECT, t->modelCharacter);
    ADD_BOOL("presentation.toon", SCOPE_PROJECT, t->toon);
    ADD_FLOAT("presentation.toon_bands", SCOPE_PROJECT, t->toonBands, 2.0, 5.0);
    ADD_FLOAT("presentation.toon_outline", SCOPE_PROJECT, t->toonOutline, 0.0, 1.0);
    ADD_BOOL("presentation.post_effects", SCOPE_PROJECT, t->postFx);
    ADD_FLOAT("presentation.bloom", SCOPE_PROJECT, t->bloom, 0.0, 3.0);
    ADD_FLOAT("presentation.pixelate", SCOPE_PROJECT, t->stylePixelate, 0.0, 1.0);
    ADD_FLOAT("presentation.painterly", SCOPE_PROJECT, t->stylePainterly, 0.0, 1.0);
    ADD_FLOAT("presentation.halftone", SCOPE_PROJECT, t->styleHalftone, 0.0, 1.0);
    ADD_FLOAT("presentation.posterize", SCOPE_PROJECT, t->stylePosterize, 0.0, 1.0);
    ADD_FLOAT("presentation.grain", SCOPE_PROJECT, t->styleGrain, 0.0, 1.0);
    ADD_FLOAT("presentation.chromatic_aberration", SCOPE_PROJECT, t->styleCA, 0.0, 1.0);
    ADD_FLOAT("presentation.saturation", SCOPE_PROJECT, t->styleSaturation, 0.0, 2.0);
    ADD_FLOAT("presentation.brightness", SCOPE_PROJECT, t->styleBrightness, 0.6, 1.5);
    ADD_FLOAT("presentation.vignette", SCOPE_PROJECT, t->styleVignette, 0.0, 1.5);

    ADD_FLOAT("preview.showcase.yaw_degrees", SCOPE_PROJECT,
              showcase->yawDegrees, -360.0, 360.0);
    ADD_FLOAT("preview.showcase.scale", SCOPE_PROJECT,
              showcase->scale, 0.50, 1.50);
    ADD_FLOAT("preview.showcase.offset_x", SCOPE_PROJECT,
              showcase->offset.x, -8.0, 8.0);
    ADD_FLOAT("preview.showcase.offset_y", SCOPE_PROJECT,
              showcase->offset.y, -4.0, 4.0);
    ADD_FLOAT("preview.showcase.camera_position_x", SCOPE_PROJECT,
              showcase->cameraPosition.x, -20.0, 20.0);
    ADD_FLOAT("preview.showcase.camera_position_y", SCOPE_PROJECT,
              showcase->cameraPosition.y, 0.20, 12.0);
    ADD_FLOAT("preview.showcase.camera_position_z", SCOPE_PROJECT,
              showcase->cameraPosition.z, -20.0, -2.0);
    ADD_FLOAT("preview.showcase.camera_target_x", SCOPE_PROJECT,
              showcase->cameraTarget.x, -8.0, 8.0);
    ADD_FLOAT("preview.showcase.camera_target_y", SCOPE_PROJECT,
              showcase->cameraTarget.y, 0.20, 4.0);
    ADD_FLOAT("preview.showcase.camera_target_z", SCOPE_PROJECT,
              showcase->cameraTarget.z, -8.0, 8.0);
    ADD_FLOAT("preview.showcase.vertical_fov", SCOPE_PROJECT,
              showcase->verticalFov, 20.0, 80.0);

    ADD_BOOL("local.god_mode", SCOPE_LOCAL, t->godMode);
    ADD_BOOL("local.infinite_ammo", SCOPE_LOCAL, t->infiniteAmmo);
    ADD_BOOL("local.show_debug", SCOPE_LOCAL, t->showDebug);

    ADD_INT("profile.selected_kit", SCOPE_PROFILE, t->selectedKit, 0, CLASS_COUNT - 1);
    ADD_INT("profile.wins", SCOPE_PROFILE, t->statWins, 0, 1000000000);
    ADD_INT("profile.losses", SCOPE_PROFILE, t->statLosses, 0, 1000000000);
    ADD_INT("profile.kos", SCOPE_PROFILE, t->statKos, 0, 1000000000);
    ADD_FLOAT("profile.ui_scale", SCOPE_PROFILE, preferences->scale, 0.75, 1.50);
    ADD_BOOL("profile.reduced_motion", SCOPE_PROFILE, preferences->reducedMotion);
    ADD_BOOL("profile.high_contrast", SCOPE_PROFILE, preferences->highContrast);
    ADD_BOOL("profile.tutorial_hints", SCOPE_PROFILE, preferences->showTutorialHints);
    ADD_INT("profile.input_glyph_mode", SCOPE_PROFILE,
            preferences->inputGlyphMode, 0, 2);
    ADD_INT("profile.tutorial_flags", SCOPE_PROFILE,
            preferences->tutorialFlags, 0, 0x7fffffff);

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        WeaponDef *k = &kits[i];
        char key[96];

        #define KIT_KEY(suffix) \
            snprintf(key, sizeof(key), "kit.%s.%s", KIT_KEYS[i], suffix)
        #define KIT_FIELD(suffix, type, member, lo, hi) do { \
            KIT_KEY(suffix); \
            n = AddField(f, n, key, type, SCOPE_PROJECT, &k->member, lo, hi, i); \
        } while (0)

        KIT_FIELD("max_health", FIELD_INT, maxHealth, 1, 100000);
        KIT_FIELD("max_ammo", FIELD_INT, maxAmmo, 1, 12);
        KIT_FIELD("main.kind", FIELD_ATTACK_KIND, mainKind, 0, ATTACK_KIND_COUNT - 1);
        KIT_FIELD("main.pellets", FIELD_INT, pellets, 0, 64);
        KIT_FIELD("main.damage", FIELD_INT, damage, 0, 100000);
        KIT_FIELD("main.healing", FIELD_INT, healing, 0, 100000);
        KIT_FIELD("main.spread_degrees", FIELD_FLOAT, spreadDeg, 0.0, 360.0);
        KIT_FIELD("main.speed", FIELD_FLOAT, speed, 0.0, 200.0);
        KIT_FIELD("main.range", FIELD_FLOAT, range, 0.1, 100.0);
        KIT_FIELD("main.radius", FIELD_FLOAT, projRadius, 0.0, 30.0);
        KIT_FIELD("main.cooldown", FIELD_FLOAT, cooldown, 0.02, 20.0);
        KIT_FIELD("main.reload_per_ammo", FIELD_FLOAT, reloadPerAmmo, 0.05, 30.0);
        KIT_FIELD("main.super_gain_per_hit", FIELD_FLOAT, superPerHit, 0.0, 1.0);
        KIT_FIELD("main.self_heal_ratio", FIELD_FLOAT, selfHealRatio, 0.0, 1.0);
        KIT_FIELD("main.range_scaled", FIELD_BOOL, rangeScaled, 0, 1);
        KIT_FIELD("main.duration", FIELD_FLOAT, duration, 0.0, 30.0);
        KIT_FIELD("main.tick_interval", FIELD_FLOAT, tickRate, 0.0, 10.0);
        KIT_FIELD("main.grow_time", FIELD_FLOAT, growTime, 0.0, 30.0);
        KIT_FIELD("main.return_speed", FIELD_FLOAT, returnSpeed, 0.0, 300.0);

        KIT_FIELD("secondary.kind", FIELD_SECONDARY_KIND,
                  secondaryKind, 0, SECONDARY_KIND_COUNT - 1);
        KIT_FIELD("secondary.cooldown", FIELD_FLOAT, mobilityCooldown, 0.0, 20.0);
        KIT_FIELD("secondary.duration", FIELD_FLOAT, mobilityDuration, 0.0, 5.0);
        KIT_FIELD("secondary.speed", FIELD_FLOAT, mobilitySpeed, 0.0, 100.0);
        KIT_FIELD("secondary.capacity", FIELD_INT, secondaryCapacity, 0, 100000);
        KIT_FIELD("secondary.move_multiplier", FIELD_FLOAT,
                  secondaryMoveMultiplier, 0.0, 1.0);
        KIT_FIELD("secondary.heal_ratio", FIELD_FLOAT,
                  secondaryHealRatio, 0.0, 1.0);
        KIT_FIELD("secondary.recharge_delay", FIELD_FLOAT,
                  secondaryRechargeDelay, 0.0, 30.0);
        KIT_FIELD("secondary.recharge_rate", FIELD_FLOAT,
                  secondaryRechargeRate, 0.0, 100000.0);
        KIT_FIELD("secondary.break_lockout", FIELD_FLOAT,
                  secondaryBreakLockout, 0.0, 30.0);

        KIT_FIELD("super.kind", FIELD_SUPER_KIND, superKind, 0, SUPER_KIND_COUNT - 1);
        KIT_FIELD("super.pellets", FIELD_INT, sPellets, 0, 64);
        KIT_FIELD("super.damage", FIELD_INT, sDamage, 0, 100000);
        KIT_FIELD("super.healing", FIELD_INT, sHealing, 0, 100000);
        KIT_FIELD("super.spread_degrees", FIELD_FLOAT, sSpreadDeg, 0.0, 360.0);
        KIT_FIELD("super.speed", FIELD_FLOAT, sSpeed, 0.0, 300.0);
        KIT_FIELD("super.range", FIELD_FLOAT, sRange, 0.0, 150.0);
        KIT_FIELD("super.radius", FIELD_FLOAT, sProjRadius, 0.0, 30.0);
        KIT_FIELD("super.piercing", FIELD_BOOL, sPiercing, 0, 1);
        KIT_FIELD("super.duration", FIELD_FLOAT, sDuration, 0.0, 60.0);
        KIT_FIELD("super.tick_interval", FIELD_FLOAT, sTickRate, 0.0, 10.0);
        KIT_FIELD("super.visual_duration", FIELD_FLOAT, sVisualDuration, 0.0, 20.0);
        KIT_FIELD("super.return_speed", FIELD_FLOAT, sReturnSpeed, 0.0, 300.0);
        KIT_FIELD("super.outbound_pull", FIELD_FLOAT, sOutboundPull, 0.0, 30.0);
        KIT_FIELD("super.return_knockback", FIELD_FLOAT,
                  sReturnKnockback, 0.0, 30.0);

        #undef KIT_FIELD
        #undef KIT_KEY
    }

    return n;
}

#undef ADD_FLOAT
#undef ADD_INT
#undef ADD_BOOL

//------------------------------------------------------------------------------------
// Parsing and validation
//------------------------------------------------------------------------------------
static Field *FindField(Field *fields, int count, const char *key)
{
    for (int i = 0; i < count; i++)
        if (strcmp(fields[i].key, key) == 0) return &fields[i];
    return NULL;
}

static bool ParseNumber(const char *text, double *out)
{
    errno = 0;
    char *end = NULL;
    double value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value)) return false;
    *out = value;
    return true;
}

static int NameIndex(const char *value, const char **names, int count)
{
    for (int i = 0; i < count; i++)
        if (strcmp(value, names[i]) == 0) return i;
    return -1;
}

static bool AssignField(Field *field, const char *value, char *message, int messageSize)
{
    double number = 0.0;
    int enumValue = -1;

    if (field->type == FIELD_ATTACK_KIND)
        enumValue = NameIndex(value, ATTACK_KIND_NAMES, ATTACK_KIND_COUNT);
    else if (field->type == FIELD_SUPER_KIND)
        enumValue = NameIndex(value, SUPER_KIND_NAMES, SUPER_KIND_COUNT);
    else if (field->type == FIELD_SECONDARY_KIND)
        enumValue = NameIndex(value, SECONDARY_KIND_NAMES, SECONDARY_KIND_COUNT);
    else if (field->type == FIELD_BOOL)
    {
        if (strcmp(value, "true") == 0) number = 1.0;
        else if (strcmp(value, "false") == 0) number = 0.0;
        else if (!ParseNumber(value, &number)) number = -1.0;
    }
    else if (!ParseNumber(value, &number))
    {
        snprintf(message, messageSize, "%s has invalid value '%s'", field->key, value);
        return false;
    }

    if (field->type == FIELD_ATTACK_KIND ||
        field->type == FIELD_SUPER_KIND ||
        field->type == FIELD_SECONDARY_KIND)
    {
        if (enumValue < 0)
        {
            snprintf(message, messageSize, "%s has unknown kind '%s'", field->key, value);
            return false;
        }
        *(int *)field->ptr = enumValue;
        return true;
    }

    if (number < field->minimum || number > field->maximum)
    {
        snprintf(message, messageSize, "%s is outside %.3g..%.3g",
                 field->key, field->minimum, field->maximum);
        return false;
    }

    if (field->type == FIELD_FLOAT) *(float *)field->ptr = (float)number;
    else if (field->type == FIELD_INT)
    {
        double rounded = floor(number + 0.5);
        if (fabs(number - rounded) > 0.000001)
        {
            snprintf(message, messageSize, "%s must be an integer", field->key);
            return false;
        }
        *(int *)field->ptr = (int)rounded;
    }
    else if (field->type == FIELD_BOOL)
    {
        if (number != 0.0 && number != 1.0)
        {
            snprintf(message, messageSize, "%s must be true/false or 0/1", field->key);
            return false;
        }
        *(bool *)field->ptr = number != 0.0;
    }
    return true;
}

static bool ValidateValues(const Tuning *t,
                           const CharacterShowcaseDefinition *showcase,
                           const WeaponDef *kits,
                           char *message, int messageSize)
{
    Field fields[MAX_FIELDS];
    UiPreferences preferences = { .scale = 1.0f };
    int count = BuildFields((Tuning *)t, &preferences,
                            (CharacterShowcaseDefinition *)showcase,
                            (WeaponDef *)kits, fields);
    // Every scope is range-checked: legacy import registers profile/local members
    // with unlimited ranges, so an out-of-range selected_kit or bot_kit would
    // otherwise pass straight through to code that indexes catalogs with it.
    for (int i = 0; i < count; i++)
    {
        double value = 0.0;
        switch (fields[i].type)
        {
            case FIELD_FLOAT: value = *(float *)fields[i].ptr; break;
            case FIELD_INT:
            case FIELD_ATTACK_KIND:
            case FIELD_SUPER_KIND:
            case FIELD_SECONDARY_KIND:
                value = *(int *)fields[i].ptr;
                break;
            case FIELD_BOOL: value = *(bool *)fields[i].ptr ? 1.0 : 0.0; break;
        }
        if (!isfinite(value) || value < fields[i].minimum || value > fields[i].maximum)
        {
            snprintf(message, messageSize, "%s is outside %.3g..%.3g",
                     fields[i].key, fields[i].minimum, fields[i].maximum);
            return false;
        }
    }

    if (!ContentShowcaseValid(showcase))
    {
        snprintf(message, messageSize, "preview.showcase framing is invalid");
        return false;
    }

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        const WeaponDef *k = &kits[i];
        if ((k->mainKind == ATTACK_PROJECTILE || k->mainKind == ATTACK_LOB) &&
            (k->pellets < 1 || k->speed <= 0.0f))
        {
            snprintf(message, messageSize, "kit.%s main projectile needs pellets and speed",
                     KIT_KEYS[i]);
            return false;
        }
        if (k->mainKind == ATTACK_RAIN &&
            (k->duration <= 0.0f || k->tickRate <= 0.0f ||
             k->tickRate > k->duration || k->growTime <= 0.0f ||
             k->growTime > k->duration || k->projRadius <= 0.0f))
        {
            snprintf(message, messageSize, "kit.%s rain timing/radius is invalid", KIT_KEYS[i]);
            return false;
        }
        if (k->mainKind == ATTACK_RETURNING &&
            (k->pellets != 1 || k->speed <= 0.0f ||
             k->returnSpeed <= 0.0f))
        {
            snprintf(message, messageSize,
                     "kit.%s returning main needs one disc and both speeds",
                     KIT_KEYS[i]);
            return false;
        }
        if (k->secondaryKind == SECONDARY_DASH &&
            (k->mobilityCooldown <= 0.0f ||
             k->mobilityDuration <= 0.0f ||
             k->mobilitySpeed <= 0.0f))
        {
            snprintf(message, messageSize,
                     "kit.%s dash secondary is incomplete", KIT_KEYS[i]);
            return false;
        }
        if (k->secondaryKind == SECONDARY_SHIELD &&
            (k->secondaryCapacity <= 0 ||
             k->secondaryMoveMultiplier <= 0.0f ||
             k->secondaryHealRatio < 0.0f ||
             k->secondaryRechargeDelay <= 0.0f ||
             k->secondaryRechargeRate <= 0.0f ||
             k->secondaryBreakLockout <= 0.0f))
        {
            snprintf(message, messageSize,
                     "kit.%s shield secondary is incomplete", KIT_KEYS[i]);
            return false;
        }
        if (k->superKind == SUPER_PROJECTILE &&
            (k->sPellets < 1 || k->sSpeed <= 0.0f || k->sRange <= 0.0f))
        {
            snprintf(message, messageSize, "kit.%s projectile super is incomplete", KIT_KEYS[i]);
            return false;
        }
        if (k->superKind == SUPER_SOUND_WAVE &&
            (k->sDuration <= 0.0f || k->sTickRate <= 0.0f ||
             k->sTickRate > k->sDuration || k->sVisualDuration <= 0.0f ||
             k->sRange <= 0.0f || k->sSpreadDeg <= 0.0f))
        {
            snprintf(message, messageSize, "kit.%s sound-wave timing/cone is invalid", KIT_KEYS[i]);
            return false;
        }
        if (k->superKind == SUPER_RETURNING &&
            (k->sPellets != 1 || k->sSpeed <= 0.0f ||
             k->sReturnSpeed <= 0.0f || k->sRange <= 0.0f ||
             k->sProjRadius <= 0.0f))
        {
            snprintf(message, messageSize,
                     "kit.%s returning super is incomplete", KIT_KEYS[i]);
            return false;
        }
    }
    return true;
}

static int KitIndexForKey(const char *name)
{
    for (int i = 0; i < CLASS_COUNT; i++)
        if (strcmp(name, KIT_KEYS[i]) == 0) return i;
    return -1;
}

static bool AssignV1Preview(CharacterShowcaseDefinition *showcase,
                            const char *key, const char *value,
                            char *message, int messageSize)
{
    char kitName[32], suffix[64], extra;
    if (sscanf(key, "preview.%31[^.].%63s%c",
               kitName, suffix, &extra) != 2)
        return false;
    int kit = KitIndexForKey(kitName);
    if (kit < 0) return false;

    double number = 0.0;
    if (!ParseNumber(value, &number))
    {
        snprintf(message, messageSize, "%s has invalid value '%s'", key, value);
        return false;
    }

    bool yaw = strcmp(suffix, "home_yaw_degrees") == 0 ||
               strcmp(suffix, "select_yaw_degrees") == 0;
    bool scale = strcmp(suffix, "home_scale") == 0 ||
                 strcmp(suffix, "select_scale") == 0;
    bool offsetX = strcmp(suffix, "home_offset_x") == 0 ||
                   strcmp(suffix, "select_offset_x") == 0;
    bool offsetY = strcmp(suffix, "home_offset_y") == 0 ||
                   strcmp(suffix, "select_offset_y") == 0;
    bool target = strcmp(suffix, "camera_target_y") == 0;
    bool distance = strcmp(suffix, "camera_distance") == 0;
    if (!yaw && !scale && !offsetX && !offsetY && !target && !distance)
        return false;

    double minimum = yaw ? -360.0 :
                     scale ? 0.50 :
                     offsetX ? -8.0 :
                     offsetY ? -4.0 :
                     target ? 0.20 : 4.0;
    double maximum = yaw ? 360.0 :
                     scale ? 1.50 :
                     offsetX ? 8.0 :
                     offsetY ? 4.0 :
                     target ? 4.0 : 14.0;
    if (number < minimum || number > maximum)
    {
        snprintf(message, messageSize, "%s is outside %.3g..%.3g",
                 key, minimum, maximum);
        return false;
    }

    // Version 1 had per-character and per-screen framing. The migration deliberately
    // derives the one v2 contract from Scrapper's home profile and discards every
    // select/other-character override.
    if (kit == CLASS_SHOTGUNNER)
    {
        if (strcmp(suffix, "home_yaw_degrees") == 0)
            showcase->yawDegrees = (float)number;
        else if (strcmp(suffix, "home_scale") == 0)
            showcase->scale = (float)number;
        else if (strcmp(suffix, "home_offset_x") == 0)
            showcase->offset.x = (float)number;
        else if (strcmp(suffix, "home_offset_y") == 0)
            showcase->offset.y = (float)number;
        else if (target)
            showcase->cameraTarget.y = (float)number;
        else if (distance)
            showcase->cameraPosition.z = -(float)number;
    }
    return true;
}

static Field *V1MobilityField(Field *fields, int count, const char *key,
                              bool *recognized)
{
    char kitName[32], suffix[32], extra;
    *recognized = false;
    if (sscanf(key, "kit.%31[^.].mobility.%31s%c",
               kitName, suffix, &extra) != 2)
        return NULL;
    int kit = KitIndexForKey(kitName);
    if (kit < 0) return NULL;
    if (strcmp(suffix, "cooldown") != 0 &&
        strcmp(suffix, "duration") != 0 &&
        strcmp(suffix, "speed") != 0)
        return NULL;
    *recognized = true;
    if (kit != CLASS_BRUISER) return NULL;

    char migrated[96];
    snprintf(migrated, sizeof(migrated), "kit.%s.secondary.%s",
             kitName, suffix);
    return FindField(fields, count, migrated);
}

static bool IsV2ObsoleteShieldField(const char *key)
{
    char kitName[32], suffix[64], extra;
    if (sscanf(key, "kit.%31[^.].secondary.%63s%c",
               kitName, suffix, &extra) != 2)
        return false;

    if (strcmp(suffix, "arc_degrees") == 0 ||
        strcmp(suffix, "counter_range") == 0 ||
        strcmp(suffix, "counter_damage_min") == 0 ||
        strcmp(suffix, "counter_damage_max") == 0 ||
        strcmp(suffix, "counter_knockback_min") == 0 ||
        strcmp(suffix, "counter_knockback_max") == 0)
        return true;

    return strcmp(kitName, "scrapper") == 0 &&
           (strcmp(suffix, "cooldown") == 0 ||
            strcmp(suffix, "duration") == 0 ||
            strcmp(suffix, "speed") == 0);
}

// requireAllKeys keeps the explicit validate-config entry point strict about a
// project file that lacks newly added keys, while the runtime load path treats the
// same situation as "use the compiled default and say so" instead of dropping the
// whole checkout to recovery defaults.
static bool LoadTypedFile(const char *path, Tuning *t, UiPreferences *preferences,
                          CharacterShowcaseDefinition *showcase, WeaponDef *kits,
                          FieldScope allowedScope, bool projectFile,
                          bool requireAllKeys, char *message, int messageSize)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        snprintf(message, messageSize, "cannot open %s", path);
        return false;
    }

    int declaredVersion = 0;
    bool declaredSeen = false;
    char probeLine[320];
    int probeLineNumber = 0;
    while (fgets(probeLine, sizeof(probeLine), file))
    {
        probeLineNumber++;
        char key[96], value[128], extra[8];
        int parsed = sscanf(probeLine, " %95s %127s %7s", key, value, extra);
        if (parsed <= 0 || key[0] == '#') continue;
        if (strcmp(key, "format_version") != 0) continue;
        double parsedVersion = 0.0;
        if (declaredSeen || parsed != 2 ||
            !ParseNumber(value, &parsedVersion) ||
            parsedVersion != floor(parsedVersion))
        {
            snprintf(message, messageSize,
                     "%s:%d has invalid or duplicate format_version",
                     path, probeLineNumber);
            fclose(file);
            return false;
        }
        declaredVersion = (int)parsedVersion;
        declaredSeen = true;
    }
    if (!declaredSeen ||
        (declaredVersion != 1 && declaredVersion != 2 &&
         declaredVersion != CONFIG_FORMAT_VERSION))
    {
        snprintf(message, messageSize, "%s uses unsupported format version %d",
                 path, declaredVersion);
        fclose(file);
        return false;
    }
    rewind(file);

    Field fields[MAX_FIELDS];
    int count = BuildFields(t, preferences, showcase, kits, fields);
    WeaponDef scrapperBefore = kits[CLASS_SHOTGUNNER];
    bool migrateV1 = declaredVersion == 1;
    bool migrateV2 = declaredVersion == 2;
    int version = 0;
    bool versionSeen = false;
    char line[320];
    int lineNumber = 0;
    char seenKeys[MAX_FIELDS][96];
    int seenKeyCount = 0;

    while (fgets(line, sizeof(line), file))
    {
        lineNumber++;
        char key[96], value[128], extra[8];
        int parsed = sscanf(line, " %95s %127s %7s", key, value, extra);
        if (parsed <= 0 || key[0] == '#') continue;
        if (parsed < 2)
        {
            snprintf(message, messageSize, "%s:%d is missing a value", path, lineNumber);
            fclose(file);
            return false;
        }
        if (parsed > 2)
        {
            snprintf(message, messageSize, "%s:%d has extra content", path, lineNumber);
            fclose(file);
            return false;
        }
        if (strcmp(key, "format_version") == 0)
        {
            double parsedVersion = 0.0;
            if (versionSeen)
            {
                snprintf(message, messageSize, "%s:%d duplicates format_version",
                         path, lineNumber);
                fclose(file);
                return false;
            }
            if (!ParseNumber(value, &parsedVersion) ||
                parsedVersion != floor(parsedVersion) ||
                parsedVersion < 0.0 || parsedVersion > 1000000.0)
            {
                snprintf(message, messageSize, "%s:%d has invalid format_version",
                         path, lineNumber);
                fclose(file);
                return false;
            }
            version = (int)parsedVersion;
            versionSeen = true;
            continue;
        }

        for (int seen = 0; seen < seenKeyCount; seen++)
        {
            if (strcmp(seenKeys[seen], key) == 0)
            {
                snprintf(message, messageSize, "%s:%d duplicates %s",
                         path, lineNumber, key);
                fclose(file);
                return false;
            }
        }
        if (seenKeyCount < MAX_FIELDS)
            snprintf(seenKeys[seenKeyCount++],
                     sizeof(seenKeys[0]), "%s", key);

        if (migrateV1 && strncmp(key, "preview.", 8) == 0)
        {
            if (!projectFile && allowedScope == SCOPE_PROFILE) continue;
            if (!AssignV1Preview(showcase, key, value,
                                 message, messageSize))
            {
                snprintf(message, messageSize,
                         "%s:%d has unknown v1 preview key %s",
                         path, lineNumber, key);
                fclose(file);
                return false;
            }
            continue;
        }

        if (migrateV2 && IsV2ObsoleteShieldField(key))
        {
            double ignored = 0.0;
            if (!ParseNumber(value, &ignored))
            {
                snprintf(message, messageSize,
                         "%s has invalid value '%s'", key, value);
                fclose(file);
                return false;
            }
            continue;
        }

        Field *field = NULL;
        if (migrateV1)
        {
            bool recognizedMobility = false;
            field = V1MobilityField(fields, count, key, &recognizedMobility);
            if (recognizedMobility)
            {
                if (!field)
                {
                    double ignored = 0.0;
                    if (!ParseNumber(value, &ignored))
                    {
                        snprintf(message, messageSize,
                                 "%s has invalid value '%s'", key, value);
                        fclose(file);
                        return false;
                    }
                    continue;
                }
            }
            else field = FindField(fields, count, key);
        }
        else field = FindField(fields, count, key);
        if (!field || (projectFile ? field->scope != SCOPE_PROJECT
                                  : field->scope == SCOPE_PROFILE && allowedScope != SCOPE_PROFILE))
        {
            if (projectFile)
            {
                snprintf(message, messageSize, "%s:%d has unknown project key %s",
                         path, lineNumber, key);
                fclose(file);
                return false;
            }
            continue;
        }
        if (!projectFile && allowedScope == SCOPE_PROFILE && field->scope != SCOPE_PROFILE)
            continue;
        if (field->seen)
        {
            snprintf(message, messageSize, "%s:%d duplicates %s", path, lineNumber, key);
            fclose(file);
            return false;
        }
        if (migrateV2 && field->type == FIELD_SECONDARY_KIND &&
            strcmp(value, "guard") == 0)
            *(int *)field->ptr = SECONDARY_SHIELD;
        else if (!AssignField(field, value, message, messageSize))
        {
            fclose(file);
            return false;
        }
        field->seen = true;
    }
    fclose(file);

    if (version != declaredVersion)
    {
        snprintf(message, messageSize, "%s changed format version while parsing", path);
        return false;
    }

    if (migrateV1)
        kits[CLASS_SHOTGUNNER] = scrapperBefore;

    if (projectFile && declaredVersion == CONFIG_FORMAT_VERSION)
    {
        int missing = 0;
        const char *firstMissing = NULL;
        for (int i = 0; i < count; i++)
        {
            if (fields[i].scope == SCOPE_PROJECT && !fields[i].seen)
            {
                if (requireAllKeys)
                {
                    snprintf(message, messageSize, "%s is missing required key %s",
                             path, fields[i].key);
                    return false;
                }
                missing++;
                if (!firstMissing) firstMissing = fields[i].key;
            }
        }
        if (missing > 0)
        {
            char notice[160];
            snprintf(notice, sizeof(notice),
                     "%d new project key(s) defaulted (first: %s); re-save to update %s",
                     missing, firstMissing, path);
            if (!ValidateValues(t, showcase, kits, message, messageSize)) return false;
            snprintf(message, messageSize, "%s", notice);
            return true;
        }
    }

    return ValidateValues(t, showcase, kits, message, messageSize);
}

//------------------------------------------------------------------------------------
// Legacy version-1/version-2 tuning import
//------------------------------------------------------------------------------------
static int BuildLegacyFields(Tuning *t, WeaponDef *kits, Field *f)
{
    int n = 0;

    #define LEGACY(key, type, member) \
        n = AddField(f, n, key, type, SCOPE_LOCAL, &(member), -1000000000.0, 1000000000.0, -1)

    LEGACY("move_speed", FIELD_FLOAT, t->moveSpeed);
    LEGACY("move_accel", FIELD_FLOAT, t->moveAccel);
    LEGACY("dash_speed", FIELD_FLOAT, t->dashSpeed);
    LEGACY("bush_reveal", FIELD_FLOAT, t->bushReveal);
    LEGACY("fire_reveal", FIELD_FLOAT, t->fireReveal);
    LEGACY("player_respawn", FIELD_FLOAT, t->playerRespawn);
    LEGACY("enemy_respawn", FIELD_FLOAT, t->enemyRespawn);
    LEGACY("time_scale", FIELD_FLOAT, t->timeScale);
    LEGACY("super_mult", FIELD_FLOAT, t->superMult);
    LEGACY("god_mode", FIELD_BOOL, t->godMode);
    LEGACY("infinite_ammo", FIELD_BOOL, t->infiniteAmmo);
    LEGACY("show_debug", FIELD_BOOL, t->showDebug);
    LEGACY("model_character", FIELD_BOOL, t->modelCharacter);
    LEGACY("toon", FIELD_BOOL, t->toon);
    LEGACY("toon_bands", FIELD_FLOAT, t->toonBands);
    LEGACY("toon_outline", FIELD_FLOAT, t->toonOutline);
    LEGACY("style_pixelate", FIELD_FLOAT, t->stylePixelate);
    LEGACY("style_painterly", FIELD_FLOAT, t->stylePainterly);
    LEGACY("style_halftone", FIELD_FLOAT, t->styleHalftone);
    LEGACY("style_posterize", FIELD_FLOAT, t->stylePosterize);
    LEGACY("style_grain", FIELD_FLOAT, t->styleGrain);
    LEGACY("style_ca", FIELD_FLOAT, t->styleCA);
    LEGACY("style_saturation", FIELD_FLOAT, t->styleSaturation);
    LEGACY("style_brightness", FIELD_FLOAT, t->styleBrightness);
    LEGACY("style_vignette", FIELD_FLOAT, t->styleVignette);
    LEGACY("post_fx", FIELD_BOOL, t->postFx);
    LEGACY("bloom", FIELD_FLOAT, t->bloom);
    LEGACY("selected_kit", FIELD_INT, t->selectedKit);
    LEGACY("stat_wins", FIELD_INT, t->statWins);
    LEGACY("stat_losses", FIELD_INT, t->statLosses);
    LEGACY("stat_kos", FIELD_INT, t->statKos);
    LEGACY("gem_grab", FIELD_BOOL, t->gemGrab);
    LEGACY("team_size", FIELD_INT, t->teamSize);
    LEGACY("gems_to_win", FIELD_INT, t->gemsToWin);
    LEGACY("gem_countdown", FIELD_FLOAT, t->gemCountdown);
    LEGACY("gem_interval", FIELD_FLOAT, t->gemVentInterval);
    LEGACY("grass_height", FIELD_FLOAT, t->grassHeight);
    LEGACY("wind_strength", FIELD_FLOAT, t->windStrength);
    LEGACY("wind_speed", FIELD_FLOAT, t->windSpeed);
    LEGACY("grass_bend_r", FIELD_FLOAT, t->grassBendRadius);
    LEGACY("grass_bend_s", FIELD_FLOAT, t->grassBendStrength);
    LEGACY("conceal_dither", FIELD_FLOAT, t->concealDither);
    LEGACY("bot_mode", FIELD_INT, t->botMode);
    LEGACY("bot_count", FIELD_INT, t->botCount);
    LEGACY("bot_kit", FIELD_INT, t->botKit);
    LEGACY("bot_mixed", FIELD_BOOL, t->botMixedKits);

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        char key[48];
        WeaponDef *k = &kits[i];

        #define LEGACY_KIT(name, type, member) do { \
            snprintf(key, sizeof(key), "kit%d_%s", i, name); \
            n = AddField(f, n, key, type, SCOPE_LOCAL, &k->member, \
                         -1000000000.0, 1000000000.0, i); \
        } while (0)

        LEGACY_KIT("health", FIELD_INT, maxHealth);
        LEGACY_KIT("pellets", FIELD_INT, pellets);
        LEGACY_KIT("damage", FIELD_INT, damage);
        LEGACY_KIT("healing", FIELD_INT, healing);
        LEGACY_KIT("spread", FIELD_FLOAT, spreadDeg);
        LEGACY_KIT("speed", FIELD_FLOAT, speed);
        LEGACY_KIT("range", FIELD_FLOAT, range);
        LEGACY_KIT("projradius", FIELD_FLOAT, projRadius);
        LEGACY_KIT("cooldown", FIELD_FLOAT, cooldown);
        LEGACY_KIT("reload", FIELD_FLOAT, reloadPerAmmo);
        LEGACY_KIT("superperhit", FIELD_FLOAT, superPerHit);
        LEGACY_KIT("sdamage", FIELD_INT, sDamage);
        LEGACY_KIT("shealing", FIELD_INT, sHealing);
        LEGACY_KIT("spellets", FIELD_INT, sPellets);
        LEGACY_KIT("sspread", FIELD_FLOAT, sSpreadDeg);
        LEGACY_KIT("srange", FIELD_FLOAT, sRange);

        #undef LEGACY_KIT
    }

    #undef LEGACY
    return n;
}

static bool LoadLegacyFile(const char *path, Tuning *t,
                           CharacterShowcaseDefinition *showcase,
                           WeaponDef *kits,
                           const WeaponDef *projectKits, char *message, int messageSize)
{
    FILE *file = fopen(path, "r");
    if (!file) return false;

    Field fields[MAX_FIELDS];
    int count = BuildLegacyFields(t, kits, fields);
    int version = 0;
    char line[256];

    while (fgets(line, sizeof(line), file))
    {
        char key[96], value[96];
        if (sscanf(line, " %95s %95s", key, value) != 2 || key[0] == '#') continue;
        if (strcmp(key, "version") == 0) { version = atoi(value); continue; }

        Field *field = FindField(fields, count, key);
        if (!field) continue;
        if (!AssignField(field, value, message, messageSize))
        {
            fclose(file);
            return false;
        }
    }
    fclose(file);

    if (version < 1 || version > 2)
    {
        snprintf(message, messageSize, "%s has unsupported legacy version %d", path, version);
        return false;
    }

    // Version 1 predates rain/Resonance. Its Guardian numbers describe a different kit.
    if (version < 2) kits[CLASS_HEALER] = projectKits[CLASS_HEALER];
    return ValidateValues(t, showcase, kits, message, messageSize);
}

//------------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------------
static bool SameFieldValue(const Field *a, const Field *b)
{
    if (a->type != b->type) return false;
    switch (a->type)
    {
        case FIELD_FLOAT:
            return fabsf(*(float *)a->ptr - *(float *)b->ptr) < 0.00005f;
        case FIELD_INT:
        case FIELD_ATTACK_KIND:
        case FIELD_SUPER_KIND:
        case FIELD_SECONDARY_KIND:
            return *(int *)a->ptr == *(int *)b->ptr;
        case FIELD_BOOL:
            return *(bool *)a->ptr == *(bool *)b->ptr;
    }
    return false;
}

static void CopyFieldValue(Field *destination, const Field *source)
{
    switch (destination->type)
    {
        case FIELD_FLOAT: *(float *)destination->ptr = *(float *)source->ptr; break;
        case FIELD_INT:
        case FIELD_ATTACK_KIND:
        case FIELD_SUPER_KIND:
        case FIELD_SECONDARY_KIND:
            *(int *)destination->ptr = *(int *)source->ptr;
            break;
        case FIELD_BOOL: *(bool *)destination->ptr = *(bool *)source->ptr; break;
    }
}

static void WriteField(FILE *file, const Field *field)
{
    switch (field->type)
    {
        case FIELD_FLOAT:
            fprintf(file, "%s %.6f\n", field->key, *(float *)field->ptr);
            break;
        case FIELD_INT:
            fprintf(file, "%s %d\n", field->key, *(int *)field->ptr);
            break;
        case FIELD_BOOL:
            fprintf(file, "%s %s\n", field->key, *(bool *)field->ptr ? "true" : "false");
            break;
        case FIELD_ATTACK_KIND:
            fprintf(file, "%s %s\n", field->key,
                    ATTACK_KIND_NAMES[*(int *)field->ptr]);
            break;
        case FIELD_SUPER_KIND:
            fprintf(file, "%s %s\n", field->key,
                    SUPER_KIND_NAMES[*(int *)field->ptr]);
            break;
        case FIELD_SECONDARY_KIND:
            fprintf(file, "%s %s\n", field->key,
                    SECONDARY_KIND_NAMES[*(int *)field->ptr]);
            break;
    }
}

static bool FinishAtomicFile(FILE *file, const char *temporaryPath, const char *path)
{
    int flushResult = fflush(file);
    int closeResult = fclose(file);
    if (flushResult != 0 || closeResult != 0)
    {
        remove(temporaryPath);
        return false;
    }
    if (rename(temporaryPath, path) != 0)
    {
        remove(temporaryPath);
        return false;
    }
    return true;
}

static FILE *OpenAtomicFile(const char *path, char *temporaryPath, int temporarySize)
{
    snprintf(temporaryPath, temporarySize, "%s.tmp", path);
    return fopen(temporaryPath, "w");
}

static bool SaveProjectSnapshot(App *w)
{
    char validation[160];
    if (!ValidateValues(&w->config.projectTuning, &w->config.projectShowcase,
                        w->config.projectWeapons,
                        validation, sizeof(validation)))
    {
        SetStatus(w, validation);
        return false;
    }

    char temporary[512];
    FILE *file = OpenAtomicFile(ProjectPath(), temporary, sizeof(temporary));
    if (!file)
    {
        SetStatus(w, "Could not open the project configuration for writing.");
        return false;
    }

    fprintf(file, "# Brawl Arena canonical project configuration.\n");
    fprintf(file, "# Written only by an explicit SAVE ... AS PROJECT DEFAULT action.\n");
    fprintf(file, "format_version %d\n\n", CONFIG_FORMAT_VERSION);

    Field fields[MAX_FIELDS];
    int count = BuildFields(&w->config.projectTuning, &w->uiPreferences,
                            &w->config.projectShowcase,
                            w->config.projectWeapons, fields);
    for (int i = 0; i < count; i++)
        if (fields[i].scope == SCOPE_PROJECT) WriteField(file, &fields[i]);

    if (!FinishAtomicFile(file, temporary, ProjectPath()))
    {
        SetStatus(w, "Project configuration write failed.");
        return false;
    }
    w->config.projectLoaded = true;
    w->config.recoveryDefaults = false;
    return true;
}

static bool SaveLocalDraft(App *w)
{
    char temporary[512];
    FILE *file = OpenAtomicFile(LocalPath(), temporary, sizeof(temporary));
    if (!file) return false;

    fprintf(file, "# Local Brawl Arena authoring draft. Ignored by Git.\n");
    fprintf(file, "# Project-scoped keys appear only while they differ from config/gameplay.cfg.\n");
    fprintf(file, "format_version %d\n", CONFIG_FORMAT_VERSION);

    Field live[MAX_FIELDS], project[MAX_FIELDS];
    int liveCount = BuildFields(&w->tune, &w->uiPreferences,
                                &w->content.showcase, w->content.weapons, live);
    int projectCount = BuildFields(&w->config.projectTuning, &w->uiPreferences,
                                   &w->config.projectShowcase,
                                   w->config.projectWeapons, project);

    for (int i = 0; i < liveCount && i < projectCount; i++)
    {
        if (live[i].scope == SCOPE_LOCAL ||
            (live[i].scope == SCOPE_PROJECT && !SameFieldValue(&live[i], &project[i])))
            WriteField(file, &live[i]);
    }

    return FinishAtomicFile(file, temporary, LocalPath());
}

static bool SaveProfile(App *w)
{
    char temporary[512];
    FILE *file = OpenAtomicFile(ProfilePath(), temporary, sizeof(temporary));
    if (!file) return false;

    fprintf(file, "# Local Brawl Arena profile. Ignored by Git.\n");
    fprintf(file, "format_version %d\n", CONFIG_FORMAT_VERSION);

    Field fields[MAX_FIELDS];
    int count = BuildFields(&w->tune, &w->uiPreferences,
                            &w->content.showcase, w->content.weapons, fields);
    for (int i = 0; i < count; i++)
        if (fields[i].scope == SCOPE_PROFILE) WriteField(file, &fields[i]);

    return FinishAtomicFile(file, temporary, ProfilePath());
}

//------------------------------------------------------------------------------------
// Public lifecycle
//------------------------------------------------------------------------------------
bool ConfigInitialize(App *w)
{
    g_dirty = false;
    g_saveTimer = 0.0f;

    TuningSetDefaults(&w->tune);
    w->uiPreferences = (UiPreferences){
        .scale = 1.0f,
        .reducedMotion = false,
        .highContrast = false,
        .showTutorialHints = true,
        .inputGlyphMode = 0,
        .tutorialFlags = 0
    };
    ContentCatalogResetAll(&w->content);
    memset(&w->config, 0, sizeof(w->config));

    Tuning candidateTuning = w->tune;
    CharacterShowcaseDefinition candidateShowcase = w->content.showcase;
    WeaponDef candidateWeapons[CLASS_COUNT];
    memcpy(candidateWeapons, w->content.weapons, sizeof(candidateWeapons));

    char message[160];
    message[0] = '\0';
    char projectNotice[160];
    projectNotice[0] = '\0';
    UiPreferences candidatePreferences = w->uiPreferences;
    bool projectOk = LoadTypedFile(ProjectPath(), &candidateTuning, &candidatePreferences,
                                   &candidateShowcase, candidateWeapons,
                                   SCOPE_PROJECT, true, false, message, sizeof(message));
    if (projectOk && message[0])
        snprintf(projectNotice, sizeof(projectNotice), "%s", message);
    if (!projectOk)
    {
        w->config.projectTuning = w->tune;
        memcpy(w->config.projectWeapons, w->content.weapons,
               sizeof(w->config.projectWeapons));
        w->config.projectShowcase = w->content.showcase;
        w->config.projectLoaded = false;
        w->config.recoveryDefaults = true;
        ContentCatalogRebuildTyped(&w->content);
        SetStatus(w, message);
        return false;
    }

    w->tune = candidateTuning;
    w->content.showcase = candidateShowcase;
    memcpy(w->content.weapons, candidateWeapons, sizeof(candidateWeapons));
    w->config.projectTuning = candidateTuning;
    w->config.projectShowcase = candidateShowcase;
    memcpy(w->config.projectWeapons, candidateWeapons, sizeof(candidateWeapons));
    w->config.projectLoaded = true;

    bool localLoaded = false;
    bool overlayError = false;
    if (PathExists(LocalPath()))
    {
        Tuning localTuning = w->tune;
        CharacterShowcaseDefinition localShowcase = w->content.showcase;
        WeaponDef localWeapons[CLASS_COUNT];
        memcpy(localWeapons, w->content.weapons, sizeof(localWeapons));
        UiPreferences localPreferences = w->uiPreferences;
        if (LoadTypedFile(LocalPath(), &localTuning, &localPreferences,
                          &localShowcase, localWeapons,
                          SCOPE_LOCAL, false, false, message, sizeof(message)))
        {
            w->tune = localTuning;
            w->content.showcase = localShowcase;
            memcpy(w->content.weapons, localWeapons, sizeof(localWeapons));
            localLoaded = true;
        }
        else { SetStatus(w, message); overlayError = true; }
    }
    else
    {
        const char *legacyPath = LegacyPath();
        if (PathExists(legacyPath))
        {
            Tuning legacyTuning = w->tune;
            WeaponDef legacyWeapons[CLASS_COUNT];
            memcpy(legacyWeapons, w->content.weapons, sizeof(legacyWeapons));
            if (LoadLegacyFile(legacyPath, &legacyTuning, &w->content.showcase,
                               legacyWeapons,
                               w->config.projectWeapons, message, sizeof(message)))
            {
                w->tune = legacyTuning;
                memcpy(w->content.weapons, legacyWeapons, sizeof(legacyWeapons));
                w->config.legacyImported = true;
                localLoaded = true;
                SaveLocalDraft(w);
                SaveProfile(w);

                // The default legacy file is retired after a successful one-time
                // import so it can never silently shadow tracked project truth
                // again. Explicit BRAWL_LEGACY_TUNING paths (tests, tooling) are
                // left untouched.
                const char *explicitLegacy = getenv("BRAWL_LEGACY_TUNING");
                if (!explicitLegacy || !explicitLegacy[0])
                {
                    char retired[512];
                    snprintf(retired, sizeof(retired), "%s.imported", legacyPath);
                    rename(legacyPath, retired);
                }
            }
            else { SetStatus(w, message); overlayError = true; }
        }
    }

    if (PathExists(ProfilePath()))
    {
        Tuning profileTuning = w->tune;
        CharacterShowcaseDefinition unchangedShowcase = w->content.showcase;
        WeaponDef unchangedWeapons[CLASS_COUNT];
        memcpy(unchangedWeapons, w->content.weapons, sizeof(unchangedWeapons));
        UiPreferences profilePreferences = w->uiPreferences;
        if (LoadTypedFile(ProfilePath(), &profileTuning, &profilePreferences,
                          &unchangedShowcase, unchangedWeapons,
                          SCOPE_PROFILE, false, false, message, sizeof(message)))
        {
            w->tune = profileTuning;
            w->uiPreferences = profilePreferences;
        }
        else { SetStatus(w, message); overlayError = true; }
    }

    if (overlayError)
    {
        // Keep the parser's actionable error message instead of hiding it behind a
        // generic "loaded" status. The rejected overlay never touched live values.
    }
    else if (w->config.legacyImported)
        SetStatus(w, "Imported legacy tuning into the local draft; original retired as .imported.");
    else if (projectNotice[0])
        SetStatus(w, projectNotice);
    else if (localLoaded && ConfigProjectOverrideCount(w) > 0)
        SetStatus(w, "Project defaults loaded with local draft overrides.");
    else
        SetStatus(w, "Canonical project defaults loaded.");

    ContentCatalogRebuildTyped(&w->content);
    return true;
}

void ConfigMarkDirty(void)
{
    g_dirty = true;
    g_saveTimer = 0.0f;
}

void ConfigFlush(App *w)
{
    if (!g_dirty) return;

    bool localOk = SaveLocalDraft(w);
    bool profileOk = SaveProfile(w);
    if (localOk && profileOk)
    {
        char status[160];
        snprintf(status, sizeof(status), "Local draft saved (%d project overrides).",
                 ConfigProjectOverrideCount(w));
        SetStatus(w, status);
        g_dirty = false;
        g_saveTimer = 0.0f;
    }
    else SetStatus(w, "Local draft/profile save failed.");
}

void ConfigAutoSave(App *w, float realDt)
{
    if (!g_dirty) return;
    g_saveTimer += realDt;
    if (g_saveTimer >= SAVE_DELAY) ConfigFlush(w);
}

bool ConfigPromoteAll(App *w)
{
    Tuning oldTuning = w->config.projectTuning;
    CharacterShowcaseDefinition oldShowcase = w->config.projectShowcase;
    WeaponDef oldWeapons[CLASS_COUNT];
    memcpy(oldWeapons, w->config.projectWeapons, sizeof(oldWeapons));

    Field live[MAX_FIELDS], project[MAX_FIELDS];
    int liveCount = BuildFields(&w->tune, &w->uiPreferences,
                                &w->content.showcase, w->content.weapons, live);
    int projectCount = BuildFields(&w->config.projectTuning, &w->uiPreferences,
                                   &w->config.projectShowcase,
                                   w->config.projectWeapons, project);
    for (int i = 0; i < liveCount && i < projectCount; i++)
        if (live[i].scope == SCOPE_PROJECT) CopyFieldValue(&project[i], &live[i]);

    if (!SaveProjectSnapshot(w))
    {
        w->config.projectTuning = oldTuning;
        w->config.projectShowcase = oldShowcase;
        memcpy(w->config.projectWeapons, oldWeapons, sizeof(oldWeapons));
        return false;
    }

    bool localOk = SaveLocalDraft(w);
    bool profileOk = SaveProfile(w);
    if (!localOk || !profileOk)
    {
        g_dirty = true;
        SetStatus(w, "PROJECT saved, but local draft/profile cleanup must be retried.");
        return true;
    }
    g_dirty = false;
    SetStatus(w, "All live gameplay values saved as PROJECT DEFAULTS.");
    return true;
}

bool ConfigPromoteKit(App *w, BrawlerClass cls)
{
    if (cls < 0 || cls >= CLASS_COUNT) return false;
    WeaponDef old = w->config.projectWeapons[cls];
    CharacterShowcaseDefinition oldShowcase = w->config.projectShowcase;
    w->config.projectWeapons[cls] = w->content.weapons[cls];
    w->config.projectShowcase = w->content.showcase;

    if (!SaveProjectSnapshot(w))
    {
        w->config.projectWeapons[cls] = old;
        w->config.projectShowcase = oldShowcase;
        return false;
    }

    bool localOk = SaveLocalDraft(w);
    bool profileOk = SaveProfile(w);
    if (!localOk || !profileOk)
    {
        g_dirty = true;
        SetStatus(w, "PROJECT kit saved, but local draft/profile cleanup must be retried.");
        return true;
    }
    g_dirty = false;

    char status[160];
    snprintf(status, sizeof(status), "%s saved as a PROJECT DEFAULT.", CLASS_NAMES[cls]);
    SetStatus(w, status);
    return true;
}

void ConfigResetAllToProject(App *w)
{
    Field live[MAX_FIELDS], project[MAX_FIELDS];
    int liveCount = BuildFields(&w->tune, &w->uiPreferences,
                                &w->content.showcase, w->content.weapons, live);
    int projectCount = BuildFields(&w->config.projectTuning, &w->uiPreferences,
                                   &w->config.projectShowcase,
                                   w->config.projectWeapons, project);
    for (int i = 0; i < liveCount && i < projectCount; i++)
        if (live[i].scope == SCOPE_PROJECT) CopyFieldValue(&live[i], &project[i]);

    ContentCatalogRebuildTyped(&w->content);
    ConfigMarkDirty();
    SetStatus(w, "Local gameplay draft discarded; project defaults restored.");
}

void ConfigResetKitToProject(App *w, BrawlerClass cls)
{
    if (cls < 0 || cls >= CLASS_COUNT) return;
    w->content.weapons[cls] = w->config.projectWeapons[cls];
    w->content.showcase = w->config.projectShowcase;
    ContentCatalogRebuildTyped(&w->content);
    ConfigMarkDirty();

    char status[160];
    snprintf(status, sizeof(status), "%s reset to its PROJECT DEFAULT.", CLASS_NAMES[cls]);
    SetStatus(w, status);
}

int ConfigProjectOverrideCount(App *w)
{
    Field live[MAX_FIELDS], project[MAX_FIELDS];
    int liveCount = BuildFields(&w->tune, &w->uiPreferences,
                                &w->content.showcase, w->content.weapons, live);
    int projectCount = BuildFields(&w->config.projectTuning, &w->uiPreferences,
                                   &w->config.projectShowcase,
                                   w->config.projectWeapons, project);
    int different = 0;
    for (int i = 0; i < liveCount && i < projectCount; i++)
        if (live[i].scope == SCOPE_PROJECT && !SameFieldValue(&live[i], &project[i]))
            different++;
    return different;
}

int ConfigKitOverrideCount(App *w, BrawlerClass cls)
{
    Field live[MAX_FIELDS], project[MAX_FIELDS];
    int liveCount = BuildFields(&w->tune, &w->uiPreferences,
                                &w->content.showcase, w->content.weapons, live);
    int projectCount = BuildFields(&w->config.projectTuning, &w->uiPreferences,
                                   &w->config.projectShowcase,
                                   w->config.projectWeapons, project);
    int different = 0;
    for (int i = 0; i < liveCount && i < projectCount; i++)
        if (live[i].scope == SCOPE_PROJECT &&
            (live[i].kit == (int)cls ||
             strncmp(live[i].key, "preview.showcase.", 17) == 0) &&
            !SameFieldValue(&live[i], &project[i]))
            different++;
    return different;
}

const char *ConfigStatus(const App *w)
{
    return w->config.status;
}

bool ConfigValidateProjectFile(const char *path, char *message, int messageSize)
{
    Tuning tuning;
    TuningSetDefaults(&tuning);
    ContentCatalog catalog = { 0 };
    ContentCatalogResetAll(&catalog);
    UiPreferences preferences = { .scale = 1.0f };
    return LoadTypedFile(path, &tuning, &preferences,
                         &catalog.showcase, catalog.weapons,
                         SCOPE_PROJECT, true, true, message, messageSize);
}
