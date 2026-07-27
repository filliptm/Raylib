#include "weapons.h"
#include "arena.h"
#include "brawler.h"
#include "effects.h"
#include "raymath.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

//------------------------------------------------------------------------------------
// Class roster. Each entry defines a main attack and a super.
// Health and damage use Brawl Stars' scale so the numbers read familiarly.
//------------------------------------------------------------------------------------
const WeaponDef WEAPON_DEFAULTS[CLASS_COUNT] = {
    // SHOTGUNNER - close range, wide spread, rewards closing the gap
    {
        .name = "SCRAPPER", .flavor = "Close-range spread", .maxHealth = 3800,
        .pellets = 5, .spreadDeg = 24.0f, .speed = 34.0f, .range = 13.0f,
        .damage = 320, .projRadius = 0.22f, .cooldown = 0.36f,
        .reloadPerAmmo = 1.35f, .superPerHit = 0.085f,
        .arcing = false, .rangeScaled = false,
        .superName = "BUCKSHOT", .sPellets = 9, .sSpreadDeg = 34.0f, .sSpeed = 36.0f,
        .sRange = 15.0f, .sDamage = 460, .sProjRadius = 0.26f, .sPiercing = false, .sDash = false
    },
    // SNIPER - long range, single shot, damage ramps with travel distance
    {
        .name = "LONGSHOT", .flavor = "Damage grows with distance", .maxHealth = 2800,
        .pellets = 1, .spreadDeg = 0.0f, .speed = 56.0f, .range = 26.0f,
        .damage = 1600, .projRadius = 0.20f, .cooldown = 0.52f,
        .reloadPerAmmo = 1.70f, .superPerHit = 0.30f,
        .arcing = false, .rangeScaled = true,
        .superName = "RAILGUN", .sPellets = 1, .sSpreadDeg = 0.0f, .sSpeed = 70.0f,
        .sRange = 34.0f, .sDamage = 2400, .sProjRadius = 0.34f, .sPiercing = true, .sDash = false
    },
    // LOBBER - arcs over walls, splashes on landing, cannot hit what it flies past
    {
        .name = "MORTAR", .flavor = "Lobs over walls, splash damage", .maxHealth = 3200,
        .pellets = 1, .spreadDeg = 0.0f, .speed = 16.0f, .range = 13.0f,
        .damage = 1000, .projRadius = 2.6f, .cooldown = 0.55f,
        .reloadPerAmmo = 1.60f, .superPerHit = 0.30f,
        .arcing = true, .rangeScaled = false,
        .superName = "BARRAGE", .sPellets = 3, .sSpreadDeg = 26.0f, .sSpeed = 16.0f,
        .sRange = 15.0f, .sDamage = 1100, .sProjRadius = 2.9f, .sPiercing = false, .sDash = false
    },
    // BRUISER - short range, tanky, closes distance with a dash super
    {
        .name = "TANK", .flavor = "Tanky brawler with a charge", .maxHealth = 5600,
        .pellets = 4, .spreadDeg = 18.0f, .speed = 30.0f, .range = 7.5f,
        .damage = 440, .projRadius = 0.24f, .cooldown = 0.42f,
        .reloadPerAmmo = 1.25f, .superPerHit = 0.095f,
        .arcing = false, .rangeScaled = false,
        .superName = "CHARGE", .sPellets = 0, .sSpreadDeg = 0.0f, .sSpeed = 0.0f,
        .sRange = 0.0f, .sDamage = 1200, .sProjRadius = 0.0f, .sPiercing = false, .sDash = true
    },
};

// The live copy gameplay reads. Seeded from WEAPON_DEFAULTS at startup.
WeaponDef WEAPONS[CLASS_COUNT];

const char *CLASS_NAMES[CLASS_COUNT] = { "SCRAPPER", "LONGSHOT", "MORTAR", "TANK" };
const char *BOT_MODE_NAMES[BOT_MODE_COUNT] = { "STATIC", "ROAM", "FIGHT" };

const Color TEAM_COLORS[2] = {
    { 70, 140, 235, 255 },    // player blue
    { 232, 82, 82, 255 }      // enemy red
};

const Color TEAM_DARK[2] = {
    { 38, 82, 158, 255 },
    { 158, 42, 42, 255 }
};

void WeaponsResetAll(void)
{
    memcpy(WEAPONS, WEAPON_DEFAULTS, sizeof(WEAPONS));
}

void WeaponsResetKit(BrawlerClass cls)
{
    WEAPONS[cls] = WEAPON_DEFAULTS[cls];
}

void TuningSetDefaults(Tuning *t)
{
    t->moveSpeed     = MOVE_SPEED;
    t->moveAccel     = MOVE_ACCEL;
    t->dashSpeed     = 26.0f;
    t->bushReveal    = BUSH_REVEAL_RANGE;
    t->fireReveal    = FIRE_REVEAL_TIME;
    t->playerRespawn = PLAYER_RESPAWN;
    t->enemyRespawn  = ENEMY_RESPAWN;
    t->timeScale     = 1.0f;
    t->superMult     = 1.0f;
    t->godMode       = false;
    t->infiniteAmmo  = false;
    t->showDebug     = false;
    t->modelCharacter = true;    // rigged model on the menu podium; toggle on WORLD tab
    t->toon           = true;    // the illustrated look; toggle on WORLD tab
    t->toonBands      = 3.0f;
    t->toonOutline    = 0.85f;
    t->stylePixelate  = 0.0f;
    t->stylePainterly = 0.0f;
    t->styleHalftone  = 0.0f;
    t->stylePosterize = 0.0f;
    t->styleGrain     = 0.0f;
    t->styleCA        = 0.0f;
    t->styleSaturation = 1.25f;  // replicates the original toon grade
    t->styleBrightness = 1.10f;
    t->styleVignette   = 0.85f;
    t->postFx        = true;
    t->bloom         = 0.85f;

    t->selectedKit       = CLASS_SHOTGUNNER;
    t->statWins = t->statLosses = t->statKos = 0;

    t->gemGrab           = true;
    t->teamSize          = 3;
    t->gemsToWin         = 10;
    t->gemCountdown      = 15.0f;
    t->gemVentInterval   = 6.5f;

    t->grassHeight       = 2.0f;    // level with a brawler, so cover really conceals
    t->windStrength      = 0.30f;
    t->windSpeed         = 1.7f;
    t->grassBendRadius   = 2.1f;
    t->grassBendStrength = 1.55f;   // blades part around you, which also keeps you visible
    t->concealDither     = 0.35f;   // ghosted, not erased: you still have to steer
    t->botMode       = BOT_STATIC;   // inert targets by default
    t->botCount      = 3;
    t->botKit        = CLASS_SHOTGUNNER;
    t->botMixedKits  = true;
}

//------------------------------------------------------------------------------------
static Projectile *AllocProjectile(World *w)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
        if (!w->projectiles[i].active) return &w->projectiles[i];
    return NULL;
}

Vector3 WeaponsArcLanding(const Brawler *b, float aimDist)
{
    const WeaponDef *def = &WEAPONS[b->cls];
    float d = Clamp(aimDist, 1.5f, def->range);
    return (Vector3){
        b->position.x + sinf(b->aimAngle) * d,
        0.0f,
        b->position.z + cosf(b->aimAngle) * d
    };
}

//------------------------------------------------------------------------------------
void WeaponsFire(World *w, int idx, bool super, float aimDist)
{
    Brawler *b = &w->brawlers[idx];
    const WeaponDef *def = &WEAPONS[b->cls];
    if (!b->alive) return;

    Color muzzle = super ? (Color){ 255, 225, 120, 255 } : TEAM_COLORS[b->team];

    // The dash super has no projectile at all: it hands control to brawler.c.
    if (super && def->sDash)
    {
        b->dashTimer = 0.45f;
        b->dashDir = (Vector3){ sinf(b->aimAngle), 0.0f, cosf(b->aimAngle) };
        b->dashHitMask = 0;
        b->revealTimer = w->tune.fireReveal;
        FxShake(w, 1.4f);
        FxMuzzleFlash(w, b->position, b->aimAngle, muzzle);
        return;
    }

    int pellets     = super ? def->sPellets : def->pellets;
    float spreadDeg = super ? def->sSpreadDeg : def->spreadDeg;
    float speed     = super ? def->sSpeed : def->speed;
    float range     = super ? def->sRange : def->range;
    int damage      = super ? def->sDamage : def->damage;
    float radius    = super ? def->sProjRadius : def->projRadius;
    bool piercing   = super ? def->sPiercing : false;

    if (pellets <= 0) return;

    Vector3 origin = b->position;
    origin.y = 0.75f;

    float spread = spreadDeg * DEG2RAD;

    for (int i = 0; i < pellets; i++)
    {
        // Fan the pellets evenly, then add a touch of scatter so it feels organic.
        float t = (pellets == 1) ? 0.5f : (i / (float)(pellets - 1));
        float offset = (t - 0.5f) * spread;
        offset += (GetRandomValue(-100, 100) / 100.0f) * spread * 0.06f;
        float angle = b->aimAngle + offset;

        Projectile *p = AllocProjectile(w);
        if (!p) return;

        *p = (Projectile){ 0 };
        p->position = origin;
        p->origin = origin;
        p->team = b->team;
        p->owner = idx;
        p->damage = damage;
        p->radius = radius;
        p->range = range;
        p->isSuper = super;
        p->piercing = piercing;
        p->breaksWalls = super;
        p->rangeScaled = def->rangeScaled;
        p->color = super ? (Color){ 255, 214, 92, 255 } : TEAM_COLORS[b->team];
        p->active = true;

        if (def->arcing)
        {
            Vector3 land = WeaponsArcLanding(b, aimDist);
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

    b->revealTimer = w->tune.fireReveal;
    FxMuzzleFlash(w, (Vector3){ origin.x + sinf(b->aimAngle) * 0.6f, origin.y, origin.z + cosf(b->aimAngle) * 0.6f },
                  b->aimAngle, muzzle);
    if (super) FxShake(w, 1.2f);
    else FxShake(w, 0.25f);
}

//------------------------------------------------------------------------------------
// Splash damage where an arcing shot lands, or where a super detonates.
//------------------------------------------------------------------------------------
static void Detonate(World *w, Projectile *p, Vector3 at)
{
    FxExplosion(w, at, p->radius, p->color);

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *t = &w->brawlers[i];
        if (!t->alive || t->team == p->team) continue;

        float d = Vector3Distance((Vector3){ t->position.x, 0, t->position.z }, (Vector3){ at.x, 0, at.z });
        if (d > p->radius) continue;

        // Falloff so the edge of the blast chips rather than deletes.
        float falloff = 1.0f - (d / p->radius) * 0.45f;
        int dmg = (int)(p->damage * falloff);

        BrawlerApplyDamage(w, i, dmg, p->owner, t->position);
        if (p->owner >= 0)
            BrawlerAwardSuper(w, p->owner, WEAPONS[w->brawlers[p->owner].cls].superPerHit * w->tune.superMult);
    }

    if (p->breaksWalls)
    {
        int broken = ArenaDamageRadius(&w->arena, at, p->radius, CRATE_HEALTH);
        if (broken > 0) FxCrateBreak(w, at);
    }
}

//------------------------------------------------------------------------------------
static bool ProjectileHitCheck(World *w, Projectile *p)
{
    // Walls first: an arcing shot flies over them entirely.
    if (!p->arcing && ArenaSolidAt(&w->arena, p->position.x, p->position.z))
    {
        if (p->breaksWalls)
        {
            if (ArenaDamageAt(&w->arena, p->position.x, p->position.z, p->damage))
                FxCrateBreak(w, p->position);
            // Supers punch through crates but still stop at solid walls.
            if (ArenaTypeAt(&w->arena, p->position.x, p->position.z) != TILE_WALL) return false;
        }
        FxImpact(w, p->position, p->color, 6);
        return true;
    }

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *t = &w->brawlers[i];
        if (!t->alive || t->team == p->team) continue;
        if (p->hitMask & (1 << i)) continue;

        float dx = t->position.x - p->position.x;
        float dz = t->position.z - p->position.z;
        float reach = BRAWLER_RADIUS + p->radius;
        if (dx * dx + dz * dz > reach * reach) continue;

        int dmg = p->damage;
        if (p->rangeScaled)
        {
            float ratio = Clamp(p->traveled / p->range, 0.0f, 1.0f);
            dmg = (int)(p->damage * (0.5f + 0.5f * ratio));
        }

        BrawlerApplyDamage(w, i, dmg, p->owner, p->position);
        if (p->owner >= 0)
            BrawlerAwardSuper(w, p->owner, WEAPONS[w->brawlers[p->owner].cls].superPerHit * w->tune.superMult);

        FxImpact(w, p->position, p->color, 8);
        p->hitMask |= (1 << i);

        if (!p->piercing) return true;
    }
    return false;
}

void ProjectilesUpdate(World *w, float dt)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
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

            if (GetRandomValue(0, 100) < 45)
            {
                Color trail = p->color;
                trail.a = 150;
                FxSpawnParticle(w, p->position, (Vector3){ 0, 0, 0 }, trail, 0.22f, 0.11f, PARTICLE_SMOKE);
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

        if (GetRandomValue(0, 100) < 30)
        {
            Color trail = p->color;
            trail.a = 140;
            FxSpawnParticle(w, p->position, (Vector3){ 0, 0, 0 }, trail, 0.13f, 0.07f, PARTICLE_MUZZLE);
        }
    }
}

void ProjectilesDraw(World *w)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *p = &w->projectiles[i];
        if (!p->active) continue;

        if (p->arcing)
        {
            DrawSphere(p->position, 0.32f, p->color);
            DrawSphere(p->position, 0.20f, WHITE);

            // Landing marker so the splash is readable before it happens.
            Color ring = p->color;
            ring.a = 90;
            DrawCylinder((Vector3){ p->arcEnd.x, 0.02f, p->arcEnd.z }, p->radius, p->radius, 0.02f, 20, ring);
            DrawCylinderWires((Vector3){ p->arcEnd.x, 0.03f, p->arcEnd.z }, p->radius, p->radius, 0.02f, 20, p->color);
        }
        else
        {
            float len = p->isSuper ? 0.9f : 0.55f;
            Vector3 dir = Vector3Normalize(p->velocity);
            Vector3 tail = Vector3Subtract(p->position, Vector3Scale(dir, len));

            Color glow = p->color;
            glow.a = 130;
            DrawCylinderEx(tail, p->position, p->radius * 0.55f, p->radius, 8, glow);
            DrawSphere(p->position, p->radius, p->color);
            DrawSphere(p->position, p->radius * 0.55f, WHITE);
        }
    }
}
