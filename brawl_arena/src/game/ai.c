#include "ai.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "gems.h"
#include "raymath.h"
#include "game_random.h"
#include "content_catalog.h"
#include <math.h>

//------------------------------------------------------------------------------------
// Cheap steering: if the way ahead is blocked, fan out to either side until a probe
// comes back clear. Enough to stop bots grinding into wall corners.
//------------------------------------------------------------------------------------
static Vector3 AvoidSteer(GameContext w, Vector3 pos, Vector3 dir, float side)
{
    if (Vector3Length(dir) < 0.001f) return dir;
    dir = Vector3Normalize(dir);

    Vector3 probe = Vector3Add(pos, Vector3Scale(dir, w.tuning->aiProbeAhead));
    if (!ArenaSolidAt(&w.session->arena, probe.x, probe.z)) return dir;

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
            if (!ArenaSolidAt(&w.session->arena, p.x, p.z)) return cand;
        }
    }
    return dir;
}

//------------------------------------------------------------------------------------
static Vector3 PickWanderPoint(GameContext w, Vector3 from)
{
    for (int attempt = 0; attempt < 24; attempt++)
    {
        float a = GameRandomInt(&w.session->random, 0, 628) / 100.0f;
        float r = GameRandomInt(&w.session->random, 6, 20);
        Vector3 p = { from.x + sinf(a) * r, 0.0f, from.z + cosf(a) * r };

        if (!ArenaSolidAt(&w.session->arena, p.x, p.z) &&
            ArenaLineOfSight(&w.session->arena, from, p))
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
        float mainSpeed = (mainAbility->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
                           mainAbility->behavior == ABILITY_BEHAVIOR_LOB)
                        ? mainAbility->data.projectile.speed : 0.0f;

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
                        b->moveIntent = AvoidSteer(w, b->position, toAlly, b->strafeDir);
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
                Vector3 toGem = Vector3Subtract(w.session->gems[gemIdx].position, b->position);
                toGem.y = 0.0f;
                b->moveIntent = AvoidSteer(w, b->position, toGem, b->strafeDir);
                if (Vector3Length(b->velocity) > 0.4f)
                    b->aimAngle = atan2f(b->velocity.x, b->velocity.z);
                continue;
            }

            if (b->aiTimer <= 0.0f || Vector3Distance(b->position, b->aiWander) < 2.0f)
            {
                b->aiWander = PickWanderPoint(w, b->position);
                b->aiTimer = 3.0f + GameRandomInt(&w.session->random, 0, 200) / 100.0f;
            }

            Vector3 dir = Vector3Subtract(b->aiWander, b->position);
            dir.y = 0.0f;
            b->moveIntent = Vector3Scale(AvoidSteer(w, b->position, dir, b->strafeDir), 0.7f);

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
                Vector3 toBush = Vector3Subtract(bush, b->position);
                toBush.y = 0.0f;
                if (Vector3Length(toBush) > 0.6f)
                    away = Vector3Normalize(Vector3Add(Vector3Scale(away, 0.5f), Vector3Normalize(toBush)));
            }

            b->moveIntent = AvoidSteer(w, b->position, away, b->strafeDir);

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
                Vector3 toGem = Vector3Subtract(w.session->gems[gemIdx].position, b->position);
                toGem.y = 0.0f;
                b->moveIntent = AvoidSteer(w, b->position, toGem, b->strafeDir);
            }
            else b->moveIntent = AvoidSteer(w, b->position, toTarget, b->strafeDir);
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
