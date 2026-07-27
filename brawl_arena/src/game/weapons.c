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

static float SuperGainFor(GameContext w, int owner)
{
    if (owner < 0 || owner >= w.session->brawlerCount) return 0.0f;
    BrawlerClass character = w.session->brawlers[owner].cls;
    return ContentMainAbility(w.content, character)->superPerHit*w.tuning->superMult;
}

static int ApplyProjectileDamage(GameContext w, Projectile *projectile,
                                 int target, int damage, Vector3 hitPosition)
{
    int removed = BrawlerApplyDamage(w, target, damage,
                                     projectile->owner, hitPosition);
    if (removed <= 0 || projectile->selfHealRatio <= 0.0f ||
        projectile->owner < 0 ||
        projectile->owner >= w.session->brawlerCount)
        return removed;

    int healing = (int)floorf(removed*projectile->selfHealRatio + 0.5f);
    if (healing > 0)
    {
        Brawler *owner = &w.session->brawlers[projectile->owner];
        Vector3 healPosition = { owner->position.x, 1.0f, owner->position.z };
        BrawlerApplyHealing(w, projectile->owner, healing,
                            projectile->owner, healPosition);
    }
    return removed;
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
        AbilityField *field = AllocAbilityField(w);
        if (field)
        {
            *field = (AbilityField){ 0 };
            field->position = WeaponsArcLanding(w, b, aimDist);
            field->position.y = 0.035f;
            field->radius = ability->radius;
            field->growTime = ability->data.area.growTime;
            field->life = ability->data.area.duration;
            field->maxLife = ability->data.area.duration;
            field->tickTimer = 0.0f; // the first small pulse lands immediately
            field->tickRate = ability->data.area.tickRate;
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
        }

        b->revealTimer = w.tuning->fireReveal;
        GameEmitMuzzle(w.session, (Vector3){ b->position.x, 0.9f, b->position.z },
                      b->aimAngle, (Color){ 134, 244, 255, 255 });
        GameEmitLight(w.session, (Vector3){ b->position.x, 1.1f, b->position.z },
                     (Color){ 122, 239, 255, 255 }, ability->range*0.55f, 0.55f);
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
        return;
    }

    // The dash super has no projectile at all: it hands control to brawler.c.
    if (super && ability->behavior == ABILITY_BEHAVIOR_DASH)
    {
        b->dashTimer = ability->data.dash.duration;
        b->dashDir = (Vector3){ sinf(b->aimAngle), 0.0f, cosf(b->aimAngle) };
        b->dashAbility = ContentCharacter(w.content, b->cls)->superAbility;
        b->dashHitMask = 0;
        b->velocity = (Vector3){ 0 };
        b->revealTimer = w.tuning->fireReveal;
        GameEmitMuzzle(w.session, b->position, b->aimAngle, muzzle);
        return;
    }

    int pellets = ability->data.projectile.pellets;
    float spreadDeg = ability->data.projectile.spreadDegrees;
    float speed = ability->data.projectile.speed;
    float range = ability->range;
    int damage = ability->damage;
    float radius = ability->radius;
    bool piercing = ability->data.projectile.piercing;

    if (pellets <= 0) return;

    Vector3 origin = b->position;
    origin.y = 0.75f;

    float spread = spreadDeg * DEG2RAD;

    for (int i = 0; i < pellets; i++)
    {
        // Fan the pellets evenly, then add a touch of scatter so it feels organic.
        float t = (pellets == 1) ? 0.5f : (i / (float)(pellets - 1));
        float offset = (t - 0.5f) * spread;
        offset += (GameRandomInt(&w.session->random, -100, 100) / 100.0f) * spread * 0.06f;
        float angle = b->aimAngle + offset;

        Projectile *p = AllocProjectile(w);
        if (!p) return;

        *p = (Projectile){ 0 };
        p->position = origin;
        p->origin = origin;
        p->team = b->team;
        p->owner = idx;
        p->damage = damage;
        p->healing = ability->healing;
        p->selfHealRatio = ability->selfHealRatio;
        p->radius = radius;
        p->range = range;
        p->isSuper = super;
        p->piercing = piercing;
        p->breaksWalls = super;
        p->rangeScaled = ability->data.projectile.rangeScaled;
        p->color = super ? (Color){ 255, 214, 92, 255 }
                         : (p->healing > 0 ? healColor : TEAM_COLORS[b->team]);
        p->active = true;

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
                    BrawlerAwardSuper(w, field->owner, SuperGainFor(w, field->owner));
                }
            }
            else
            {
                BrawlerApplyDamage(w, i, field->damage, field->owner, t->position);
                affected = true;
                BrawlerAwardSuper(w, field->owner, SuperGainFor(w, field->owner));
            }
        }

        Color pulse = affected ? (Color){ 104, 255, 190, 255 }
                               : (Color){ 89, 207, 217, 220 };
        GameEmitShockwave(w.session, field->position, radius, 0.19f, pulse);
        field->tickTimer += field->tickRate;
    }
}

static void AbilityFieldsUpdate(GameContext w, float dt)
{
    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
    {
        AbilityField *field = &w.session->abilityFields[i];
        if (!field->active) continue;

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
        if (p->breaksWalls)
        {
            if (ArenaDamageAt(&w.session->arena, p->position.x, p->position.z, p->damage))
                GameEmitCrateBreak(w.session, p->position);
            // Supers punch through crates but still stop at solid walls.
            if (ArenaTypeAt(&w.session->arena, p->position.x, p->position.z) != TILE_WALL) return false;
        }
        GameEmitImpact(w.session, p->position, p->color, 6);
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

        ApplyProjectileDamage(w, p, i, dmg, p->position);
        if (p->owner >= 0)
            BrawlerAwardSuper(w, p->owner, SuperGainFor(w, p->owner));

        GameEmitImpact(w.session, p->position, p->color, 8);
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

        for (int s = 0; s < steps; s++)
        {
            Vector3 delta = Vector3Scale(p->velocity, sub);
            p->position = Vector3Add(p->position, delta);
            p->traveled += Vector3Length(delta);

            if (ProjectileHitCheck(w, p)) { consumed = true; break; }

            if (p->traveled >= p->range)
            {
                if (p->isSuper && p->radius > 1.0f) Detonate(w, p, p->position);
                consumed = true;
                break;
            }
        }

        if (consumed) { p->active = false; continue; }

        if (GameRandomInt(&w.session->random, 0, 100) < 30)
        {
            Color trail = p->color;
            trail.a = 140;
            GameEmitParticle(w.session, p->position, (Vector3){ 0, 0, 0 }, trail, 0.13f, 0.07f, PARTICLE_MUZZLE);
        }
    }
}
