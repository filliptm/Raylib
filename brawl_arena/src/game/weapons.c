#include "weapons.h"
#include "arena.h"
#include "brawler.h"
#include "game_events.h"
#include "raymath.h"
#include "game_random.h"
#include "content_catalog.h"
#include <stddef.h>
#include <math.h>

static Projectile *AllocProjectile(GameContext w)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
        if (!w.session->projectiles[i].active) return &w.session->projectiles[i];
    return NULL;
}

static AbilityField *AllocAbilityField(GameContext w)
{
    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
        if (!w.session->abilityFields[i].active) return &w.session->abilityFields[i];
    return NULL;
}

bool WeaponsMineActive(GameContext w, int owner, bool *armed)
{
    if (armed) *armed = false;
    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
    {
        AbilityField *field = &w.session->abilityFields[i];
        if (!field->active || field->type != ABILITY_FIELD_MINE ||
            field->owner != owner)
            continue;
        if (armed) *armed = field->armed;
        return true;
    }
    return false;
}

bool WeaponsPlaceMine(GameContext w, int owner,
                      const AbilityDefinition *ability)
{
    if (!ability || ability->behavior != ABILITY_BEHAVIOR_MINE ||
        owner < 0 || owner >= w.session->brawlerCount)
        return false;

    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
    {
        AbilityField *existing = &w.session->abilityFields[i];
        if (existing->active && existing->type == ABILITY_FIELD_MINE &&
            existing->owner == owner)
            existing->active = false;
    }

    AbilityField *field = AllocAbilityField(w);
    if (!field) return false;
    Brawler *b = &w.session->brawlers[owner];
    *field = (AbilityField){
        .position = { b->position.x, 0.0f, b->position.z },
        .radius = ability->radius,
        .growTime = ability->data.mine.armTime,
        .armTime = ability->data.mine.armTime,
        .triggerRadius = ability->data.mine.triggerRadius,
        .knockback = ability->data.mine.knockback,
        .damage = ability->damage,
        .team = b->team,
        .owner = owner,
        .type = ABILITY_FIELD_MINE,
        .active = true
    };

    Vector3 ground = { b->position.x, 0.14f, b->position.z };
    GameEmitVfxAttached(
        w.session, VFX_MORTAR_MINE_PLACE,
        (Vector3){ b->position.x, 0.90f, b->position.z },
        ground, b->aimAngle, 1.0f,
        (Color){ 255, 176, 72, 255 },
        owner, VFX_SOCKET_RIGHT_HAND, -1, VFX_SOCKET_NONE);
    return true;
}

static VfxEffectId CastVfxFor(BrawlerClass cls, bool super)
{
    switch (cls)
    {
        case CLASS_SHOTGUNNER:
            return super ? VFX_SCRAPPER_SUPER_CAST : VFX_SCRAPPER_CAST;
        case CLASS_SNIPER:
            return super ? VFX_LONGSHOT_SUPER_CAST : VFX_LONGSHOT_CAST;
        case CLASS_LOBBER:
            return super ? VFX_MORTAR_SUPER_CAST : VFX_MORTAR_CAST;
        case CLASS_BRUISER:
            return super ? VFX_TANK_CHARGE_START : VFX_TANK_CAST;
        case CLASS_HEALER:
            return super ? VFX_GUARDIAN_RESONANCE_CAST
                         : VFX_GUARDIAN_RAIN_CAST;
        default:
            return VFX_NONE;
    }
}

static VfxEffectId ImpactVfxFor(GameContext w, const Projectile *projectile)
{
    if (projectile->owner < 0 ||
        projectile->owner >= w.session->brawlerCount)
        return VFX_NONE;
    switch (w.session->brawlers[projectile->owner].cls)
    {
        case CLASS_SHOTGUNNER:
            return projectile->isSuper ? VFX_SCRAPPER_SUPER_IMPACT
                                       : VFX_SCRAPPER_IMPACT;
        case CLASS_SNIPER:
            return projectile->isSuper ? VFX_LONGSHOT_SUPER_IMPACT
                                       : VFX_LONGSHOT_IMPACT;
        case CLASS_LOBBER:
            return projectile->isSuper ? VFX_MORTAR_SUPER_IMPACT
                                       : VFX_MORTAR_IMPACT;
        case CLASS_BRUISER:
            return VFX_TANK_IMPACT;
        default:
            return VFX_NONE;
    }
}

static Vector3 AimEndpoint(Vector3 position, float angle, float distance)
{
    return (Vector3){
        position.x + sinf(angle)*distance,
        position.y,
        position.z + cosf(angle)*distance
    };
}

static Color AbilityVfxColor(BrawlerClass cls, Team team, bool super)
{
    if (super) return (Color){ 255, 218, 104, 255 };
    switch (cls)
    {
        case CLASS_SNIPER: return (Color){ 118, 232, 255, 255 };
        case CLASS_LOBBER: return (Color){ 255, 174, 92, 255 };
        case CLASS_BRUISER: return (Color){ 255, 188, 96, 255 };
        case CLASS_HEALER: return (Color){ 70, 244, 166, 255 };
        default: return TEAM_COLORS[team];
    }
}

static float SuperGainFor(GameContext w, int owner)
{
    if (owner < 0 || owner >= w.session->brawlerCount) return 0.0f;
    BrawlerClass character = w.session->brawlers[owner].cls;
    return ContentMainAbility(w.content, character)->superPerHit*w.tuning->superMult;
}

static BrawlerDamageResult ApplyProjectileDamage(
    GameContext w, Projectile *projectile, int target, int damage,
    Vector3 hitPosition)
{
    BrawlerDamageResult result =
        BrawlerApplyDamageDetailed(w, target, damage,
                                   projectile->owner, hitPosition);
    if (result.healthRemoved <= 0 || projectile->selfHealRatio <= 0.0f ||
        projectile->owner < 0 ||
        projectile->owner >= w.session->brawlerCount)
        return result;

    int healing = (int)floorf(
        result.healthRemoved*projectile->selfHealRatio + 0.5f);
    if (healing > 0)
    {
        Brawler *owner = &w.session->brawlers[projectile->owner];
        Vector3 healPosition = { owner->position.x, 1.0f, owner->position.z };
        int restored = BrawlerApplyHealing(w, projectile->owner, healing,
                                           projectile->owner, healPosition);
        if (restored > 0)
            GameEmitVfxAttached(w.session, VFX_TANK_RECLAIM, hitPosition,
                                healPosition, 0.0f, 1.15f,
                                (Color){ 70, 244, 166, 255 },
                                -1, VFX_SOCKET_NONE,
                                projectile->owner, VFX_SOCKET_CHEST);
    }
    return result;
}

static bool BeginProjectileReturn(GameContext w, Projectile *projectile)
{
    if (!projectile || projectile->motion != PROJECTILE_MOTION_RETURNING ||
        !projectile->outbound)
        return false;
    if (projectile->owner < 0 ||
        projectile->owner >= w.session->brawlerCount)
        return false;

    Brawler *owner = &w.session->brawlers[projectile->owner];
    if (!owner->alive || owner->cls != projectile->ownerClass) return false;

    projectile->outbound = false;
    projectile->traveled = 0.0f;
    projectile->hitMask = 0;
    Vector3 toOwner = Vector3Subtract(owner->position, projectile->position);
    toOwner.y = 0.0f;
    if (Vector3Length(toOwner) > 0.001f)
        projectile->velocity =
            Vector3Scale(Vector3Normalize(toOwner), projectile->returnSpeed);

    GameEmitVfx(w.session, VFX_SCRAPPER_RETURN,
                projectile->position, owner->position,
                atan2f(projectile->velocity.x, projectile->velocity.z),
                projectile->isSuper ? 1.65f : 1.15f, projectile->color);
    return true;
}

Vector3 WeaponsArcLanding(GameContext w, const Brawler *b, float aimDist)
{
    const AbilityDefinition *ability = ContentMainAbility(w.content, b->cls);
    float d = Clamp(aimDist, 1.5f, ability->range);
    return (Vector3){
        b->position.x + sinf(b->aimAngle) * d,
        0.0f,
        b->position.z + cosf(b->aimAngle) * d
    };
}

static bool PointInSoundCone(Vector3 origin, float angle, float range, float spread,
                             Vector3 point)
{
    float dx = point.x - origin.x;
    float dz = point.z - origin.z;
    float distSq = dx*dx + dz*dz;
    if (distSq > range*range || distSq < 0.0001f) return false;

    float invDist = 1.0f/sqrtf(distSq);
    float dot = dx*invDist*sinf(angle) + dz*invDist*cosf(angle);
    return dot >= cosf(spread*0.5f);
}

//------------------------------------------------------------------------------------
void WeaponsFire(GameContext w, int idx, bool super, float aimDist)
{
    Brawler *b = &w.session->brawlers[idx];
    const AbilityDefinition *mainAbility = ContentMainAbility(w.content, b->cls);
    const AbilityDefinition *superAbility = ContentSuperAbility(w.content, b->cls);
    const AbilityDefinition *ability = super ? superAbility : mainAbility;
    if (!b->alive) return;

    Color healColor = { 70, 244, 166, 255 };
    Color muzzle = (super || ability->healing <= 0)
                 ? (super ? (Color){ 255, 225, 120, 255 } : TEAM_COLORS[b->team])
                 : healColor;

    if (!super && ability->behavior == ABILITY_BEHAVIOR_RAIN)
    {
        Vector3 landing = WeaponsArcLanding(w, b, aimDist);
        AbilityField *field = AllocAbilityField(w);
        if (field)
        {
            *field = (AbilityField){ 0 };
            field->position = landing;
            field->position.y = 0.035f;
            field->radius = ability->radius;
            field->growTime = ability->data.area.growTime;
            field->life = ability->data.area.duration;
            field->maxLife = ability->data.area.duration;
            field->tickTimer = 0.0f; // the first small pulse lands immediately
            // A non-positive tick rate would never advance the catch-up pulse loop.
            field->tickRate = fmaxf(ability->data.area.tickRate, 0.05f);
            field->damage = ability->damage;
            field->healing = ability->healing;
            field->team = b->team;
            field->owner = idx;
            field->type = ABILITY_FIELD_RAIN;
            field->active = true;
        }

        b->revealTimer = w.tuning->fireReveal;
        GameEmitMuzzle(w.session, (Vector3){ b->position.x, 0.8f, b->position.z },
                      b->aimAngle, healColor);
        GameEmitLight(w.session, (Vector3){ b->position.x, 1.0f, b->position.z },
                     healColor, 2.4f, 0.22f);
        GameEmitVfxAttached(w.session, VFX_GUARDIAN_RAIN_CAST,
                            (Vector3){ b->position.x, 0.85f, b->position.z },
                            (Vector3){ landing.x, 0.0f, landing.z },
                            b->aimAngle, 1.65f, healColor,
                            idx, VFX_SOCKET_RIGHT_HAND,
                            -1, VFX_SOCKET_NONE);
        GameEmitCharacterAction(w.session, idx, CHARACTER_ACTION_CAST);
        return;
    }

    // Resonance catches every visible target in a wide cone at cast time. The field is
    // only the travelling sound-wave visualization; each target owns its timed mark.
    if (super && ability->behavior == ABILITY_BEHAVIOR_SOUND_WAVE)
    {
        AbilityField *field = AllocAbilityField(w);
        if (field)
        {
            *field = (AbilityField){ 0 };
            field->position = b->position;
            field->position.y = 0.045f;
            field->angle = b->aimAngle;
            field->range = ability->range;
            field->spread = ability->data.area.spreadDegrees*DEG2RAD;
            field->life = ability->data.area.visualDuration;
            field->maxLife = ability->data.area.visualDuration;
            field->team = b->team;
            field->owner = idx;
            field->type = ABILITY_FIELD_SOUND_WAVE;
            field->active = true;
        }

        for (int i = 0; i < w.session->brawlerCount; i++)
        {
            Brawler *t = &w.session->brawlers[i];
            if (i == idx || !t->alive) continue;
            if (!PointInSoundCone(b->position, b->aimAngle, ability->range,
                                  ability->data.area.spreadDegrees*DEG2RAD,
                                  t->position)) continue;
            if (!ArenaLineOfSight(&w.session->arena, b->position, t->position)) continue;

            BrawlerApplyPulseStatus(w, i, b->team, idx,
                                    ability->damage, ability->healing,
                                    ability->data.area.duration,
                                    ability->data.area.tickRate);
            Color markColor = (t->team == b->team)
                            ? healColor : (Color){ 255, 116, 154, 255 };
            GameEmitImpact(w.session, (Vector3){ t->position.x, 0.8f, t->position.z }, markColor, 9);
            GameEmitVfxAttached(
                w.session,
                t->team == b->team
                    ? VFX_GUARDIAN_RESONANCE_HEAL
                    : VFX_GUARDIAN_RESONANCE_DAMAGE,
                t->position, t->position, 0.0f, 1.25f, markColor,
                i, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
        }

        b->revealTimer = w.tuning->fireReveal;
        GameEmitMuzzle(w.session, (Vector3){ b->position.x, 0.9f, b->position.z },
                      b->aimAngle, (Color){ 134, 244, 255, 255 });
        GameEmitLight(w.session, (Vector3){ b->position.x, 1.1f, b->position.z },
                     (Color){ 122, 239, 255, 255 }, ability->range*0.55f, 0.55f);
        Vector3 castStart = { b->position.x, 0.85f, b->position.z };
        GameEmitVfxAttached(
            w.session, VFX_GUARDIAN_RESONANCE_CAST,
            castStart, AimEndpoint(castStart, b->aimAngle, ability->range),
            b->aimAngle, 1.85f, (Color){ 122, 239, 255, 255 },
            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
        GameEmitCharacterAction(w.session, idx, CHARACTER_ACTION_SUPER);
        return;
    }

    // Sanctuary is centered on its caster. It heals living teammates (including the
    // caster), never revives, and never damages enemies.
    if (super && ability->behavior == ABILITY_BEHAVIOR_HEALING_BURST)
    {
        for (int i = 0; i < w.session->brawlerCount; i++)
        {
            Brawler *t = &w.session->brawlers[i];
            if (!t->alive || t->team != b->team) continue;
            if (Vector3Distance(b->position, t->position) > ability->range) continue;
            BrawlerApplyHealing(w, i, ability->healing, idx, t->position);
        }

        Vector3 ground = { b->position.x, 0.04f, b->position.z };
        GameEmitShockwave(w.session, ground, ability->range, 0.55f, healColor);
        GameEmitShockwave(w.session, ground, ability->range*0.62f, 0.35f,
                          (Color){ 190, 255, 222, 255 });
        GameEmitLight(w.session, (Vector3){ b->position.x, 1.0f, b->position.z },
                     healColor, ability->range, 0.45f);
        for (int i = 0; i < 28; i++)
        {
            float angle = GameRandomInt(&w.session->random, 0, 628)/100.0f;
            float radius = GameRandomInt(&w.session->random, 20, 100)/100.0f*ability->range;
            Vector3 pos = { b->position.x + sinf(angle)*radius, 0.15f,
                            b->position.z + cosf(angle)*radius };
            GameEmitParticle(w.session, pos, (Vector3){ 0, 1.6f, 0 }, healColor,
                            0.55f, 0.14f, PARTICLE_SPARK);
        }
        b->revealTimer = w.tuning->fireReveal;
        GameEmitVfxAttached(w.session, VFX_GENERIC_HEAL,
                            b->position, b->position,
                            0.0f, ability->range*0.48f, healColor,
                            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
        GameEmitCharacterAction(w.session, idx, CHARACTER_ACTION_CAST);
        return;
    }

    // The dash super has no projectile at all: it hands control to brawler.c.
    if (super && ability->behavior == ABILITY_BEHAVIOR_DASH)
    {
        b->dashTimer = ability->data.dash.duration;
        b->dashDir = (Vector3){ sinf(b->aimAngle), 0.0f, cosf(b->aimAngle) };
        b->dashAbility = ContentCharacter(w.content, b->cls)->superAbility;
        b->dashHitMask = 0;
        b->dashVfxTimer = 0.0f;
        b->velocity = (Vector3){ 0 };
        b->revealTimer = w.tuning->fireReveal;
        GameEmitMuzzle(w.session, b->position, b->aimAngle, muzzle);
        Vector3 castStart = { b->position.x, 0.75f, b->position.z };
        GameEmitVfxAttached(
            w.session, CastVfxFor(b->cls, true), castStart,
            AimEndpoint(castStart, b->aimAngle, 1.8f),
            b->aimAngle, 1.75f,
            AbilityVfxColor(b->cls, b->team, true),
            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
        GameEmitCharacterAction(w.session, idx, CHARACTER_ACTION_MOBILITY);
        return;
    }

    bool returning = ability->behavior == ABILITY_BEHAVIOR_RETURNING;
    int pellets = returning ? 1 : ability->data.projectile.pellets;
    float spreadDeg = returning ? 0.0f
                                : ability->data.projectile.spreadDegrees;
    float speed = returning ? ability->data.returning.outboundSpeed
                            : ability->data.projectile.speed;
    float range = ability->range;
    int damage = ability->damage;
    float radius = ability->radius;
    bool piercing = returning ? true : ability->data.projectile.piercing;

    if (pellets <= 0) return;
    GameEmitCharacterAction(w.session, idx,
                            super ? CHARACTER_ACTION_SUPER
                                  : CHARACTER_ACTION_MAIN);

    Vector3 origin = b->position;
    origin.y = 0.75f;

    float spread = spreadDeg * DEG2RAD;

    for (int i = 0; i < pellets; i++)
    {
        // Non-zero spread fans pellets evenly and adds a touch of organic scatter.
        // Zero-spread pairs use parallel lanes instead of occupying the same pixels:
        // a 1.1-radius center gap keeps Longshot's two bolts visibly distinct while
        // their silhouettes still overlap enough to read as one precision shot.
        float t = (pellets == 1) ? 0.5f : (i / (float)(pellets - 1));
        float offset = (t - 0.5f) * spread;
        offset += (GameRandomInt(&w.session->random, -100, 100) / 100.0f) * spread * 0.06f;
        float angle = b->aimAngle + offset;
        float laneOffset = 0.0f;
        if (!returning && ability->behavior == ABILITY_BEHAVIOR_PROJECTILE &&
            pellets > 1 && fabsf(spread) < 0.0001f)
        {
            laneOffset =
                (i - (pellets - 1)*0.5f)*radius*1.10f;
        }

        // A full pool drops the remaining pellets but must still run the post-loop
        // reveal and muzzle feedback: ammo and cooldown were already paid.
        Projectile *p = AllocProjectile(w);
        if (!p) break;

        *p = (Projectile){ 0 };
        Vector3 projectileOrigin = {
            origin.x + cosf(b->aimAngle)*laneOffset,
            origin.y,
            origin.z - sinf(b->aimAngle)*laneOffset
        };
        p->position = projectileOrigin;
        p->origin = projectileOrigin;
        p->team = b->team;
        p->owner = idx;
        p->damage = damage;
        p->healing = ability->healing;
        p->selfHealRatio = ability->selfHealRatio;
        p->radius = radius;
        p->range = range;
        p->isSuper = super;
        p->piercing = piercing;
        p->breaksWalls = super && !returning;
        p->rangeScaled = returning ? false
                                   : ability->data.projectile.rangeScaled;
        p->ownerClass = b->cls;
        p->color = super ? (Color){ 255, 214, 92, 255 }
                         : (p->healing > 0
                                ? healColor
                                : AbilityVfxColor(b->cls, b->team, false));
        p->active = true;

        if (returning)
        {
            p->motion = PROJECTILE_MOTION_RETURNING;
            p->outbound = true;
            p->returnSpeed = ability->data.returning.returnSpeed;
            p->outboundPull = ability->data.returning.outboundPull;
            p->returnKnockback = ability->data.returning.returnKnockback;
            p->breaksCrates = ability->data.returning.breaksCrates;
        }

        if (ability->behavior == ABILITY_BEHAVIOR_LOB)
        {
            Vector3 land = WeaponsArcLanding(w, b, aimDist);
            // Spread arcing shots by rotating the landing point around the shooter.
            if (pellets > 1)
            {
                float d = Vector3Distance((Vector3){ b->position.x, 0, b->position.z }, land);
                land.x = b->position.x + sinf(angle) * d;
                land.z = b->position.z + cosf(angle) * d;
            }

            p->arcing = true;
            p->arcStart = origin;
            p->arcEnd = land;
            p->arcDur = Vector3Distance(origin, land) / speed;
            if (p->arcDur < 0.15f) p->arcDur = 0.15f;
            p->arcHeight = Vector3Distance(origin, land) * 0.42f + 1.0f;
            p->arcT = 0.0f;
        }
        else
        {
            p->velocity = (Vector3){ sinf(angle) * speed, 0.0f, cosf(angle) * speed };
        }
    }

    b->revealTimer = w.tuning->fireReveal;
    GameEmitMuzzle(w.session, (Vector3){ origin.x + sinf(b->aimAngle) * 0.6f, origin.y, origin.z + cosf(b->aimAngle) * 0.6f },
                  b->aimAngle, muzzle);
    Vector3 castPosition = {
        origin.x + sinf(b->aimAngle)*0.6f,
        origin.y,
        origin.z + cosf(b->aimAngle)*0.6f
    };
    GameEmitVfxAttached(
        w.session, CastVfxFor(b->cls, super), castPosition,
        AimEndpoint(castPosition, b->aimAngle,
                    b->cls == CLASS_SNIPER ? 4.2f : 2.2f),
        b->aimAngle, super ? 1.75f : 1.35f,
        AbilityVfxColor(b->cls, b->team, super),
        idx, VFX_SOCKET_RIGHT_HAND, -1, VFX_SOCKET_NONE);
}

static void UpdateRainField(GameContext w, AbilityField *field, float dt)
{
    float age = field->maxLife - field->life;
    float growTime = field->growTime;
    if (growTime <= 0.0f) growTime = field->maxLife;
    float radius = field->radius*Clamp(age/growTime, 0.15f, 1.0f);

    field->tickTimer -= dt;
    while (field->tickTimer <= 0.0f && field->life > 0.0f)
    {
        bool affected = false;
        for (int i = 0; i < w.session->brawlerCount; i++)
        {
            Brawler *t = &w.session->brawlers[i];
            if (!t->alive) continue;

            float dx = t->position.x - field->position.x;
            float dz = t->position.z - field->position.z;
            if (dx*dx + dz*dz > radius*radius) continue;

            if (t->team == field->team)
            {
                int restored = BrawlerApplyHealing(w, i, field->healing,
                                                    field->owner, t->position);
                if (restored > 0)
                {
                    affected = true;
                    GameEmitVfxAttached(
                        w.session, VFX_GUARDIAN_RAIN_HEAL,
                        t->position, t->position, 0.0f, 1.08f,
                        (Color){ 70, 244, 166, 255 },
                        i, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
                    BrawlerAwardSuper(w, field->owner, SuperGainFor(w, field->owner));
                }
            }
            else
            {
                BrawlerApplyDamage(w, i, field->damage, field->owner, t->position);
                GameEmitVfxAttached(
                    w.session, VFX_GUARDIAN_RAIN_DAMAGE,
                    t->position, t->position, 0.0f, 1.12f,
                    (Color){ 255, 100, 164, 255 },
                    i, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
                affected = true;
                BrawlerAwardSuper(w, field->owner, SuperGainFor(w, field->owner));
            }
        }

        Color pulse = affected ? (Color){ 104, 255, 190, 255 }
                               : (Color){ 89, 207, 217, 220 };
        GameEmitShockwave(w.session, field->position, radius, 0.19f, pulse);
        GameEmitVfx(w.session, VFX_GUARDIAN_RAIN_PULSE,
                    field->position, field->position, 0.0f, radius, pulse);
        field->tickTimer += field->tickRate;
    }
}

static void AbilityFieldsUpdate(GameContext w, float dt)
{
    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
    {
        AbilityField *field = &w.session->abilityFields[i];
        if (!field->active) continue;

        if (field->type == ABILITY_FIELD_MINE)
        {
            if (field->owner < 0 ||
                field->owner >= w.session->brawlerCount ||
                !w.session->brawlers[field->owner].alive)
            {
                field->active = false;
                continue;
            }
            const AbilityDefinition *secondary = ContentSecondaryAbility(
                w.content, w.session->brawlers[field->owner].cls);
            if (!secondary ||
                secondary->behavior != ABILITY_BEHAVIOR_MINE)
            {
                field->active = false;
                continue;
            }

            if (!field->armed)
            {
                field->armTime = fmaxf(0.0f, field->armTime - dt);
                if (field->armTime <= 0.0f)
                {
                    field->armed = true;
                    GameEmitVfx(
                        w.session, VFX_MORTAR_MINE_ARM,
                        field->position, field->position, 0.0f, 1.0f,
                        TEAM_COLORS[field->team]);
                }
                continue;
            }

            bool triggered = false;
            for (int target = 0; target < w.session->brawlerCount; target++)
            {
                Brawler *b = &w.session->brawlers[target];
                if (!b->alive || b->team == field->team) continue;
                if (Vector3Distance(b->position, field->position) >
                    field->triggerRadius)
                    continue;
                if (!ArenaLineOfSight(
                        &w.session->arena, field->position, b->position))
                    continue;
                triggered = true;
                break;
            }
            if (!triggered) continue;

            GameEmitVfx(
                w.session, VFX_MORTAR_MINE_DETONATE,
                field->position, field->position, 0.0f,
                field->radius, (Color){ 255, 146, 62, 255 });
            GameEmitExplosion(
                w.session, field->position, field->radius,
                (Color){ 255, 146, 62, 255 });

            for (int target = 0; target < w.session->brawlerCount; target++)
            {
                Brawler *b = &w.session->brawlers[target];
                if (!b->alive || b->team == field->team) continue;
                Vector3 away = Vector3Subtract(b->position, field->position);
                away.y = 0.0f;
                float distance = Vector3Length(away);
                if (distance > field->radius ||
                    !ArenaLineOfSight(
                        &w.session->arena, field->position, b->position))
                    continue;
                if (distance <= 0.001f)
                {
                    float angle =
                        (float)(((target + 1)*37 + (field->owner + 1)*17) & 15)*
                        (PI*2.0f/16.0f);
                    away = (Vector3){ sinf(angle), 0.0f, cosf(angle) };
                }
                else away = Vector3Scale(away, 1.0f/distance);

                BrawlerApplyDamageDetailed(
                    w, target, field->damage, field->owner,
                    (Vector3){ b->position.x, 0.75f, b->position.z });
                BrawlerDisplace(
                    w, target, Vector3Scale(away, field->knockback));
            }
            field->active = false;
            continue;
        }

        field->life -= dt;
        if (field->type == ABILITY_FIELD_RAIN) UpdateRainField(w, field, dt);
        if (field->life <= 0.0f) field->active = false;
    }
}

//------------------------------------------------------------------------------------
// Splash damage where an arcing shot lands, or where a super detonates.
//------------------------------------------------------------------------------------
static void Detonate(GameContext w, Projectile *p, Vector3 at)
{
    GameEmitVfx(w.session, ImpactVfxFor(w, p), at, at, 0.0f,
                p->radius, p->color);
    GameEmitExplosion(w.session, at, p->radius, p->color);

    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *t = &w.session->brawlers[i];
        if (!t->alive || t->team == p->team) continue;

        float d = Vector3Distance((Vector3){ t->position.x, 0, t->position.z }, (Vector3){ at.x, 0, at.z });
        if (d > p->radius) continue;

        // Falloff so the edge of the blast chips rather than deletes.
        float falloff = 1.0f - (d / p->radius) * 0.45f;
        int dmg = (int)(p->damage * falloff);

        ApplyProjectileDamage(w, p, i, dmg, t->position);
        if (p->owner >= 0)
            BrawlerAwardSuper(w, p->owner, SuperGainFor(w, p->owner));
    }

    if (p->breaksWalls)
    {
        int broken = ArenaDamageRadius(&w.session->arena, at, p->radius, w.tuning->crateHealth);
        if (broken > 0) GameEmitCrateBreak(w.session, at);
    }
}

//------------------------------------------------------------------------------------
static bool ProjectileHitCheck(GameContext w, Projectile *p)
{
    // Walls first: an arcing shot flies over them entirely.
    if (!p->arcing && ArenaSolidAt(&w.session->arena, p->position.x, p->position.z))
    {
        if (p->motion == PROJECTILE_MOTION_RETURNING)
        {
            TileType cover =
                ArenaTypeAt(&w.session->arena, p->position.x, p->position.z);
            if (cover == TILE_CRATE && p->breaksCrates)
            {
                int tileX = ArenaTileX(&w.session->arena, p->position.x);
                int tileZ = ArenaTileZ(&w.session->arena, p->position.z);
                int crateHealth = p->damage;
                if (ArenaInBounds(&w.session->arena, tileX, tileZ))
                    crateHealth =
                        w.session->arena.tiles[tileZ][tileX].health;
                if (ArenaDamageAt(&w.session->arena, p->position.x,
                                  p->position.z, crateHealth))
                    GameEmitCrateBreak(w.session, p->position);
                return false;
            }
            if (p->outbound && BeginProjectileReturn(w, p)) return false;

            GameEmitImpact(w.session, p->position, p->color, 8);
            GameEmitVfx(w.session, ImpactVfxFor(w, p), p->position,
                        p->position, 0.0f, p->isSuper ? 1.35f : 1.0f,
                        p->color);
            return true;
        }

        if (p->breaksWalls)
        {
            if (ArenaDamageAt(&w.session->arena, p->position.x, p->position.z, p->damage))
                GameEmitCrateBreak(w.session, p->position);
            // Supers punch through crates but still stop at solid walls.
            if (ArenaTypeAt(&w.session->arena, p->position.x, p->position.z) != TILE_WALL) return false;
        }
        GameEmitImpact(w.session, p->position, p->color, 6);
        GameEmitVfx(w.session, ImpactVfxFor(w, p), p->position,
                    p->position, 0.0f, 1.0f, p->color);
        return true;
    }

    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *t = &w.session->brawlers[i];
        if (!t->alive) continue;
        if (p->hitMask & (1 << i)) continue;
        if (i == p->owner) continue;

        float dx = t->position.x - p->position.x;
        float dz = t->position.z - p->position.z;
        float reach = BRAWLER_RADIUS + p->radius;
        if (dx * dx + dz * dz > reach * reach) continue;

        if (t->team == p->team)
        {
            if (p->healing <= 0) continue;
            int restored = BrawlerApplyHealing(w, i, p->healing, p->owner, p->position);
            if (restored <= 0) continue; // full-health allies do not waste the bolt

            if (p->owner >= 0)
                BrawlerAwardSuper(w, p->owner, SuperGainFor(w, p->owner));
            p->hitMask |= (1 << i);
            if (!p->piercing) return true;
            continue;
        }

        int dmg = p->damage;
        if (p->rangeScaled)
        {
            float ratio = Clamp(p->traveled / p->range, 0.0f, 1.0f);
            dmg = (int)(p->damage * (0.5f + 0.5f * ratio));
        }

        BrawlerDamageResult damageResult =
            ApplyProjectileDamage(w, p, i, dmg, p->position);
        if (p->owner >= 0)
            BrawlerAwardSuper(w, p->owner, SuperGainFor(w, p->owner));

        if ((damageResult.healthRemoved > 0 ||
             damageResult.shieldAbsorbed > 0) &&
            p->motion == PROJECTILE_MOTION_RETURNING)
        {
            if (p->outbound && p->outboundPull > 0.0f)
            {
                Vector3 direction = Vector3Normalize(
                    (Vector3){ p->velocity.x, 0.0f, p->velocity.z });
                Vector3 fromLine = Vector3Subtract(t->position, p->position);
                float along = Vector3DotProduct(fromLine, direction);
                Vector3 nearest =
                    Vector3Add(p->position, Vector3Scale(direction, along));
                Vector3 pull = Vector3Subtract(nearest, t->position);
                pull.y = 0.0f;
                if (Vector3Length(pull) > 0.001f)
                {
                    pull = Vector3Scale(
                        Vector3Normalize(pull),
                        fminf(p->outboundPull, Vector3Length(pull)));
                    BrawlerDisplace(w, i, pull);
                }
            }
            else if (!p->outbound && p->returnKnockback > 0.0f)
            {
                Vector3 direction = Vector3Normalize(
                    (Vector3){ p->velocity.x, 0.0f, p->velocity.z });
                BrawlerDisplace(
                    w, i, Vector3Scale(direction, p->returnKnockback));
            }
        }

        GameEmitImpact(w.session, p->position, p->color, 8);
        GameEmitVfx(w.session, ImpactVfxFor(w, p), p->position,
                    p->position, 0.0f, p->isSuper ? 1.25f : 0.9f,
                    p->color);
        p->hitMask |= (1 << i);

        if (!p->piercing) return true;
    }
    return false;
}

void ProjectilesUpdate(GameContext w, float dt)
{
    AbilityFieldsUpdate(w, dt);

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w.session->projectiles[i];
        if (!p->active) continue;

        if (p->motion == PROJECTILE_MOTION_RETURNING)
        {
            if (p->owner < 0 || p->owner >= w.session->brawlerCount ||
                !w.session->brawlers[p->owner].alive ||
                w.session->brawlers[p->owner].cls != p->ownerClass)
            {
                p->active = false;
                continue;
            }

            if (!p->outbound)
            {
                Brawler *owner = &w.session->brawlers[p->owner];
                Vector3 toOwner = Vector3Subtract(owner->position, p->position);
                toOwner.y = 0.0f;
                if (Vector3Length(toOwner) <=
                    BRAWLER_RADIUS + p->radius*0.65f)
                {
                    GameEmitVfxAttached(
                        w.session, VFX_SCRAPPER_CATCH,
                        p->position, owner->position, owner->aimAngle,
                        p->isSuper ? 1.55f : 1.05f, p->color,
                        p->owner, VFX_SOCKET_RIGHT_HAND,
                        -1, VFX_SOCKET_NONE);
                    p->active = false;
                    continue;
                }
                p->velocity =
                    Vector3Scale(Vector3Normalize(toOwner), p->returnSpeed);
            }
        }

        if (p->arcing)
        {
            p->arcT += dt / p->arcDur;
            if (p->arcT >= 1.0f)
            {
                Detonate(w, p, p->arcEnd);
                p->active = false;
                continue;
            }

            Vector3 flat = Vector3Lerp(p->arcStart, p->arcEnd, p->arcT);
            flat.y = sinf(p->arcT * PI) * p->arcHeight;
            p->position = flat;

            if (GameRandomInt(&w.session->random, 0, 100) < 45)
            {
                Color trail = p->color;
                trail.a = 150;
                GameEmitParticle(w.session, p->position, (Vector3){ 0, 0, 0 }, trail, 0.22f, 0.11f, PARTICLE_SMOKE);
            }
            continue;
        }

        // Sub-step fast projectiles so nothing tunnels through a brawler or wall.
        float frameDist = Vector3Length(p->velocity) * dt;
        int steps = (int)(frameDist / 0.3f) + 1;
        float sub = dt / steps;
        bool consumed = false;
        bool turned = false;

        for (int s = 0; s < steps; s++)
        {
            Vector3 delta = Vector3Scale(p->velocity, sub);
            p->position = Vector3Add(p->position, delta);
            p->traveled += Vector3Length(delta);

            if (ProjectileHitCheck(w, p)) { consumed = true; break; }

            if (p->traveled >= p->range &&
                (p->motion != PROJECTILE_MOTION_RETURNING || p->outbound))
            {
                if (p->motion == PROJECTILE_MOTION_RETURNING && p->outbound)
                {
                    if (!BeginProjectileReturn(w, p)) consumed = true;
                    turned = true;
                    break;
                }
                if (p->isSuper && p->radius > 1.0f) Detonate(w, p, p->position);
                consumed = true;
                break;
            }
        }

        if (consumed) { p->active = false; continue; }
        if (turned) continue;

        if (GameRandomInt(&w.session->random, 0, 100) < 30)
        {
            Color trail = p->color;
            trail.a = 140;
            GameEmitParticle(w.session, p->position, (Vector3){ 0, 0, 0 }, trail, 0.13f, 0.07f, PARTICLE_MUZZLE);
        }
    }
}
