#include "ai.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "raymath.h"
#include <math.h>

#define RETREAT_HEALTH 0.30f
#define PROBE_AHEAD 1.6f

//------------------------------------------------------------------------------------
// Cheap steering: if the way ahead is blocked, fan out to either side until a probe
// comes back clear. Enough to stop bots grinding into wall corners.
//------------------------------------------------------------------------------------
static Vector3 AvoidSteer(World *w, Vector3 pos, Vector3 dir, float side)
{
    if (Vector3Length(dir) < 0.001f) return dir;
    dir = Vector3Normalize(dir);

    Vector3 probe = Vector3Add(pos, Vector3Scale(dir, PROBE_AHEAD));
    if (!ArenaSolidAt(&w->arena, probe.x, probe.z)) return dir;

    const float angles[] = { 40.0f, 75.0f, 110.0f, 145.0f, 180.0f };
    float base = atan2f(dir.x, dir.z);

    for (int i = 0; i < 5; i++)
    {
        for (int s = 0; s < 2; s++)
        {
            float sign = (s == 0) ? side : -side;
            float a = base + sign * angles[i] * DEG2RAD;
            Vector3 cand = { sinf(a), 0.0f, cosf(a) };
            Vector3 p = Vector3Add(pos, Vector3Scale(cand, PROBE_AHEAD));
            if (!ArenaSolidAt(&w->arena, p.x, p.z)) return cand;
        }
    }
    return dir;
}

//------------------------------------------------------------------------------------
static Vector3 PickWanderPoint(World *w, Vector3 from)
{
    for (int attempt = 0; attempt < 24; attempt++)
    {
        float a = GetRandomValue(0, 628) / 100.0f;
        float r = GetRandomValue(6, 20);
        Vector3 p = { from.x + sinf(a) * r, 0.0f, from.z + cosf(a) * r };

        if (!ArenaSolidAt(&w->arena, p.x, p.z) &&
            ArenaLineOfSight(&w->arena, from, p))
            return p;
    }
    return from;
}

// Nearest bush tile within a small window, for retreating into cover.
static bool FindNearbyBush(World *w, Vector3 from, Vector3 *out)
{
    int cx = ArenaTileX(from.x), cz = ArenaTileZ(from.z);
    float best = 1e9f;
    bool found = false;

    for (int tz = cz - 6; tz <= cz + 6; tz++)
    {
        for (int tx = cx - 6; tx <= cx + 6; tx++)
        {
            if (!ArenaInBounds(tx, tz)) continue;
            if (w->arena.tiles[tz][tx].type != TILE_BUSH) continue;

            Vector3 c = ArenaTileCenter(tx, tz);
            float d = Vector3Distance(from, c);
            if (d < best) { best = d; *out = c; found = true; }
        }
    }
    return found;
}

//------------------------------------------------------------------------------------
void AIUpdate(World *w, float dt)
{
    BotMode mode = w->tune.botMode;

    for (int i = 0; i < w->brawlerCount; i++)
    {
        Brawler *b = &w->brawlers[i];
        if (b->isPlayer || !b->alive) continue;
        if (b->dashTimer > 0.0f) continue;

        // Static bots are inert targets: they hold position and never shoot back.
        if (mode == BOT_STATIC)
        {
            b->moveIntent = (Vector3){ 0 };
            b->aiState = AI_IDLE;
            b->aiTarget = -1;
            continue;
        }

        const WeaponDef *def = &WEAPONS[b->cls];

        b->aiTimer -= dt;
        b->aiReactTimer -= dt;

        // Roaming bots wander but never acquire a target, so they never open fire.
        int target = -1;
        if (mode == BOT_FIGHT)
        {
            // Re-acquire a target if the current one is gone or has broken line of sight.
            target = b->aiTarget;
            if (target < 0 || !BrawlerCanSee(w, i, target))
                target = BrawlerNearestVisibleEnemy(w, i);
        }
        b->aiTarget = target;

        float healthRatio = (float)b->health / (float)b->maxHealth;

        //--- No target: patrol -----------------------------------------------------
        if (target < 0)
        {
            b->aiState = AI_IDLE;

            if (b->aiTimer <= 0.0f || Vector3Distance(b->position, b->aiWander) < 2.0f)
            {
                b->aiWander = PickWanderPoint(w, b->position);
                b->aiTimer = 3.0f + GetRandomValue(0, 200) / 100.0f;
            }

            Vector3 dir = Vector3Subtract(b->aiWander, b->position);
            dir.y = 0.0f;
            b->moveIntent = Vector3Scale(AvoidSteer(w, b->position, dir, b->strafeDir), 0.7f);

            if (Vector3Length(b->velocity) > 0.4f)
                b->aimAngle = atan2f(b->velocity.x, b->velocity.z);
            continue;
        }

        //--- Target acquired -------------------------------------------------------
        Brawler *t = &w->brawlers[target];
        Vector3 toTarget = Vector3Subtract(t->position, b->position);
        toTarget.y = 0.0f;
        float dist = Vector3Length(toTarget);

        // Lead the shot, since every projectile has travel time.
        float travel = (def->speed > 0.1f) ? (dist / def->speed) : 0.0f;
        Vector3 predicted = Vector3Add(t->position, Vector3Scale(t->velocity, travel * 0.75f));
        Vector3 aimVec = Vector3Subtract(predicted, b->position);

        float aimError = (GetRandomValue(-6, 6) / 100.0f);
        b->aimAngle = atan2f(aimVec.x, aimVec.z) + aimError;
        float aimDist = Vector3Length((Vector3){ aimVec.x, 0.0f, aimVec.z });

        //--- Retreat when hurt -----------------------------------------------------
        if (healthRatio < RETREAT_HEALTH)
        {
            b->aiState = AI_RETREAT;

            Vector3 away = Vector3Normalize(Vector3Negate(toTarget));
            Vector3 bush;
            if (FindNearbyBush(w, b->position, &bush))
            {
                Vector3 toBush = Vector3Subtract(bush, b->position);
                toBush.y = 0.0f;
                if (Vector3Length(toBush) > 0.6f)
                    away = Vector3Normalize(Vector3Add(Vector3Scale(away, 0.5f), Vector3Normalize(toBush)));
            }

            b->moveIntent = AvoidSteer(w, b->position, away, b->strafeDir);

            // Still fights back, just less eagerly.
            if (b->aiReactTimer <= 0.0f && dist < def->range * 0.9f)
            {
                if (BrawlerTryAttack(w, i, aimDist))
                    b->aiReactTimer = 0.35f + GetRandomValue(0, 30) / 100.0f;
            }
            continue;
        }

        //--- Close the gap ---------------------------------------------------------
        float idealRange = def->range * 0.62f;

        if (dist > def->range * 0.92f)
        {
            b->aiState = AI_CHASE;
            b->moveIntent = AvoidSteer(w, b->position, toTarget, b->strafeDir);
        }
        else
        {
            b->aiState = AI_ATTACK;

            // Hold the preferred range while circling.
            Vector3 forward = Vector3Normalize(toTarget);
            Vector3 strafe = { forward.z * b->strafeDir, 0.0f, -forward.x * b->strafeDir };

            float adjust = (dist - idealRange) / idealRange;
            adjust = Clamp(adjust, -1.0f, 1.0f);

            Vector3 move = Vector3Add(Vector3Scale(forward, adjust * 0.8f), Vector3Scale(strafe, 0.85f));
            b->moveIntent = AvoidSteer(w, b->position, move, b->strafeDir);

            if (b->aiTimer <= 0.0f)
            {
                b->strafeDir = -b->strafeDir;
                b->aiTimer = 1.2f + GetRandomValue(0, 150) / 100.0f;
            }

            // Supers go off when they will actually connect.
            bool superInRange = def->sDash ? (dist < 9.0f) : (dist < def->sRange * 0.9f);
            if (b->superCharge >= 1.0f && superInRange && b->aiReactTimer <= 0.0f)
            {
                if (BrawlerTrySuper(w, i, aimDist))
                {
                    b->aiReactTimer = 0.6f;
                    continue;
                }
            }

            if (b->aiReactTimer <= 0.0f)
            {
                if (BrawlerTryAttack(w, i, aimDist))
                    b->aiReactTimer = 0.12f + GetRandomValue(0, 22) / 100.0f;
            }
        }
    }
}
