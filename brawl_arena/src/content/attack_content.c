#include "attack_content.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ANCHOR_NAMES[ATTACK_ANCHOR_COUNT] = {
    "cast", "self", "projectile", "impact",
    "field_start", "field_pulse", "field_end", "mark_applied", "mark_tick"
};
static const char *PATTERN_NAMES[ATTACK_PATTERN_COUNT] = {
    "single", "burst", "ring", "cone"
};
static const char *BLEND_NAMES[ATTACK_BLEND_COUNT] = { "alpha", "additive" };
static const char *SHAPE_NAMES[ATTACK_SHAPE_COUNT] = {
    "sprite", "shield", "orb", "disc", "dome", "column"
};
static const char *MOTION_NAMES[ATTACK_MOTION_COUNT] = {
    "none", "recoil", "raise_right", "raise_left", "swing_right",
    "twist", "slam", "lean", "raise_both", "conduct"
};

static int NameIndex(const char *value, const char **names, int count)
{
    for (int i = 0; i < count; i++)
        if (strcmp(value, names[i]) == 0) return i;
    return -1;
}

void AttackContentDefaults(ContentCatalog *catalog)
{
    memset(catalog->attacks, 0, sizeof(catalog->attacks));
}

bool AttackAuthored(const ContentCatalog *catalog, int abilityIndex)
{
    if (abilityIndex < 0 || abilityIndex >= catalog->abilityCount) return false;
    return catalog->attacks[abilityIndex].authored;
}

static AttackEffectLayer DefaultLayer(void)
{
    return (AttackEffectLayer){
        .used = true,
        .anchor = ATTACK_ANCHOR_CAST,
        .atlas = 0,
        .frame = 0,
        .frameCount = 1,
        .fps = 24.0f,
        .pattern = ATTACK_PATTERN_SINGLE,
        .count = 1,
        .delay = 0.0f,
        .duration = 0.35f,
        .forward = 0.6f,
        .up = 0.8f,
        .side = 0.0f,
        .spreadDeg = 30.0f,
        .speed = 0.0f,
        .gravity = 0.0f,
        .drag = 0.0f,
        .scaleStart = 1.0f,
        .scaleEnd = 1.4f,
        .colorStart = (Color){ 255, 255, 255, 255 },
        .colorEnd = (Color){ 255, 255, 255, 0 },
        .blend = ATTACK_BLEND_ADDITIVE,
        .rotateSpeed = 0.0f,
        .ground = false,
        .shape = ATTACK_SHAPE_SPRITE,
        .emissive = 0.55f,
        .loop = false,
        .fitField = false,
        .useEventColor = false
    };
}

void AttackPresentationTemplate(const ContentCatalog *catalog, int abilityIndex,
                                AttackPresentation *out)
{
    memset(out, 0, sizeof(*out));
    out->authored = true;
    out->projectile = (AttackProjectileVisual){
        .tintOverride = false,
        .tint = (Color){ 255, 255, 255, 255 },
        .glow = 1.0f,
        .visualScale = 1.0f,
        .spin = 0.0f,
        .trailLength = 0.0f
    };

    const AbilityDefinition *ability =
        (abilityIndex >= 0 && abilityIndex < catalog->abilityCount)
        ? &catalog->abilities[abilityIndex] : NULL;

    // Muzzle flash at the cast point.
    AttackEffectLayer muzzle = DefaultLayer();
    muzzle.anchor = ATTACK_ANCHOR_CAST;
    muzzle.atlas = 3;               // energy loop
    muzzle.frameCount = 8;
    muzzle.fps = 30.0f;
    muzzle.duration = 0.28f;
    muzzle.scaleStart = 0.9f;
    muzzle.scaleEnd = 1.5f;
    out->layers[0] = muzzle;

    // Impact burst where the shot lands.
    AttackEffectLayer impact = DefaultLayer();
    impact.anchor = ATTACK_ANCHOR_IMPACT;
    impact.atlas = 1;               // explosion
    impact.frameCount = 14;
    impact.fps = 30.0f;
    impact.duration = 0.45f;
    impact.forward = 0.0f;
    impact.up = 0.7f;
    impact.scaleStart = 1.2f;
    impact.scaleEnd = 1.8f;
    out->layers[1] = impact;

    // Sparks thrown from the impact.
    AttackEffectLayer sparks = DefaultLayer();
    sparks.anchor = ATTACK_ANCHOR_IMPACT;
    sparks.atlas = 0;               // shapes
    sparks.pattern = ATTACK_PATTERN_BURST;
    sparks.count = 6;
    sparks.duration = 0.5f;
    sparks.forward = 0.0f;
    sparks.up = 0.5f;
    sparks.spreadDeg = 150.0f;
    sparks.speed = 5.5f;
    sparks.gravity = -9.0f;
    sparks.scaleStart = 0.32f;
    sparks.scaleEnd = 0.05f;
    out->layers[2] = sparks;

    // Wide-area behaviors read better with a grounded ring on cast.
    if (ability && (ability->behavior == ABILITY_BEHAVIOR_RAIN ||
                    ability->behavior == ABILITY_BEHAVIOR_SOUND_WAVE ||
                    ability->behavior == ABILITY_BEHAVIOR_HEALING_BURST))
    {
        AttackEffectLayer ring = DefaultLayer();
        ring.anchor = ATTACK_ANCHOR_CAST;
        ring.pattern = ATTACK_PATTERN_RING;
        ring.count = 10;
        ring.duration = 0.6f;
        ring.forward = 0.0f;
        ring.up = 0.1f;
        ring.speed = 4.0f;
        ring.scaleStart = 0.4f;
        ring.scaleEnd = 0.1f;
        ring.ground = true;
        out->layers[3] = ring;
    }
}

//------------------------------------------------------------------------------------
// Validation
//------------------------------------------------------------------------------------
static bool FiniteRange(float value, float lo, float hi)
{
    return isfinite(value) && value >= lo && value <= hi;
}

static bool ValidateLayer(const AttackEffectLayer *layer, const char *abilityId,
                          int index, char *message, int capacity)
{
    #define LAYER_CHECK(cond, what) do { \
        if (!(cond)) { \
            snprintf(message, capacity, "%s layer %d: %s", abilityId, index, what); \
            return false; \
        } \
    } while (0)

    LAYER_CHECK(layer->anchor >= 0 && layer->anchor < ATTACK_ANCHOR_COUNT, "bad anchor");
    LAYER_CHECK(layer->atlas >= 0 && layer->atlas < MAX_ATTACK_ATLASES, "bad atlas");
    LAYER_CHECK(layer->frame >= 0 && layer->frame < 256, "bad frame");
    LAYER_CHECK(layer->frameCount >= 1 && layer->frameCount <= 64, "bad frame count");
    LAYER_CHECK(FiniteRange(layer->fps, 0.0f, 120.0f), "bad fps");
    LAYER_CHECK(layer->pattern >= 0 && layer->pattern < ATTACK_PATTERN_COUNT, "bad pattern");
    LAYER_CHECK(layer->count >= 1 && layer->count <= 32, "bad count");
    LAYER_CHECK(FiniteRange(layer->delay, 0.0f, 5.0f), "bad delay");
    LAYER_CHECK(FiniteRange(layer->duration, 0.02f, 10.0f), "bad duration");
    LAYER_CHECK(FiniteRange(layer->forward, -20.0f, 20.0f) &&
                FiniteRange(layer->up, -20.0f, 20.0f) &&
                FiniteRange(layer->side, -20.0f, 20.0f), "bad offset");
    LAYER_CHECK(FiniteRange(layer->spreadDeg, 0.0f, 360.0f), "bad spread");
    LAYER_CHECK(FiniteRange(layer->speed, -100.0f, 100.0f), "bad speed");
    LAYER_CHECK(FiniteRange(layer->gravity, -100.0f, 100.0f), "bad gravity");
    LAYER_CHECK(FiniteRange(layer->drag, 0.0f, 20.0f), "bad drag");
    LAYER_CHECK(FiniteRange(layer->scaleStart, 0.0f, 30.0f) &&
                FiniteRange(layer->scaleEnd, 0.0f, 30.0f), "bad scale");
    LAYER_CHECK(layer->blend >= 0 && layer->blend < ATTACK_BLEND_COUNT, "bad blend");
    LAYER_CHECK(FiniteRange(layer->rotateSpeed, -3600.0f, 3600.0f), "bad rotation");
    LAYER_CHECK(layer->shape >= 0 && layer->shape < ATTACK_SHAPE_COUNT, "bad shape");
    LAYER_CHECK(FiniteRange(layer->emissive, 0.0f, 1.0f), "bad emissive");

    #undef LAYER_CHECK
    return true;
}

bool AttackContentValidate(const ContentCatalog *catalog, char *message, int capacity)
{
    for (int i = 0; i < catalog->abilityCount; i++)
    {
        const AttackPresentation *doc = &catalog->attacks[i];
        if (!doc->authored) continue;
        const char *id = catalog->abilities[i].id;

        for (int layer = 0; layer < MAX_ATTACK_LAYERS; layer++)
            if (doc->layers[layer].used &&
                !ValidateLayer(&doc->layers[layer], id, layer, message, capacity))
                return false;

        for (int motion = 0; motion < MAX_ATTACK_MOTIONS; motion++)
        {
            const AttackMotion *m = &doc->motions[motion];
            if (!m->used) continue;
            if (m->kind < 0 || m->kind >= ATTACK_MOTION_COUNT ||
                !FiniteRange(m->delay, 0.0f, 5.0f) ||
                !FiniteRange(m->duration, 0.02f, 5.0f) ||
                !FiniteRange(m->amplitude, 0.0f, 3.0f))
            {
                snprintf(message, capacity, "%s motion %d is invalid", id, motion);
                return false;
            }
        }

        const AttackProjectileVisual *pv = &doc->projectile;
        if (!FiniteRange(pv->glow, 0.0f, 8.0f) ||
            !FiniteRange(pv->visualScale, 0.05f, 10.0f) ||
            !FiniteRange(pv->spin, -30.0f, 30.0f) ||
            !FiniteRange(pv->trailLength, 0.0f, 2.0f))
        {
            snprintf(message, capacity, "%s projectile visuals are invalid", id);
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------------
// Serialization: `attack <abilityId>` opens a document; `layer <i> key=value ...`,
// `motion <i> ...`, and `projectile ...` lines fill it. Unknown keys are skipped so
// the schema can grow without breaking older files.
//------------------------------------------------------------------------------------
static int AbilityIndexById(const ContentCatalog *catalog, const char *id)
{
    for (int i = 0; i < catalog->abilityCount; i++)
        if (strcmp(catalog->abilities[i].id, id) == 0) return i;
    return -1;
}

static bool ParseColorField(const char *key, const char *value, const char *prefix,
                            Color *color)
{
    size_t length = strlen(prefix);
    if (strncmp(key, prefix, length) != 0 || key[length + 1] != '\0') return false;
    int channel = atoi(value);
    if (channel < 0) channel = 0;
    if (channel > 255) channel = 255;
    switch (key[length])
    {
        case 'r': color->r = (unsigned char)channel; return true;
        case 'g': color->g = (unsigned char)channel; return true;
        case 'b': color->b = (unsigned char)channel; return true;
        case 'a': color->a = (unsigned char)channel; return true;
        default: return false;
    }
}

static void AssignLayerField(AttackEffectLayer *layer, const char *key, const char *value)
{
    int named;
    if (strcmp(key, "anchor") == 0 &&
        (named = NameIndex(value, ANCHOR_NAMES, ATTACK_ANCHOR_COUNT)) >= 0)
        layer->anchor = named;
    else if (strcmp(key, "pattern") == 0 &&
             (named = NameIndex(value, PATTERN_NAMES, ATTACK_PATTERN_COUNT)) >= 0)
        layer->pattern = named;
    else if (strcmp(key, "blend") == 0 &&
             (named = NameIndex(value, BLEND_NAMES, ATTACK_BLEND_COUNT)) >= 0)
        layer->blend = named;
    else if (strcmp(key, "atlas") == 0) layer->atlas = atoi(value);
    else if (strcmp(key, "frame") == 0) layer->frame = atoi(value);
    else if (strcmp(key, "frames") == 0) layer->frameCount = atoi(value);
    else if (strcmp(key, "fps") == 0) layer->fps = (float)atof(value);
    else if (strcmp(key, "count") == 0) layer->count = atoi(value);
    else if (strcmp(key, "delay") == 0) layer->delay = (float)atof(value);
    else if (strcmp(key, "duration") == 0) layer->duration = (float)atof(value);
    else if (strcmp(key, "forward") == 0) layer->forward = (float)atof(value);
    else if (strcmp(key, "up") == 0) layer->up = (float)atof(value);
    else if (strcmp(key, "side") == 0) layer->side = (float)atof(value);
    else if (strcmp(key, "spread") == 0) layer->spreadDeg = (float)atof(value);
    else if (strcmp(key, "speed") == 0) layer->speed = (float)atof(value);
    else if (strcmp(key, "gravity") == 0) layer->gravity = (float)atof(value);
    else if (strcmp(key, "drag") == 0) layer->drag = (float)atof(value);
    else if (strcmp(key, "scale0") == 0) layer->scaleStart = (float)atof(value);
    else if (strcmp(key, "scale1") == 0) layer->scaleEnd = (float)atof(value);
    else if (strcmp(key, "rotate") == 0) layer->rotateSpeed = (float)atof(value);
    else if (strcmp(key, "ground") == 0) layer->ground = atoi(value) != 0;
    else if (strcmp(key, "shape") == 0 &&
             (named = NameIndex(value, SHAPE_NAMES, ATTACK_SHAPE_COUNT)) >= 0)
        layer->shape = named;
    else if (strcmp(key, "emissive") == 0) layer->emissive = (float)atof(value);
    else if (strcmp(key, "loop") == 0) layer->loop = atoi(value) != 0;
    else if (strcmp(key, "fit") == 0) layer->fitField = atoi(value) != 0;
    else if (strcmp(key, "usecolor") == 0) layer->useEventColor = atoi(value) != 0;
    else if (ParseColorField(key, value, "c0", &layer->colorStart)) { }
    else if (ParseColorField(key, value, "c1", &layer->colorEnd)) { }
}

bool AttackContentLoadFile(ContentCatalog *catalog, const char *path,
                           char *message, int capacity)
{
    FILE *file = fopen(path, "r");
    if (!file) return true;    // nothing authored at this path yet

    AttackPresentation staged[MAX_ABILITIES];
    memcpy(staged, catalog->attacks, sizeof(staged));

    char line[512];
    int lineNumber = 0;
    int current = -1;
    while (fgets(line, sizeof(line), file))
    {
        lineNumber++;
        char *cursor = line;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '#' || *cursor == '\n' || *cursor == '\0') continue;

        char word[64];
        int consumed = 0;
        if (sscanf(cursor, "%63s%n", word, &consumed) != 1) continue;
        cursor += consumed;

        if (strcmp(word, "format_version") == 0) continue;
        if (strcmp(word, "attack") == 0)
        {
            char id[64];
            if (sscanf(cursor, "%63s", id) != 1)
            {
                snprintf(message, capacity, "%s:%d attack line has no id", path, lineNumber);
                fclose(file);
                return false;
            }
            current = AbilityIndexById(catalog, id);
            // Unknown ability ids are skipped, not fatal: content evolves.
            if (current >= 0)
            {
                memset(&staged[current], 0, sizeof(staged[current]));
                staged[current].authored = true;
                staged[current].projectile = (AttackProjectileVisual){
                    .tint = (Color){ 255, 255, 255, 255 },
                    .glow = 1.0f, .visualScale = 1.0f
                };
            }
            continue;
        }
        if (current < 0) continue;

        if (strcmp(word, "layer") == 0 || strcmp(word, "motion") == 0)
        {
            bool isLayer = word[0] == 'l';
            int index = -1;
            if (sscanf(cursor, "%d%n", &index, &consumed) != 1 || index < 0 ||
                index >= (isLayer ? MAX_ATTACK_LAYERS : MAX_ATTACK_MOTIONS))
            {
                snprintf(message, capacity, "%s:%d has a bad slot index", path, lineNumber);
                fclose(file);
                return false;
            }
            cursor += consumed;

            AttackEffectLayer *layer = NULL;
            AttackMotion *motion = NULL;
            if (isLayer)
            {
                layer = &staged[current].layers[index];
                *layer = DefaultLayer();
            }
            else
            {
                motion = &staged[current].motions[index];
                *motion = (AttackMotion){ .used = true, .duration = 0.3f, .amplitude = 1.0f };
            }

            char pair[96];
            while (sscanf(cursor, "%95s%n", pair, &consumed) == 1)
            {
                cursor += consumed;
                char *equals = strchr(pair, '=');
                if (!equals) continue;
                *equals = '\0';
                const char *value = equals + 1;
                if (layer) AssignLayerField(layer, pair, value);
                else
                {
                    int named;
                    if (strcmp(pair, "kind") == 0 &&
                        (named = NameIndex(value, MOTION_NAMES, ATTACK_MOTION_COUNT)) >= 0)
                        motion->kind = named;
                    else if (strcmp(pair, "delay") == 0) motion->delay = (float)atof(value);
                    else if (strcmp(pair, "duration") == 0) motion->duration = (float)atof(value);
                    else if (strcmp(pair, "amplitude") == 0) motion->amplitude = (float)atof(value);
                }
            }
            continue;
        }

        if (strcmp(word, "projectile") == 0)
        {
            AttackProjectileVisual *pv = &staged[current].projectile;
            char pair[96];
            while (sscanf(cursor, "%95s%n", pair, &consumed) == 1)
            {
                cursor += consumed;
                char *equals = strchr(pair, '=');
                if (!equals) continue;
                *equals = '\0';
                const char *value = equals + 1;
                if (strcmp(pair, "tint") == 0) pv->tintOverride = atoi(value) != 0;
                else if (strcmp(pair, "hide") == 0) pv->hideBody = atoi(value) != 0;
                else if (strcmp(pair, "glow") == 0) pv->glow = (float)atof(value);
                else if (strcmp(pair, "scale") == 0) pv->visualScale = (float)atof(value);
                else if (strcmp(pair, "spin") == 0) pv->spin = (float)atof(value);
                else if (strcmp(pair, "trail") == 0) pv->trailLength = (float)atof(value);
                else if (ParseColorField(pair, value, "c", &pv->tint)) { }
            }
            continue;
        }
    }
    fclose(file);

    // Validate the staged catalog before letting it replace live documents.
    ContentCatalog *mutable = (ContentCatalog *)catalog;
    AttackPresentation backup[MAX_ABILITIES];
    memcpy(backup, mutable->attacks, sizeof(backup));
    memcpy(mutable->attacks, staged, sizeof(staged));
    if (!AttackContentValidate(catalog, message, capacity))
    {
        memcpy(mutable->attacks, backup, sizeof(backup));
        return false;
    }
    return true;
}

static void WriteLayer(FILE *file, int index, const AttackEffectLayer *layer)
{
    fprintf(file,
            "layer %d anchor=%s atlas=%d frame=%d frames=%d fps=%g pattern=%s "
            "count=%d delay=%g duration=%g forward=%g up=%g side=%g spread=%g "
            "speed=%g gravity=%g drag=%g scale0=%g scale1=%g "
            "c0r=%d c0g=%d c0b=%d c0a=%d c1r=%d c1g=%d c1b=%d c1a=%d "
            "blend=%s rotate=%g ground=%d shape=%s emissive=%g loop=%d fit=%d usecolor=%d\n",
            index, ANCHOR_NAMES[layer->anchor], layer->atlas, layer->frame,
            layer->frameCount, layer->fps, PATTERN_NAMES[layer->pattern],
            layer->count, layer->delay, layer->duration, layer->forward,
            layer->up, layer->side, layer->spreadDeg, layer->speed,
            layer->gravity, layer->drag, layer->scaleStart, layer->scaleEnd,
            layer->colorStart.r, layer->colorStart.g, layer->colorStart.b,
            layer->colorStart.a, layer->colorEnd.r, layer->colorEnd.g,
            layer->colorEnd.b, layer->colorEnd.a, BLEND_NAMES[layer->blend],
            layer->rotateSpeed, layer->ground ? 1 : 0,
            SHAPE_NAMES[layer->shape], layer->emissive,
            layer->loop ? 1 : 0, layer->fitField ? 1 : 0,
            layer->useEventColor ? 1 : 0);
}

bool AttackContentSaveFile(const ContentCatalog *catalog, const char *path,
                           char *message, int capacity)
{
    if (!AttackContentValidate(catalog, message, capacity)) return false;

    char temporary[512];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE *file = fopen(temporary, "w");
    if (!file)
    {
        snprintf(message, capacity, "cannot open %s for writing", temporary);
        return false;
    }

    fprintf(file, "# Brawl Arena authored attack presentation.\n");
    fprintf(file, "format_version 1\n");
    for (int i = 0; i < catalog->abilityCount; i++)
    {
        const AttackPresentation *doc = &catalog->attacks[i];
        if (!doc->authored) continue;

        fprintf(file, "\nattack %s\n", catalog->abilities[i].id);
        for (int layer = 0; layer < MAX_ATTACK_LAYERS; layer++)
            if (doc->layers[layer].used)
                WriteLayer(file, layer, &doc->layers[layer]);
        for (int motion = 0; motion < MAX_ATTACK_MOTIONS; motion++)
        {
            const AttackMotion *m = &doc->motions[motion];
            if (!m->used) continue;
            fprintf(file, "motion %d kind=%s delay=%g duration=%g amplitude=%g\n",
                    motion, MOTION_NAMES[m->kind], m->delay, m->duration,
                    m->amplitude);
        }
        const AttackProjectileVisual *pv = &doc->projectile;
        fprintf(file,
                "projectile hide=%d tint=%d cr=%d cg=%d cb=%d ca=%d glow=%g scale=%g "
                "spin=%g trail=%g\n",
                pv->hideBody ? 1 : 0,
                pv->tintOverride ? 1 : 0, pv->tint.r, pv->tint.g, pv->tint.b,
                pv->tint.a, pv->glow, pv->visualScale, pv->spin, pv->trailLength);
    }

    if (fflush(file) != 0 || fclose(file) != 0)
    {
        remove(temporary);
        snprintf(message, capacity, "write to %s failed", temporary);
        return false;
    }
    if (rename(temporary, path) != 0)
    {
        remove(temporary);
        snprintf(message, capacity, "could not replace %s", path);
        return false;
    }
    return true;
}
