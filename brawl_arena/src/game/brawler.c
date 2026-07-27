#include "brawler.h"
#include "arena.h"
#include "weapons.h"
#include "game_events.h"
#include "gems.h"
#include "raymath.h"
#include "game_random.h"
#include "content_catalog.h"
#include <stdio.h>
#include <math.h>

void BrawlerSpawn(GameContext w, int idx, Team team, BrawlerClass cls, Vector3 pos, bool isPlayer)
{
    Brawler *b = &w.session->brawlers[idx];
    const CharacterDefinition *def = ContentCharacter(w.content, cls);

    *b = (Brawler){ 0 };
    b->position = pos;
    b->position.y = 0.0f;
    b->team = team;
    b->cls = cls;
    b->isPlayer = isPlayer;
    b->maxHealth = def->maxHealth;
    b->health = def->maxHealth;
    b->ammo = (float)def->maxAmmo;
    b->superCharge = 0.0f;
    b->alive = true;
    b->spawnScale = 0.0f;
    b->aimAngle = (team == TEAM_PLAYER) ? 0.0f : PI;
    b->moveFacing = b->aimAngle;
    b->renderYaw = b->aimAngle;
    b->shotYaw = b->aimAngle;
    b->dashAbility = -1;
    b->bobPhase = GameRandomInt(&w.session->random, 0, 628) / 100.0f;
    b->aiTarget = -1;
    b->strafeDir = (GameRandomInt(&w.session->random, 0, 1) == 0) ? -1.0f : 1.0f;
    b->aiWander = pos;

    GameEmitImpact(w.session, pos, TEAM_COLORS[team], 10);
}

void BrawlerRespawn(GameContext w, int idx)
{
    Brawler *b = &w.session->brawlers[idx];
    int slot = b->spawnSlot;
    bool player = b->isPlayer;
    Team team = b->team;
    BrawlerClass cls = b->cls;

    BrawlerSpawn(w, idx, team, cls, ArenaSpawnFor(&w.session->arena, team, slot), player);
    w.session->brawlers[idx].spawnSlot = slot;
}

void BrawlerAwardSuper(GameContext w, int idx, float amount)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return;
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive) return;

    b->superCharge += amount;
    if (b->superCharge > 1.0f) b->superCharge = 1.0f;
}

int BrawlerApplyDamage(GameContext w, int idx, int damage, int attacker, Vector3 hitPos)
{
    if (idx < 0 || idx >= w.session->brawlerCount || damage <= 0) return 0;
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive) return 0;

    // God mode still flashes and shows numbers, so feedback stays readable.
    if (b->isPlayer && w.tuning->godMode)
    {
        b->hitFlash = 1.0f;
        return 0;
    }

    int before = b->health;
    b->health -= damage;
    if (b->health < 0) b->health = 0;
    int removed = before - b->health;
    b->hitFlash = 1.0f;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", damage);
    GameEmitFloatText(w.session, hitPos, buf, (attacker == w.session->playerIdx) ? (Color){ 255, 235, 140, 255 } : (Color){ 255, 150, 150, 255 });

    if (b->health == 0)
    {
        b->alive = false;
        b->respawnTimer = b->isPlayer ? w.tuning->playerRespawn : w.tuning->enemyRespawn;
        b->dashTimer = 0.0f;
        b->dashAbility = -1;

        GameEmitDeath(w.session, b->position, TEAM_COLORS[b->team]);

        // Everything they were carrying goes back on the floor for anyone to take.
        GemsDropFrom(w, idx);

        if (attacker == w.session->playerIdx && !b->isPlayer)
        {
            w.session->kills++;
            GameEmitFloatText(w.session, b->position, "KO!", (Color){ 255, 210, 80, 255 });
            // A takedown tops up the killer's super, the way a finished fight should feel.
            BrawlerAwardSuper(w, attacker, 0.20f);
        }
        if (b->isPlayer) w.session->deaths++;
    }
    return removed;
}

int BrawlerApplyHealing(GameContext w, int idx, int amount, int healer, Vector3 hitPos)
{
    (void)healer;
    if (idx < 0 || idx >= w.session->brawlerCount || amount <= 0) return 0;

    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive || b->health >= b->maxHealth) return 0;

    int before = b->health;
    b->health += amount;
    if (b->health > b->maxHealth) b->health = b->maxHealth;
    int restored = b->health - before;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", restored);
    GameEmitFloatText(w.session, hitPos, buf, (Color){ 112, 255, 174, 255 });
    GameEmitImpact(w.session, hitPos, (Color){ 70, 244, 166, 255 }, 7);
    GameEmitLight(w.session, (Vector3){ hitPos.x, hitPos.y + 0.7f, hitPos.z },
                 (Color){ 70, 244, 166, 255 }, 1.7f, 0.18f);
    return restored;
}

void BrawlerApplyPulseStatus(GameContext w, int idx, Team sourceTeam, int source,
                             int damage, int healing, float duration, float tickRate)
{
    if (idx < 0 || idx >= w.session->brawlerCount || duration <= 0.0f || tickRate <= 0.0f) return;

    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive) return;

    StatusEffect *status = 0;
    for (int i = 0; i < MAX_STATUS_EFFECTS; i++)
    {
        if (b->statuses[i].active &&
            b->statuses[i].source == source &&
            b->statuses[i].sourceTeam == sourceTeam)
        {
            status = &b->statuses[i];
            break;
        }
        if (!status && !b->statuses[i].active) status = &b->statuses[i];
    }
    if (!status) status = &b->statuses[0];
    *status = (StatusEffect){
        .remaining = duration,
        .tickTimer = tickRate,
        .tickRate = tickRate,
        .damage = damage,
        .healing = healing,
        .source = source,
        .sourceTeam = sourceTeam,
        .active = true
    };
}

static void UpdateStatuses(GameContext w, int idx, float dt)
{
    Brawler *b = &w.session->brawlers[idx];
    for (int i = 0; i < MAX_STATUS_EFFECTS; i++)
    {
        StatusEffect *status = &b->statuses[i];
        if (!status->active) continue;

        // Only time inside the status lifespan advances the pulse clock, preserving a
        // final scheduled pulse when a frame crosses the exact expiry boundary.
        float activeDt = fminf(dt, status->remaining);
        status->remaining -= dt;
        status->tickTimer -= activeDt;
        while (status->tickTimer <= 0.0001f && b->alive)
        {
            Vector3 hit = { b->position.x, 0.75f, b->position.z };
            if (b->team == status->sourceTeam)
                BrawlerApplyHealing(w, idx, status->healing, status->source, hit);
            else
                BrawlerApplyDamage(w, idx, status->damage, status->source, hit);
            status->tickTimer += status->tickRate;
        }
        if (status->remaining <= 0.0f) *status = (StatusEffect){ 0 };
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

void BrawlerFaceShot(GameContext w, int idx, float yaw, float holdTime)
{
    Brawler *b = &w.session->brawlers[idx];
    b->shotYaw = yaw;
    if (holdTime > b->aimHold) b->aimHold = holdTime;
}

bool BrawlerTryAttack(GameContext w, int idx, float aimDist)
{
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive || b->attackCd > 0.0f || b->dashTimer > 0.0f) return false;

    bool freeAmmo = b->isPlayer && w.tuning->infiniteAmmo;
    if (!freeAmmo && b->ammo < 1.0f) return false;
    if (!freeAmmo) b->ammo -= 1.0f;
    b->attackCd = ContentMainAbility(w.content, b->cls)->cooldown;
    WeaponsFire(w, idx, false, aimDist);
    return true;
}

bool BrawlerTrySuper(GameContext w, int idx, float aimDist)
{
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive || b->superCharge < 1.0f || b->dashTimer > 0.0f) return false;

    if (!(b->isPlayer && w.tuning->infiniteAmmo)) b->superCharge = 0.0f;
    b->attackCd = ContentMainAbility(w.content, b->cls)->cooldown;
    WeaponsFire(w, idx, true, aimDist);
    return true;
}

bool BrawlerTryMobility(GameContext w, int idx, Vector3 direction)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return false;

    Brawler *b = &w.session->brawlers[idx];
    const CharacterDefinition *character = ContentCharacter(w.content, b->cls);
    const AbilityDefinition *ability = ContentMobilityAbility(w.content, b->cls);
    if (!b->alive || !character || !ability ||
        ability->behavior != ABILITY_BEHAVIOR_DASH ||
        b->mobilityCooldown > 0.0f || b->dashTimer > 0.0f)
        return false;

    direction.y = 0.0f;
    if (Vector3Length(direction) < 0.001f) return false;

    b->dashTimer = ability->data.dash.duration;
    b->dashDir = Vector3Normalize(direction);
    b->dashAbility = character->mobilityAbility;
    b->dashHitMask = 0;
    b->mobilityCooldown = ability->cooldown;
    b->velocity = (Vector3){ 0 };
    b->revealTimer = w.tuning->fireReveal;
    GameEmitMuzzle(w.session, b->position,
                   atan2f(b->dashDir.x, b->dashDir.z),
                   (Color){ 92, 220, 255, 255 });
    GameEmitLight(w.session, (Vector3){ b->position.x, 0.8f, b->position.z },
                  (Color){ 92, 220, 255, 255 }, 2.2f, 0.16f);
    return true;
}

bool BrawlerCanSee(GameContext w, int viewer, int target)
{
    if (viewer < 0 || target < 0) return false;
    Brawler *v = &w.session->brawlers[viewer];
    Brawler *t = &w.session->brawlers[target];
    if (!t->alive || !v->alive) return false;

    if (!ArenaLineOfSight(&w.session->arena, v->position, t->position)) return false;

    // Bushes hide you unless you just fired, or the viewer is right on top of you.
    if (t->inBush && t->revealTimer <= 0.0f)
    {
        float d = Vector3Distance(v->position, t->position);
        if (d > w.tuning->bushReveal) return false;
    }
    return true;
}

int BrawlerNearestVisibleEnemy(GameContext w, int idx)
{
    Brawler *b = &w.session->brawlers[idx];
    int best = -1;
    float bestDist = 1e9f;

    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *t = &w.session->brawlers[i];
        if (i == idx || !t->alive || t->team == b->team) continue;
        if (!BrawlerCanSee(w, idx, i)) continue;

        float d = Vector3Distance(b->position, t->position);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

int BrawlerMostWoundedAlly(GameContext w, int idx, float maxDistance)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return -1;

    Brawler *b = &w.session->brawlers[idx];
    int best = -1;
    float bestRatio = 1.0f;
    float bestDist = maxDistance;

    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *t = &w.session->brawlers[i];
        if (i == idx || !t->alive || t->team != b->team || t->health >= t->maxHealth) continue;
        if (!ArenaLineOfSight(&w.session->arena, b->position, t->position)) continue;

        float dist = Vector3Distance(b->position, t->position);
        if (dist > maxDistance) continue;

        float ratio = (float)t->health/(float)t->maxHealth;
        if (ratio < bestRatio - 0.001f || (fabsf(ratio - bestRatio) <= 0.001f && dist < bestDist))
        {
            best = i;
            bestRatio = ratio;
            bestDist = dist;
        }
    }
    return best;
}

//------------------------------------------------------------------------------------
static void UpdateDash(GameContext w, int idx, float dt)
{
    Brawler *b = &w.session->brawlers[idx];
    const AbilityDefinition *ability = ContentAbility(w.content, b->dashAbility);
    if (!ability || ability->behavior != ABILITY_BEHAVIOR_DASH)
    {
        b->dashTimer = 0.0f;
        b->dashAbility = -1;
        return;
    }

    float moveDt = fminf(dt, b->dashTimer);
    b->dashTimer -= moveDt;
    if (b->dashTimer < 0.0001f) b->dashTimer = 0.0f;

    float speed = ability->data.dash.speed > 0.0f
                ? ability->data.dash.speed : w.tuning->dashSpeed;
    Vector3 next = Vector3Add(b->position, Vector3Scale(b->dashDir, speed*moveDt));

    // Destructive dashes sample the leading edge of the brawler, so the crate breaks
    // before circle resolution can pin the actor against its face.
    Vector3 dashNose = Vector3Add(next, Vector3Scale(b->dashDir, BRAWLER_RADIUS));
    if (ability->data.dash.breaksCrates &&
        ArenaTypeAt(&w.session->arena, dashNose.x, dashNose.z) == TILE_CRATE)
    {
        if (ArenaDamageAt(&w.session->arena, dashNose.x, dashNose.z,
                          w.tuning->crateHealth))
            GameEmitCrateBreak(w.session,
                               (Vector3){ dashNose.x, 0.6f, dashNose.z });
    }

    Vector3 resolved = ArenaResolveCircle(&w.session->arena, next, BRAWLER_RADIUS);
    // A regular boost stops on both cover types; Charge destroys crates first but
    // still ends immediately against a permanent wall.
    if (Vector3Distance(resolved, next) > 0.01f) b->dashTimer = 0.0f;
    b->position = resolved;

    if (ability->damage > 0)
    {
        for (int i = 0; i < w.session->brawlerCount; i++)
        {
            Brawler *t = &w.session->brawlers[i];
            if (!t->alive || t->team == b->team) continue;
            if (b->dashHitMask & (1 << i)) continue;

            if (Vector3Distance(b->position, t->position) < BRAWLER_RADIUS*2.2f)
            {
                BrawlerApplyDamage(w, i, ability->damage, idx, t->position);
                b->dashHitMask |= (1 << i);

                if (ability->data.dash.knockback > 0.0f)
                {
                    Vector3 push = Vector3Scale(b->dashDir,
                                                ability->data.dash.knockback);
                    Vector3 shoved = Vector3Add(t->position, push);
                    t->position = ArenaResolveCircle(&w.session->arena, shoved,
                                                     BRAWLER_RADIUS);
                }
                GameEmitImpact(w.session, t->position,
                               (Color){ 255, 220, 120, 255 }, 12);
            }
        }
    }

    Color trail = ability->damage > 0
                ? (Color){ 255, 200, 110, 180 }
                : (Color){ 92, 220, 255, 180 };
    GameEmitParticle(w.session, b->position, (Vector3){ 0, 1.0f, 0 },
                     trail, 0.3f, 0.28f, PARTICLE_SMOKE);

    if (b->dashTimer <= 0.0f) b->dashAbility = -1;
}

//------------------------------------------------------------------------------------
void BrawlersUpdate(GameContext w, float dt)
{
    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *b = &w.session->brawlers[i];
        const CharacterDefinition *character = ContentCharacter(w.content, b->cls);
        const AbilityDefinition *mainAbility = ContentMainAbility(w.content, b->cls);

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
        if (b->mobilityCooldown > 0.0f)
            b->mobilityCooldown = fmaxf(0.0f, b->mobilityCooldown - dt);
        if (b->aimHold > 0.0f) b->aimHold = fmaxf(0.0f, b->aimHold - dt);
        if (b->spawnScale < 1.0f) b->spawnScale = fminf(1.0f, b->spawnScale + dt * 4.5f);

        UpdateStatuses(w, i, dt);
        if (!b->alive) continue;

        // Ammo regenerates continuously, one pip at a time.
        if (b->ammo > (float)character->maxAmmo) b->ammo = (float)character->maxAmmo;
        if (b->ammo < (float)character->maxAmmo)
        {
            b->ammo += dt/mainAbility->reloadPerAmmo;
            if (b->ammo > (float)character->maxAmmo) b->ammo = (float)character->maxAmmo;
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
        Vector3 desired = Vector3Scale(b->moveIntent, w.tuning->moveSpeed);
        b->velocity.x = Lerp(b->velocity.x, desired.x, w.tuning->moveAccel * dt);
        b->velocity.z = Lerp(b->velocity.z, desired.z, w.tuning->moveAccel * dt);

        Vector3 next = Vector3Add(b->position, Vector3Scale(b->velocity, dt));
        next.y = 0.0f;
        b->position = ArenaResolveCircle(&w.session->arena, next, BRAWLER_RADIUS);

        // Keep brawlers from stacking on the same tile.
        for (int j = 0; j < w.session->brawlerCount; j++)
        {
            if (j == i) continue;
            Brawler *o = &w.session->brawlers[j];
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
                b->position = ArenaResolveCircle(&w.session->arena, b->position, BRAWLER_RADIUS);
            }
        }

        float speed = Vector3Length(b->velocity);
        if (speed > 0.4f)
        {
            b->moveFacing = atan2f(b->velocity.x, b->velocity.z);
            b->bobPhase += dt * (8.0f + speed);
        }

        b->inBush = ArenaBushAt(&w.session->arena, b->position.x, b->position.z);

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
        else if (b->isPlayer && (b->deliberateAim))
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
    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *b = &w.session->brawlers[i];

        if (b->team == TEAM_PLAYER || b->isPlayer) { b->visible = true; continue; }
        if (!b->inBush || b->revealTimer > 0.0f)   { b->visible = true; continue; }

        b->visible = false;
        for (int j = 0; j < w.session->brawlerCount; j++)
        {
            Brawler *o = &w.session->brawlers[j];
            if (!o->alive || o->team != TEAM_PLAYER) continue;
            if (Vector3Distance(o->position, b->position) <= w.tuning->bushReveal)
            {
                b->visible = true;
                break;
            }
        }
    }
}
