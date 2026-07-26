#include "brawler.h"
#include "arena.h"
#include "weapons.h"
#include "effects.h"
#include "raymath.h"
#include <stdio.h>
#include <math.h>

void BrawlerSpawn(World *w, int idx, Team team, BrawlerClass cls, Vector3 pos, bool isPlayer)
{
    Brawler *b = &w->brawlers[idx];
    const WeaponDef *def = &WEAPONS[cls];

    *b = (Brawler){ 0 };
    b->position = pos;
    b->position.y = 0.0f;
    b->team = team;
    b->cls = cls;
    b->isPlayer = isPlayer;
    b->maxHealth = def->maxHealth;
    b->health = def->maxHealth;
    b->ammo = (float)MAX_AMMO;
    b->superCharge = 0.0f;
    b->alive = true;
    b->spawnScale = 0.0f;
    b->aimAngle = (team == TEAM_PLAYER) ? 0.0f : PI;
    b->moveFacing = b->aimAngle;
    b->renderYaw = b->aimAngle;
    b->shotYaw = b->aimAngle;
    b->bobPhase = GetRandomValue(0, 628) / 100.0f;
    b->aiTarget = -1;
    b->strafeDir = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
    b->aiWander = pos;

    FxImpact(w, pos, TEAM_COLORS[team], 10);
}

void BrawlerRespawn(World *w, int idx)
{
    Brawler *b = &w->brawlers[idx];
    Vector3 pos;

    if (b->team == TEAM_PLAYER)
    {
        pos = w->arena.playerSpawn;
    }
    else
    {
        int n = w->arena.enemySpawnCount;
        pos = w->arena.enemySpawns[GetRandomValue(0, n - 1)];
    }

    BrawlerSpawn(w, idx, b->team, b->cls, pos, b->isPlayer);
}

void BrawlerAwardSuper(World *w, int idx, float amount)
{
    if (idx < 0 || idx >= w->brawlerCount) return;
    Brawler *b = &w->brawlers[idx];
    if (!b->alive) return;

    b->superCharge += amount;
    if (b->superCharge > 1.0f) b->superCharge = 1.0f;
}

void BrawlerApplyDamage(World *w, int idx, int damage, int attacker, Vector3 hitPos)
{
    Brawler *b = &w->brawlers[idx];
    if (!b->alive || damage <= 0) return;

    // God mode still flashes and shows numbers, so feedback stays readable.
    if (b->isPlayer && w->tune.godMode)
    {
        b->hitFlash = 1.0f;
        return;
    }

    b->health -= damage;
    b->hitFlash = 1.0f;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", damage);
    FxFloatText(w, hitPos, buf, (attacker == w->playerIdx) ? (Color){ 255, 235, 140, 255 } : (Color){ 255, 150, 150, 255 });

    if (b->isPlayer) FxShake(w, 0.8f);

    if (b->health <= 0)
    {
        b->health = 0;
        b->alive = false;
        b->respawnTimer = b->isPlayer ? w->tune.playerRespawn : w->tune.enemyRespawn;
        b->dashTimer = 0.0f;

        FxDeathBurst(w, b->position, TEAM_COLORS[b->team]);

        if (attacker == w->playerIdx && !b->isPlayer)
        {
            w->kills++;
            FxFloatText(w, b->position, "KO!", (Color){ 255, 210, 80, 255 });
            // A takedown tops up the killer's super, the way a finished fight should feel.
            BrawlerAwardSuper(w, attacker, 0.20f);
        }
        if (b->isPlayer) w->deaths++;
    }
}

// Shortest-path interpolation between two angles, so turning never takes the long way
// round when a shot is fired behind the brawler.
static float AngleLerp(float from, float to, float t)
{
    float delta = to - from;
    while (delta > PI) delta -= 2.0f*PI;
    while (delta < -PI) delta += 2.0f*PI;
    return from + delta*t;
}

void BrawlerFaceShot(World *w, int idx, float yaw, float holdTime)
{
    Brawler *b = &w->brawlers[idx];
    b->shotYaw = yaw;
    if (holdTime > b->aimHold) b->aimHold = holdTime;
}

bool BrawlerTryAttack(World *w, int idx, float aimDist)
{
    Brawler *b = &w->brawlers[idx];
    if (!b->alive || b->attackCd > 0.0f || b->dashTimer > 0.0f) return false;

    bool freeAmmo = b->isPlayer && w->tune.infiniteAmmo;
    if (!freeAmmo && b->ammo < 1.0f) return false;
    if (!freeAmmo) b->ammo -= 1.0f;
    b->attackCd = WEAPONS[b->cls].cooldown;
    WeaponsFire(w, idx, false, aimDist);
    return true;
}

bool BrawlerTrySuper(World *w, int idx, float aimDist)
{
    Brawler *b = &w->brawlers[idx];
    if (!b->alive || b->superCharge < 1.0f || b->dashTimer > 0.0f) return false;

    if (!(b->isPlayer && w->tune.infiniteAmmo)) b->superCharge = 0.0f;
    b->attackCd = WEAPONS[b->cls].cooldown;
    WeaponsFire(w, idx, true, aimDist);
    return true;
}

bool BrawlerCanSee(World *w, int viewer, int target)
{
    if (viewer < 0 || target < 0) return false;
    Brawler *v = &w->brawlers[viewer];
    Brawler *t = &w->brawlers[target];
    if (!t->alive || !v->alive) return false;

    if (!ArenaLineOfSight(&w->arena, v->position, t->position)) return false;

    // Bushes hide you unless you just fired, or the viewer is right on top of you.
    if (t->inBush && t->revealTimer <= 0.0f)
    {
        float d = Vector3Distance(v->position, t->position);
        if (d > w->tune.bushReveal) return false;
    }
    return true;
}

int BrawlerNearestVisibleEnemy(World *w, int idx)
{
    Brawler *b = &w->brawlers[idx];
    int best = -1;
    float bestDist = 1e9f;

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *t = &w->brawlers[i];
        if (i == idx || !t->alive || t->team == b->team) continue;
        if (!BrawlerCanSee(w, idx, i)) continue;

        float d = Vector3Distance(b->position, t->position);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

//------------------------------------------------------------------------------------
static void UpdateDash(World *w, int idx, float dt)
{
    Brawler *b = &w->brawlers[idx];
    const WeaponDef *def = &WEAPONS[b->cls];

    b->dashTimer -= dt;

    Vector3 next = Vector3Add(b->position, Vector3Scale(b->dashDir, w->tune.dashSpeed * dt));

    // A charge smashes through crates rather than stopping dead on them.
    if (ArenaTypeAt(&w->arena, next.x, next.z) == TILE_CRATE)
    {
        if (ArenaDamageAt(&w->arena, next.x, next.z, CRATE_HEALTH))
            FxCrateBreak(w, (Vector3){ next.x, 0.6f, next.z });
    }

    Vector3 resolved = ArenaResolveCircle(&w->arena, next, BRAWLER_RADIUS);
    // Running into a solid wall ends the charge early.
    if (Vector3Distance(resolved, next) > 0.35f) b->dashTimer = 0.0f;
    b->position = resolved;

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *t = &w->brawlers[i];
        if (!t->alive || t->team == b->team) continue;
        if (b->dashHitMask & (1 << i)) continue;

        if (Vector3Distance(b->position, t->position) < BRAWLER_RADIUS * 2.2f)
        {
            BrawlerApplyDamage(w, i, def->sDamage, idx, t->position);
            b->dashHitMask |= (1 << i);

            // Knock the victim back so the charge reads as physical.
            Vector3 push = Vector3Scale(b->dashDir, 3.0f);
            Vector3 shoved = Vector3Add(t->position, push);
            t->position = ArenaResolveCircle(&w->arena, shoved, BRAWLER_RADIUS);
            FxImpact(w, t->position, (Color){ 255, 220, 120, 255 }, 12);
        }
    }

    FxSpawnParticle(w, b->position, (Vector3){ 0, 1.0f, 0 },
                    (Color){ 255, 200, 110, 180 }, 0.3f, 0.28f, PARTICLE_SMOKE);
}

//------------------------------------------------------------------------------------
void BrawlersUpdate(World *w, float dt)
{
    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        const WeaponDef *def = &WEAPONS[b->cls];

        if (!b->alive)
        {
            b->respawnTimer -= dt;
            if (b->respawnTimer <= 0.0f) BrawlerRespawn(w, i);
            continue;
        }

        // Timers
        if (b->attackCd > 0.0f) b->attackCd -= dt;
        if (b->hitFlash > 0.0f) b->hitFlash = fmaxf(0.0f, b->hitFlash - dt * 5.0f);
        if (b->revealTimer > 0.0f) b->revealTimer -= dt;
        if (b->aimHold > 0.0f) b->aimHold = fmaxf(0.0f, b->aimHold - dt);
        if (b->spawnScale < 1.0f) b->spawnScale = fminf(1.0f, b->spawnScale + dt * 4.5f);

        // Ammo regenerates continuously, one pip at a time.
        if (b->ammo < (float)MAX_AMMO)
        {
            b->ammo += dt / def->reloadPerAmmo;
            if (b->ammo > (float)MAX_AMMO) b->ammo = (float)MAX_AMMO;
        }

        if (b->dashTimer > 0.0f)
        {
            UpdateDash(w, i, dt);
            b->bobPhase += dt * 22.0f;
            b->renderYaw = AngleLerp(b->renderYaw, atan2f(b->dashDir.x, b->dashDir.z),
                                     Clamp(24.0f * dt, 0.0f, 1.0f));
            continue;
        }

        // Movement: accelerate toward the controller's intent, then resolve collisions.
        Vector3 desired = Vector3Scale(b->moveIntent, w->tune.moveSpeed);
        b->velocity.x = Lerp(b->velocity.x, desired.x, w->tune.moveAccel * dt);
        b->velocity.z = Lerp(b->velocity.z, desired.z, w->tune.moveAccel * dt);

        Vector3 next = Vector3Add(b->position, Vector3Scale(b->velocity, dt));
        next.y = 0.0f;
        b->position = ArenaResolveCircle(&w->arena, next, BRAWLER_RADIUS);

        // Keep brawlers from stacking on the same tile.
        for (int j = 0; j < w->brawlerCount; j++)
        {
            if (j == i) continue;
            Brawler *o = &w->brawlers[j];
            if (!o->alive) continue;

            float dx = b->position.x - o->position.x;
            float dz = b->position.z - o->position.z;
            float d2 = dx * dx + dz * dz;
            float minD = BRAWLER_RADIUS * 2.0f;

            if (d2 < minD * minD && d2 > 0.0001f)
            {
                float d = sqrtf(d2);
                float push = (minD - d) * 0.5f;
                b->position.x += (dx / d) * push;
                b->position.z += (dz / d) * push;
                b->position = ArenaResolveCircle(&w->arena, b->position, BRAWLER_RADIUS);
            }
        }

        float speed = Vector3Length(b->velocity);
        if (speed > 0.4f)
        {
            b->moveFacing = atan2f(b->velocity.x, b->velocity.z);
            b->bobPhase += dt * (8.0f + speed);
        }

        b->inBush = ArenaBushAt(&w->arena, b->position.x, b->position.z);

        //--- Visual facing -----------------------------------------------------
        // Priority: a shot just fired > deliberate aiming > direction of travel.
        // Snapping onto a shot is quick so it reads as a turn-and-fire; easing back to
        // the run direction is slower so it does not look twitchy.
        float targetYaw = b->renderYaw;
        float turnRate = 11.0f;

        if (b->aimHold > 0.0f)
        {
            targetYaw = b->shotYaw;
            turnRate = 26.0f;
        }
        else if (b->isPlayer && (w->charging || w->aimingSuper))
        {
            targetYaw = b->aimAngle;
            turnRate = 20.0f;
        }
        else if (!b->isPlayer && b->aiTarget >= 0)
        {
            targetYaw = b->aimAngle;
            turnRate = 14.0f;
        }
        else if (speed > 0.4f)
        {
            targetYaw = b->moveFacing;
        }

        b->renderYaw = AngleLerp(b->renderYaw, targetYaw, Clamp(turnRate * dt, 0.0f, 1.0f));
    }

    // Visibility is resolved from the player's side, for rendering and for the HUD.
    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];

        if (b->team == TEAM_PLAYER || b->isPlayer) { b->visible = true; continue; }
        if (!b->inBush || b->revealTimer > 0.0f)   { b->visible = true; continue; }

        b->visible = false;
        for (int j = 0; j < w->brawlerCount; j++)
        {
            Brawler *o = &w->brawlers[j];
            if (!o->alive || o->team != TEAM_PLAYER) continue;
            if (Vector3Distance(o->position, b->position) <= w->tune.bushReveal)
            {
                b->visible = true;
                break;
            }
        }
    }
}
