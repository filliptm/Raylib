#include "ai.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "gems.h"
#include "raymath.h"
#include "game_random.h"
#include "content_catalog.h"
#include <math.h>

#define AI_ROUTE_CLEARANCE (BRAWLER_RADIUS + 0.05f)

//------------------------------------------------------------------------------------
// Short-range steering uses the same circle body as movement. Point probes incorrectly
// marked routes beside corners as clear even when a brawler could not fit there.
//------------------------------------------------------------------------------------
static Vector3 AvoidSteer(GameContext w, Vector3 pos, Vector3 dir, float side)
{
    if (Vector3Length(dir) < 0.001f) return dir;
    dir = Vector3Normalize(dir);

    Vector3 probe = Vector3Add(pos, Vector3Scale(dir, w.tuning->aiProbeAhead));
    if (ArenaSweepCircleClear(
            &w.session->arena, pos, probe, BRAWLER_RADIUS)) return dir;

    const float angles[] = { 40.0f, 75.0f, 110.0f, 145.0f, 180.0f };
    float base = atan2f(dir.x, dir.z);

    for (int i = 0; i < 5; i++)
    {
        for (int s = 0; s < 2; s++)
        {
            float sign = (s == 0) ? side : -side;
            float a = base + sign * angles[i] * DEG2RAD;
            Vector3 cand = { sinf(a), 0.0f, cosf(a) };
            Vector3 p = Vector3Add(pos, Vector3Scale(cand, w.tuning->aiProbeAhead));
            if (ArenaSweepCircleClear(
                    &w.session->arena, pos, p, BRAWLER_RADIUS)) return cand;
        }
    }
    return dir;
}

static bool NavigableCell(const Arena *arena, int tx, int tz)
{
    if (!ArenaInBounds(arena, tx, tz)) return false;
    TileType type = arena->tiles[tz][tx].type;
    if (type == TILE_WALL || type == TILE_CRATE) return false;
    return ArenaCircleClear(
        arena, ArenaTileCenter(arena, tx, tz), AI_ROUTE_CLEARANCE);
}

Vector3 AINavigationDirection(GameContext w, Vector3 from, Vector3 goal, float side)
{
    from.y = 0.0f;
    goal.y = 0.0f;
    Vector3 direct = Vector3Subtract(goal, from);
    direct.y = 0.0f;
    if (Vector3Length(direct) < 0.001f) return (Vector3){ 0 };

    if (ArenaSweepCircleClear(
            &w.session->arena, from, goal, BRAWLER_RADIUS))
        return Vector3Normalize(direct);

    const Arena *arena = &w.session->arena;
    int startX = ArenaTileX(arena, from.x);
    int startZ = ArenaTileZ(arena, from.z);
    if (!ArenaInBounds(arena, startX, startZ))
        return AvoidSteer(w, from, direct, side);

    // If a target lies too close to cover, route to the closest body-clear tile. Combat
    // and gem pickup radii can finish the interaction without demanding an invalid pose.
    int goalX = -1, goalZ = -1;
    float closest = 1e30f;
    for (int tz = 0; tz < arena->height; tz++)
    {
        for (int tx = 0; tx < arena->width; tx++)
        {
            if (!NavigableCell(arena, tx, tz)) continue;
            Vector3 center = ArenaTileCenter(arena, tx, tz);
            float dx = center.x - goal.x;
            float dz = center.z - goal.z;
            float distanceSquared = dx*dx + dz*dz;
            if (distanceSquared >= closest) continue;
            closest = distanceSquared;
            goalX = tx;
            goalZ = tz;
        }
    }
    if (goalX < 0)
        return AvoidSteer(w, from, direct, side);

    short previous[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH];
    short queueX[MAX_ARENA_WIDTH*MAX_ARENA_HEIGHT];
    short queueZ[MAX_ARENA_WIDTH*MAX_ARENA_HEIGHT];
    for (int tz = 0; tz < arena->height; tz++)
        for (int tx = 0; tx < arena->width; tx++)
            previous[tz][tx] = -1;

    int head = 0, tail = 0;
    int startIndex = startZ*arena->width + startX;
    previous[startZ][startX] = (short)startIndex;
    queueX[tail] = (short)startX;
    queueZ[tail++] = (short)startZ;

    static const int DX[4] = { 1, 0, -1, 0 };
    static const int DZ[4] = { 0, 1, 0, -1 };
    int directionOffset = side < 0.0f ? 0 : 2;

    while (head < tail && previous[goalZ][goalX] < 0)
    {
        int tx = queueX[head];
        int tz = queueZ[head++];
        int currentIndex = tz*arena->width + tx;

        for (int step = 0; step < 4; step++)
        {
            int direction = (step + directionOffset) & 3;
            int nextX = tx + DX[direction];
            int nextZ = tz + DZ[direction];
            if (!NavigableCell(arena, nextX, nextZ) ||
                previous[nextZ][nextX] >= 0) continue;
            previous[nextZ][nextX] = (short)currentIndex;
            queueX[tail] = (short)nextX;
            queueZ[tail++] = (short)nextZ;
        }
    }

    if (previous[goalZ][goalX] < 0)
        return AvoidSteer(w, from, direct, side);

    int stepX = goalX;
    int stepZ = goalZ;
    int stepIndex = stepZ*arena->width + stepX;
    while (previous[stepZ][stepX] != startIndex &&
           previous[stepZ][stepX] != stepIndex)
    {
        stepIndex = previous[stepZ][stepX];
        stepX = stepIndex % arena->width;
        stepZ = stepIndex / arena->width;
    }

    Vector3 waypoint = ArenaTileCenter(arena, stepX, stepZ);
    Vector3 toWaypoint = Vector3Subtract(waypoint, from);
    toWaypoint.y = 0.0f;
    return AvoidSteer(w, from, toWaypoint, side);
}

//------------------------------------------------------------------------------------
static Vector3 PickWanderPoint(GameContext w, Vector3 from)
{
    for (int attempt = 0; attempt < 24; attempt++)
    {
        float a = GameRandomInt(&w.session->random, 0, 628) / 100.0f;
        float r = GameRandomInt(&w.session->random, 6, 20);
        Vector3 p = { from.x + sinf(a) * r, 0.0f, from.z + cosf(a) * r };

        if (ArenaCircleClear(&w.session->arena, p, BRAWLER_RADIUS) &&
            ArenaSweepCircleClear(
                &w.session->arena, from, p, BRAWLER_RADIUS))
            return p;
    }
    return from;
}

// Nearest bush tile within a small window, for retreating into cover.
static bool FindNearbyBush(GameContext w, Vector3 from, Vector3 *out)
{
    const Arena *arena = &w.session->arena;
    int cx = ArenaTileX(arena, from.x), cz = ArenaTileZ(arena, from.z);
    float best = 1e9f;
    bool found = false;

    for (int tz = cz - 6; tz <= cz + 6; tz++)
    {
        for (int tx = cx - 6; tx <= cx + 6; tx++)
        {
            if (!ArenaInBounds(arena, tx, tz)) continue;
            if (arena->tiles[tz][tx].type != TILE_BUSH) continue;

            Vector3 c = ArenaTileCenter(arena, tx, tz);
            float d = Vector3Distance(from, c);
            if (d < best) { best = d; *out = c; found = true; }
        }
    }
    return found;
}

static bool TryTacticalMobility(GameContext w, int idx, Vector3 direction)
{
    Brawler *b = &w.session->brawlers[idx];
    const AbilityDefinition *secondary =
        ContentSecondaryAbility(w.content, b->cls);
    if (!secondary || secondary->behavior != ABILITY_BEHAVIOR_DASH)
        return false;
    if (!BrawlerTryMobility(w, idx, direction)) return false;

    b->aiReactTimer = fmaxf(b->aiReactTimer, 0.18f);
    return true;
}

//------------------------------------------------------------------------------------
void AIUpdate(GameContext w, float dt)
{
    // A real match needs both sides playing. In the tuning sandbox the STATIC/ROAM
    // modes are the whole point, so they win there.
    BotMode mode = (!w.session->sandbox && w.tuning->gemGrab) ? BOT_FIGHT : w.tuning->botMode;

    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *b = &w.session->brawlers[i];
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

        const AbilityDefinition *mainAbility = ContentMainAbility(w.content, b->cls);
        const AbilityDefinition *superAbility = ContentSuperAbility(w.content, b->cls);
        float mainSpeed = 0.0f;
        if (mainAbility->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
            mainAbility->behavior == ABILITY_BEHAVIOR_LOB)
            mainSpeed = mainAbility->data.projectile.speed;
        else if (mainAbility->behavior == ABILITY_BEHAVIOR_RETURNING)
            mainSpeed = mainAbility->data.returning.outboundSpeed;

        b->aiTimer -= dt;
        b->aiReactTimer -= dt;

        bool shieldThreat = BrawlerProjectileThreat(w, i, 0.48f);
        if (b->shieldActive && !shieldThreat)
            BrawlerReleaseShield(w, i);
        else if (!b->shieldActive && !shieldThreat &&
                 b->shieldRearmRequired)
            BrawlerReleaseShield(w, i);
        if (mode == BOT_FIGHT && shieldThreat && !b->shieldActive)
        {
            Vector3 facing = {
                sinf(b->aimAngle), 0.0f, cosf(b->aimAngle)
            };
            if (BrawlerTrySecondary(w, i, facing))
            {
                b->aiReactTimer = fmaxf(b->aiReactTimer, 0.18f);
                continue;
            }
        }

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

        // A Guardian aims its support areas at hurt teammates before returning to
        // combat. Resonance faces the ally it needs to catch; the old radial-heal
        // behavior remains supported for any tuned/custom kit that still uses it.
        int supportTarget = -1;
        if (mode == BOT_FIGHT && mainAbility->healing > 0)
        {
            supportTarget = BrawlerMostWoundedAlly(w, i, mainAbility->range);

            bool teamNeedsSuper =
                                  superAbility->behavior == ABILITY_BEHAVIOR_HEALING_BURST &&
                                  healthRatio < w.tuning->aiSupportSuperHealth;
            int nearbyHurt = BrawlerMostWoundedAlly(w, i, superAbility->range);
            if (nearbyHurt >= 0)
            {
                Brawler *ally = &w.session->brawlers[nearbyHurt];
                teamNeedsSuper = teamNeedsSuper ||
                                 (float)ally->health/(float)ally->maxHealth <
                                 w.tuning->aiSupportSuperHealth;
            }

            if ((superAbility->behavior == ABILITY_BEHAVIOR_HEALING_BURST ||
                 superAbility->behavior == ABILITY_BEHAVIOR_SOUND_WAVE) && teamNeedsSuper &&
                b->superCharge >= 1.0f && b->aiReactTimer <= 0.0f)
            {
                if (superAbility->behavior == ABILITY_BEHAVIOR_SOUND_WAVE && nearbyHurt >= 0)
                {
                    Vector3 superAim = Vector3Subtract(w.session->brawlers[nearbyHurt].position,
                                                       b->position);
                    b->aimAngle = atan2f(superAim.x, superAim.z);
                }
                if (BrawlerTrySuper(w, i, 0.0f))
                    b->aiReactTimer = 0.6f;
            }

            if (supportTarget >= 0)
            {
                Brawler *ally = &w.session->brawlers[supportTarget];
                float allyRatio = (float)ally->health/(float)ally->maxHealth;
                if (allyRatio < w.tuning->aiSupportHealth)
                {
                    Vector3 toAlly = Vector3Subtract(ally->position, b->position);
                    toAlly.y = 0.0f;
                    float allyDist = Vector3Length(toAlly);
                    float travel = (mainSpeed > 0.1f) ? allyDist/mainSpeed : 0.0f;
                    Vector3 predicted = Vector3Add(ally->position,
                                                  Vector3Scale(ally->velocity, travel*0.55f));
                    Vector3 aim = Vector3Subtract(predicted, b->position);
                    b->aimAngle = atan2f(aim.x, aim.z);

                    if (allyDist > mainAbility->range*0.88f)
                    {
                        b->aiState = AI_CHASE;
                        b->moveIntent = AINavigationDirection(
                            w, b->position, ally->position, b->strafeDir);
                    }
                    else
                    {
                        b->aiState = AI_ATTACK;
                        b->moveIntent = (Vector3){ 0 };
                        if (b->aiReactTimer <= 0.0f && BrawlerTryAttack(w, i, allyDist))
                            b->aiReactTimer = 0.16f + GameRandomInt(&w.session->random, 0, 18)/100.0f;
                    }
                    continue;
                }
            }
        }

        // In Gem Grab a loose gem on the floor is worth breaking off for.
        int gemIdx = -1;
        float gemDist = 0.0f;
        if (w.tuning->gemGrab && w.session->match.phase != MATCH_OVER)
        {
            gemIdx = GemsNearestLoose(w, b->position, 17.0f);
            if (gemIdx >= 0)
                gemDist = Vector3Distance(b->position, w.session->gems[gemIdx].position);
        }

        //--- No target: fetch a gem, else patrol -----------------------------------
        if (target < 0)
        {
            b->aiState = AI_IDLE;

            if (gemIdx >= 0)
            {
                b->moveIntent = AINavigationDirection(
                    w, b->position, w.session->gems[gemIdx].position,
                    b->strafeDir);
                if (Vector3Length(b->velocity) > 0.4f)
                    b->aimAngle = atan2f(b->velocity.x, b->velocity.z);
                continue;
            }

            if (b->aiTimer <= 0.0f || Vector3Distance(b->position, b->aiWander) < 2.0f)
            {
                b->aiWander = PickWanderPoint(w, b->position);
                b->aiTimer = 3.0f + GameRandomInt(&w.session->random, 0, 200) / 100.0f;
            }

            b->moveIntent = Vector3Scale(
                AINavigationDirection(
                    w, b->position, b->aiWander, b->strafeDir),
                0.7f);

            if (Vector3Length(b->velocity) > 0.4f)
                b->aimAngle = atan2f(b->velocity.x, b->velocity.z);
            continue;
        }

        //--- Target acquired -------------------------------------------------------
        Brawler *t = &w.session->brawlers[target];
        Vector3 toTarget = Vector3Subtract(t->position, b->position);
        toTarget.y = 0.0f;
        float dist = Vector3Length(toTarget);

        // Lead the shot, since every projectile has travel time.
        float travel = (mainSpeed > 0.1f) ? (dist/mainSpeed) : 0.0f;
        Vector3 predicted = Vector3Add(t->position, Vector3Scale(t->velocity, travel * 0.75f));
        Vector3 aimVec = Vector3Subtract(predicted, b->position);

        float aimError = (GameRandomInt(&w.session->random, -6, 6) / 100.0f);
        b->aimAngle = atan2f(aimVec.x, aimVec.z) + aimError;
        float aimDist = Vector3Length((Vector3){ aimVec.x, 0.0f, aimVec.z });

        //--- Retreat when hurt -----------------------------------------------------
        if (healthRatio < w.tuning->aiRetreatHealth)
        {
            b->aiState = AI_RETREAT;

            Vector3 away = Vector3Normalize(Vector3Negate(toTarget));
            Vector3 bush;
            if (FindNearbyBush(w, b->position, &bush))
            {
                b->moveIntent = AINavigationDirection(
                    w, b->position, bush, b->strafeDir);
            }
            else
                b->moveIntent = AvoidSteer(
                    w, b->position, away, b->strafeDir);
            if (TryTacticalMobility(w, i, b->moveIntent)) continue;

            // Still fights back, just less eagerly.
            if (b->aiReactTimer <= 0.0f && dist < mainAbility->range*0.9f)
            {
                if (BrawlerTryAttack(w, i, aimDist))
                    b->aiReactTimer = 0.35f + GameRandomInt(&w.session->random, 0, 30) / 100.0f;
            }
            continue;
        }

        //--- Close the gap ---------------------------------------------------------
        float idealRange = mainAbility->range*0.62f;

        if (dist > mainAbility->range*0.92f)
        {
            b->aiState = AI_CHASE;

            // Out of weapon range anyway: grab the gem on the way rather than walking
            // past it to start a fight it cannot yet win.
            if (gemIdx >= 0 && gemDist < dist)
            {
                b->moveIntent = AINavigationDirection(
                    w, b->position, w.session->gems[gemIdx].position,
                    b->strafeDir);
            }
            else
                b->moveIntent = AINavigationDirection(
                    w, b->position, t->position, b->strafeDir);

            // Mobility kits spend their short cooldown closing meaningful gaps, while
            // retaining the charged super for its separate combat payoff.
            TryTacticalMobility(w, i, b->moveIntent);
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
                b->aiTimer = 1.2f + GameRandomInt(&w.session->random, 0, 150) / 100.0f;
            }

            // Supers go off when they will actually connect.
            bool superInRange =
                superAbility->behavior != ABILITY_BEHAVIOR_HEALING_BURST &&
                (superAbility->behavior == ABILITY_BEHAVIOR_DASH
                 ? dist < 9.0f : dist < superAbility->range*0.9f);
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
                    b->aiReactTimer = 0.12f + GameRandomInt(&w.session->random, 0, 22) / 100.0f;
            }
        }
    }
}
