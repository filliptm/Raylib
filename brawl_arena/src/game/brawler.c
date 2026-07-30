#include "brawler.h"
#include "arena.h"
#include "weapons.h"
#include "game_events.h"
#include "gems.h"
#include "raymath.h"
#include "game_random.h"
#include "content_catalog.h"
#include "attack_content.h"
#include <stdio.h>
#include <math.h>

static void InterruptHealthRegeneration(GameContext w, Brawler *b)
{
    b->lastCombatTime = w.session->time;
    b->lastHealthRegenPulseTime = w.session->time;
}

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
    b->shieldAbility = -1;
    b->grappleAbility = -1;
    const AbilityDefinition *secondary = ContentSecondaryAbility(w.content, cls);
    if (secondary && secondary->behavior == ABILITY_BEHAVIOR_SHIELD)
    {
        b->shieldAbility = def->mobilityAbility;
        b->shieldCharge = (float)secondary->data.shield.capacity;
    }
    InterruptHealthRegeneration(w, b);
    b->bobPhase = GameRandomInt(&w.session->random, 0, 628) / 100.0f;
    b->aiTarget = -1;
    b->aiNavGoal = -1;
    b->aiNavWaypoint = -1;
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

BrawlerDamageResult BrawlerApplyDamageDetailed(
    GameContext w, int idx, int damage, int attacker, Vector3 hitPos)
{
    BrawlerDamageResult result = { 0 };
    if (idx < 0 || idx >= w.session->brawlerCount || damage <= 0)
        return result;
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive) return result;

    // God mode still flashes and shows numbers, so feedback stays readable.
    if (b->isPlayer && w.tuning->godMode)
    {
        b->hitFlash = 1.0f;
        return result;
    }

    const AbilityDefinition *shield =
        ContentAbility(w.content, b->shieldAbility);
    bool hostileAttacker =
        attacker >= 0 && attacker < w.session->brawlerCount &&
        attacker != idx &&
        w.session->brawlers[attacker].team != b->team;
    if (b->shieldActive && hostileAttacker && b->shieldCharge > 0.0f &&
        shield && shield->behavior == ABILITY_BEHAVIOR_SHIELD)
    {
        result.shieldAbsorbed =
            (int)fminf((float)damage, b->shieldCharge);
        b->shieldCharge -= (float)result.shieldAbsorbed;
        if (b->shieldCharge < 0.001f) b->shieldCharge = 0.0f;
        damage -= result.shieldAbsorbed;
        b->shieldRechargeDelay = shield->data.shield.rechargeDelay;
        b->hitFlash = 1.0f;
        InterruptHealthRegeneration(w, b);

        char shieldText[16];
        snprintf(shieldText, sizeof(shieldText), "SH -%d",
                 result.shieldAbsorbed);
        GameEmitCombatText(w.session, attacker, idx, COMBAT_TEXT_SHIELD,
                           hitPos, shieldText,
                           (Color){ 112, 232, 255, 255 });
        GameEmitImpact(w.session, hitPos, (Color){ 104, 226, 255, 255 }, 10);
        GameEmitVfxAttached(
            w.session, VFX_SCRAPPER_SHIELD_HIT,
            hitPos, b->position, b->aimAngle,
            0.95f + result.shieldAbsorbed/
                    (float)shield->data.shield.capacity,
            (Color){ 104, 226, 255, 255 },
            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);

        int healing = (int)floorf(
            result.shieldAbsorbed*shield->data.shield.healRatio + 0.5f);
        if (healing > 0)
            BrawlerApplyHealing(w, idx, healing, idx,
                                (Vector3){ b->position.x, 1.0f,
                                           b->position.z });

        if (b->shieldCharge <= 0.0f)
        {
            b->shieldActive = false;
            b->shieldBrokenTimer = shield->data.shield.breakLockout;
            b->shieldRearmRequired = true;
            GameEmitVfxAttached(
                w.session, VFX_SCRAPPER_SHIELD_BREAK,
                (Vector3){ b->position.x, 1.0f, b->position.z },
                (Vector3){ b->position.x, 1.0f, b->position.z },
                b->aimAngle, 1.65f, (Color){ 104, 226, 255, 255 },
                idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
            GameEmitShockwave(w.session, b->position, 2.0f, 0.30f,
                              (Color){ 104, 226, 255, 220 });
        }
    }

    if (damage > 0)
    {
        int before = b->health;
        b->health -= damage;
        if (b->health < 0) b->health = 0;
        result.healthRemoved = before - b->health;
        b->hitFlash = 1.0f;
        if (result.healthRemoved > 0)
        {
            InterruptHealthRegeneration(w, b);
            if (shield && shield->behavior == ABILITY_BEHAVIOR_SHIELD)
                b->shieldRechargeDelay = shield->data.shield.rechargeDelay;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%d", result.healthRemoved);
        GameEmitCombatText(
            w.session, attacker, idx, COMBAT_TEXT_DAMAGE, hitPos, buf,
            (attacker == w.session->playerIdx)
                ? (Color){ 255, 235, 140, 255 }
                : (Color){ 255, 150, 150, 255 });
    }

    if (b->health == 0)
    {
        b->alive = false;
        b->respawnTimer = b->isPlayer ? w.tuning->playerRespawn : w.tuning->enemyRespawn;
        b->dashTimer = 0.0f;
        b->dashAbility = -1;
        b->grappleDelayTimer = 0.0f;
        b->grappleTimer = 0.0f;
        b->grappleAbility = -1;
        b->shieldActive = false;
        b->shieldAbility = -1;

        GameEmitDeath(w.session, b->position, TEAM_COLORS[b->team]);

        // Everything they were carrying goes back on the floor for anyone to take.
        GemsDropFrom(w, idx);

        if (attacker == w.session->playerIdx && !b->isPlayer)
        {
            w.session->kills++;
            GameEmitCombatText(w.session, attacker, idx, COMBAT_TEXT_KNOCKOUT,
                               b->position, "KO!",
                               (Color){ 255, 210, 80, 255 });
            // A takedown tops up the killer's super, the way a finished fight should feel.
            BrawlerAwardSuper(w, attacker, 0.20f);
        }
        if (b->isPlayer) w.session->deaths++;
    }
    return result;
}

int BrawlerApplyDamage(GameContext w, int idx, int damage, int attacker,
                       Vector3 hitPos)
{
    return BrawlerApplyDamageDetailed(w, idx, damage, attacker,
                                      hitPos).healthRemoved;
}

int BrawlerApplyHealing(GameContext w, int idx, int amount, int healer, Vector3 hitPos)
{
    if (idx < 0 || idx >= w.session->brawlerCount || amount <= 0) return 0;

    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive || b->health >= b->maxHealth) return 0;

    int before = b->health;
    b->health += amount;
    if (b->health > b->maxHealth) b->health = b->maxHealth;
    int restored = b->health - before;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", restored);
    GameEmitCombatText(w.session, healer, idx, COMBAT_TEXT_HEALING,
                       hitPos, buf,
                       (Color){ 112, 255, 174, 255 });
    GameEmitImpact(w.session, hitPos, (Color){ 70, 244, 166, 255 }, 7);
    GameEmitLight(w.session, (Vector3){ hitPos.x, hitPos.y + 0.7f, hitPos.z },
                 (Color){ 70, 244, 166, 255 }, 1.7f, 0.18f);
    GameEmitVfxAttached(w.session, VFX_GENERIC_HEAL, hitPos, hitPos,
                        0.0f, 0.95f, (Color){ 70, 244, 166, 255 },
                        idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
    return restored;
}

void BrawlerApplyPulseStatus(GameContext w, int idx, Team sourceTeam, int source,
                             int damage, int healing, float duration, float tickRate,
                             int abilityIndex)
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
    if (!status)
    {
        // Every slot is busy with another source: evict the effect closest to expiry
        // instead of always clobbering slot 0.
        status = &b->statuses[0];
        for (int i = 1; i < MAX_STATUS_EFFECTS; i++)
            if (b->statuses[i].remaining < status->remaining)
                status = &b->statuses[i];
    }
    *status = (StatusEffect){
        .remaining = duration,
        .tickTimer = tickRate,
        .tickRate = tickRate,
        .damage = damage,
        .healing = healing,
        .source = source,
        .sourceTeam = sourceTeam,
        .abilityIndex = abilityIndex,
        .active = true
    };

    if (AttackAuthored(w.content, abilityIndex))
        GameEmitAttackMark(w.session, GAME_EVENT_ATTACK_MARK_APPLIED,
                           abilityIndex, idx,
                           (Vector3){ b->position.x, 0.9f, b->position.z },
                           duration,
                           b->team == sourceTeam
                               ? (Color){ 96, 255, 196, 255 }
                               : (Color){ 255, 96, 170, 255 });
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
            bool authored = AttackAuthored(w.content, status->abilityIndex);
            if (b->team == status->sourceTeam)
            {
                int restored =
                    BrawlerApplyHealing(w, idx, status->healing,
                                        status->source, hit);
                if (restored > 0)
                {
                    if (authored)
                        GameEmitAttackMark(w.session, GAME_EVENT_ATTACK_MARK_TICK,
                                           status->abilityIndex, idx, hit,
                                           status->remaining,
                                           (Color){ 96, 255, 196, 255 });
                    else
                        GameEmitVfxAttached(
                            w.session, VFX_GUARDIAN_RESONANCE_HEAL,
                            hit, hit, 0.0f, 1.05f,
                            (Color){ 70, 244, 166, 255 },
                            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
                }
            }
            else
            {
                int removed =
                    BrawlerApplyDamage(w, idx, status->damage,
                                       status->source, hit);
                if (removed > 0)
                {
                    if (authored)
                        GameEmitAttackMark(w.session, GAME_EVENT_ATTACK_MARK_TICK,
                                           status->abilityIndex, idx, hit,
                                           status->remaining,
                                           (Color){ 255, 96, 170, 255 });
                    else
                        GameEmitVfxAttached(
                            w.session, VFX_GUARDIAN_RESONANCE_DAMAGE,
                            hit, hit, 0.0f, 1.10f,
                            (Color){ 255, 92, 158, 255 },
                            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
                }
            }
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
    if (!b->alive || b->attackCd > 0.0f || b->dashTimer > 0.0f ||
        b->shieldActive || BrawlerIsGrappling(b))
        return false;

    bool freeAmmo = b->isPlayer && w.tuning->infiniteAmmo;
    if (!freeAmmo && b->ammo < 1.0f) return false;
    if (!freeAmmo) b->ammo -= 1.0f;
    // attackCd is at most one frame below zero here; carrying it keeps sustained
    // fire cadence exact instead of quantized to frame boundaries.
    b->attackCd = ContentMainAbility(w.content, b->cls)->cooldown + b->attackCd;
    InterruptHealthRegeneration(w, b);
    WeaponsFire(w, idx, false, aimDist);
    return true;
}

bool BrawlerTrySuper(GameContext w, int idx, float aimDist)
{
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive || b->superCharge < 1.0f || b->dashTimer > 0.0f ||
        b->shieldActive || BrawlerIsGrappling(b))
        return false;

    if (!(b->isPlayer && w.tuning->infiniteAmmo)) b->superCharge = 0.0f;
    // A super may fire mid-cooldown, so only a negative sub-frame remainder carries.
    b->attackCd = ContentMainAbility(w.content, b->cls)->cooldown +
                  (b->attackCd < 0.0f ? b->attackCd : 0.0f);
    InterruptHealthRegeneration(w, b);
    WeaponsFire(w, idx, true, aimDist);
    return true;
}

void BrawlerReleaseShield(GameContext w, int idx)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return;
    Brawler *b = &w.session->brawlers[idx];
    b->shieldActive = false;
    b->shieldRearmRequired = false;
}

Vector3 BrawlerGrappleEndpoint(const Arena *arena, Vector3 start,
                               Vector3 direction, float range)
{
    direction.y = 0.0f;
    if (Vector3LengthSqr(direction) < 0.000001f || range <= 0.0f)
    {
        start.y = 0.0f;
        return start;
    }
    direction = Vector3Normalize(direction);

    float stepLength =
        fminf(arena->tileSize*0.25f, BRAWLER_RADIUS*0.5f);
    if (stepLength < 0.05f) stepLength = 0.05f;
    int steps = (int)ceilf(range/stepLength);
    if (steps < 1) steps = 1;

    Vector3 endpoint = start;
    for (int step = 1; step <= steps; step++)
    {
        float distance = range*step/(float)steps;
        Vector3 candidate =
            Vector3Add(start, Vector3Scale(direction, distance));
        if (!ArenaCircleClear(arena, candidate, BRAWLER_RADIUS)) break;
        endpoint = candidate;
    }
    endpoint.y = 0.0f;
    return endpoint;
}


// Authored secondary abilities get a cast anchor on top of their legacy VFX, so
// the studio can layer additional effects onto shields, dashes, grapples, and
// mines without the game losing its built-in presentation.
static void EmitSecondaryCast(GameContext w, int idx,
                              const CharacterDefinition *character)
{
    Brawler *b = &w.session->brawlers[idx];
    if (!AttackAuthored(w.content, character->mobilityAbility)) return;
    GameEmitAttackCast(w.session, character->mobilityAbility, idx,
                       (Vector3){ b->position.x, 0.85f, b->position.z },
                       b->aimAngle, (Color){ 255, 255, 255, 255 });
}

bool BrawlerTrySecondary(GameContext w, int idx, Vector3 direction)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return false;

    Brawler *b = &w.session->brawlers[idx];
    const CharacterDefinition *character = ContentCharacter(w.content, b->cls);
    const AbilityDefinition *ability = ContentSecondaryAbility(w.content, b->cls);
    if (!b->alive || !character || !ability ||
        b->mobilityCooldown > 0.0f || b->dashTimer > 0.0f ||
        b->shieldActive || BrawlerIsGrappling(b))
        return false;

    direction.y = 0.0f;
    if (ability->behavior == ABILITY_BEHAVIOR_SHIELD)
    {
        if (b->shieldRearmRequired || b->shieldBrokenTimer > 0.0f ||
            b->shieldCharge <= 0.0f)
            return false;
        if (Vector3Length(direction) > 0.001f)
            b->aimAngle = atan2f(direction.x, direction.z);
        b->shieldActive = true;
        b->shieldAbility = character->mobilityAbility;
        b->revealTimer = w.tuning->fireReveal;
        GameEmitVfxAttached(
            w.session, VFX_SCRAPPER_SHIELD_START,
            (Vector3){ b->position.x, 0.85f, b->position.z },
            (Vector3){ b->position.x, 0.85f, b->position.z },
            b->aimAngle, 1.35f, (Color){ 104, 226, 255, 255 },
            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
        GameEmitCharacterAction(w.session, idx, CHARACTER_ACTION_GUARD);
        EmitSecondaryCast(w, idx, character);
        return true;
    }

    if (ability->behavior == ABILITY_BEHAVIOR_GRAPPLE)
    {
        if (Vector3Length(direction) < 0.001f) return false;
        direction = Vector3Normalize(direction);
        Vector3 endpoint = BrawlerGrappleEndpoint(
            &w.session->arena, b->position, direction, ability->range);
        if (Vector3Distance(b->position, endpoint) < 1.5f)
            return false;

        b->grappleDelayTimer = ability->data.grapple.launchDelay;
        b->grappleTimer = ability->data.grapple.pullDuration;
        b->grappleAnchor = endpoint;
        b->grappleAbility = character->mobilityAbility;
        b->mobilityCooldown = ability->cooldown;
        b->velocity = (Vector3){ 0 };
        b->aimAngle = atan2f(direction.x, direction.z);
        b->shotYaw = b->aimAngle;
        b->aimHold = ability->data.grapple.launchDelay +
                     ability->data.grapple.pullDuration;
        b->revealTimer = w.tuning->fireReveal;

        Vector3 hand = { b->position.x, 1.05f, b->position.z };
        Vector3 anchor = { endpoint.x, 0.18f, endpoint.z };
        GameEmitVfxAttached(
            w.session, VFX_LONGSHOT_GRAPPLE_FIRE,
            hand, anchor, b->aimAngle, 1.0f,
            (Color){ 72, 224, 255, 255 },
            idx, VFX_SOCKET_RIGHT_HAND, -1, VFX_SOCKET_NONE);
        GameEmitCharacterActionTimed(
            w.session, idx, CHARACTER_ACTION_GRAPPLE,
            ability->data.grapple.launchDelay +
            ability->data.grapple.pullDuration + 0.18f);
        EmitSecondaryCast(w, idx, character);
        return true;
    }

    if (ability->behavior == ABILITY_BEHAVIOR_MINE)
    {
        if (!WeaponsPlaceMine(w, idx, ability)) return false;
        b->mobilityCooldown = ability->cooldown;
        b->velocity = (Vector3){ 0 };
        b->revealTimer = w.tuning->fireReveal;
        GameEmitCharacterActionTimed(
            w.session, idx, CHARACTER_ACTION_MINE_DEPLOY,
            ability->data.mine.armTime + 0.27f);
        EmitSecondaryCast(w, idx, character);
        return true;
    }

    if (ability->behavior != ABILITY_BEHAVIOR_DASH ||
        Vector3Length(direction) < 0.001f)
        return false;

    b->dashTimer = ability->data.dash.duration;
    b->dashDir = Vector3Normalize(direction);
    b->dashAbility = character->mobilityAbility;
    b->dashHitMask = 0;
    b->dashVfxTimer = 0.0f;
    b->mobilityCooldown = ability->cooldown;
    b->velocity = (Vector3){ 0 };
    b->revealTimer = w.tuning->fireReveal;
    GameEmitMuzzle(w.session, b->position,
                   atan2f(b->dashDir.x, b->dashDir.z),
                   (Color){ 92, 220, 255, 255 });
    GameEmitLight(w.session, (Vector3){ b->position.x, 0.8f, b->position.z },
                  (Color){ 92, 220, 255, 255 }, 2.2f, 0.16f);
    Vector3 jetStart = { b->position.x, 0.72f, b->position.z };
    Vector3 jetEnd =
        Vector3Subtract(jetStart, Vector3Scale(b->dashDir, 1.45f));
    GameEmitVfxAttached(
        w.session, VFX_TANK_JETS_START, jetStart, jetEnd,
        atan2f(b->dashDir.x, b->dashDir.z), 1.30f,
        (Color){ 92, 220, 255, 255 },
        idx, VFX_SOCKET_LEFT_SHOULDER, -1, VFX_SOCKET_NONE);
    GameEmitVfxAttached(
        w.session, VFX_TANK_JETS_START, jetStart, jetEnd,
        atan2f(b->dashDir.x, b->dashDir.z), 1.30f,
        (Color){ 92, 220, 255, 255 },
        idx, VFX_SOCKET_RIGHT_SHOULDER, -1, VFX_SOCKET_NONE);
    GameEmitCharacterAction(w.session, idx, CHARACTER_ACTION_MOBILITY);
    EmitSecondaryCast(w, idx, character);
    return true;
}

bool BrawlerTryMobility(GameContext w, int idx, Vector3 direction)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return false;
    const AbilityDefinition *ability =
        ContentSecondaryAbility(w.content, w.session->brawlers[idx].cls);
    if (!ability || ability->behavior != ABILITY_BEHAVIOR_DASH) return false;
    return BrawlerTrySecondary(w, idx, direction);
}

bool BrawlerIsGrappling(const Brawler *b)
{
    return b &&
           (b->grappleDelayTimer > 0.0f || b->grappleTimer > 0.0f);
}

void BrawlerDisplace(GameContext w, int idx, Vector3 displacement)
{
    if (idx < 0 || idx >= w.session->brawlerCount) return;
    Brawler *b = &w.session->brawlers[idx];
    if (!b->alive) return;
    b->grappleDelayTimer = 0.0f;
    b->grappleTimer = 0.0f;
    b->grappleAbility = -1;
    b->position = ArenaMoveCircle(
        &w.session->arena, b->position,
        displacement, BRAWLER_RADIUS).position;
}

bool BrawlerProjectileThreat(GameContext w, int idx, float horizon)
{
    if (idx < 0 || idx >= w.session->brawlerCount || horizon <= 0.0f)
        return false;
    Brawler *b = &w.session->brawlers[idx];
    const AbilityDefinition *ability =
        ContentSecondaryAbility(w.content, b->cls);
    if (!b->alive || b->shieldBrokenTimer > 0.0f ||
        b->shieldCharge <= 0.0f ||
        !ability || ability->behavior != ABILITY_BEHAVIOR_SHIELD)
        return false;

    for (int projectile = 0; projectile < MAX_PROJECTILES; projectile++)
    {
        Projectile *p = &w.session->projectiles[projectile];
        if (!p->active || p->team == b->team) continue;
        float speedSq = p->velocity.x*p->velocity.x +
                        p->velocity.z*p->velocity.z;
        if (speedSq <= 0.001f) continue;

        Vector3 relative = Vector3Subtract(b->position, p->position);
        relative.y = 0.0f;
        float time = (relative.x*p->velocity.x +
                      relative.z*p->velocity.z)/speedSq;
        if (time < 0.0f || time > horizon) continue;
        Vector3 closest = Vector3Add(
            p->position, Vector3Scale(p->velocity, time));
        if (Vector3Distance(closest, b->position) >
            BRAWLER_RADIUS + p->radius)
            continue;

        return true;
    }
    return false;
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
    Vector3 displacement = Vector3Scale(b->dashDir, speed*moveDt);

    // Sample the leading edge across the entire step so tuned/high-frame-time Charges
    // cannot tunnel past a crate before the body sweep gets a chance to stop.
    if (ability->data.dash.breaksCrates)
    {
        float sampleLength = fminf(w.session->arena.tileSize*0.25f,
                                   BRAWLER_RADIUS*0.5f);
        int samples = (int)ceilf(Vector3Length(displacement)/sampleLength);
        if (samples < 1) samples = 1;
        for (int sample = 1; sample <= samples; sample++)
        {
            Vector3 dashNose = Vector3Add(
                b->position,
                Vector3Scale(displacement, (float)sample/(float)samples));
            dashNose = Vector3Add(
                dashNose, Vector3Scale(b->dashDir, BRAWLER_RADIUS));
            if (ArenaTypeAt(&w.session->arena,
                            dashNose.x, dashNose.z) != TILE_CRATE) continue;
            if (ArenaDamageAt(&w.session->arena, dashNose.x, dashNose.z,
                              w.tuning->crateHealth))
                GameEmitCrateBreak(
                    w.session, (Vector3){ dashNose.x, 0.6f, dashNose.z });
        }
    }

    ArenaMoveResult move = ArenaMoveCircle(
        &w.session->arena, b->position, displacement, BRAWLER_RADIUS);
    // A regular boost stops on both cover types; Charge destroys crates first but
    // still ends immediately against a permanent wall.
    bool blocked = move.collided;
    if (blocked) b->dashTimer = 0.0f;
    b->position = move.position;

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
                    BrawlerDisplace(w, i, push);
                }
                GameEmitImpact(w.session, t->position,
                               (Color){ 255, 220, 120, 255 }, 12);
                GameEmitVfx(w.session, VFX_TANK_CHARGE_IMPACT,
                            t->position, t->position, 0.0f, 1.55f,
                            (Color){ 255, 204, 104, 255 });
            }
        }
    }

    b->dashVfxTimer -= moveDt;
    if (b->dashVfxTimer <= 0.0f)
    {
        Vector3 trailStart = { b->position.x, 0.68f, b->position.z };
        float trailLength = ability->damage > 0 ? 1.65f : 1.25f;
        Vector3 trailEnd =
            Vector3Subtract(trailStart, Vector3Scale(b->dashDir, trailLength));
        GameEmitVfxAttached(
            w.session,
            ability->damage > 0
                ? VFX_TANK_CHARGE_TRAIL : VFX_TANK_JETS_TRAIL,
            trailStart, trailEnd,
            atan2f(b->dashDir.x, b->dashDir.z),
            ability->damage > 0 ? 1.25f : 1.05f,
            ability->damage > 0
                ? (Color){ 255, 196, 96, 255 }
                : (Color){ 92, 220, 255, 255 },
            idx, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
        b->dashVfxTimer += ability->damage > 0 ? 0.055f : 0.045f;
    }

    if (blocked && ability->damage > 0)
        GameEmitVfx(w.session, VFX_TANK_CHARGE_IMPACT,
                    b->position, b->position, 0.0f, 1.35f,
                    (Color){ 255, 204, 104, 255 });

    Color trail = ability->damage > 0
                ? (Color){ 255, 200, 110, 180 }
                : (Color){ 92, 220, 255, 180 };
    GameEmitParticle(w.session, b->position, (Vector3){ 0, 1.0f, 0 },
                     trail, 0.3f, 0.28f, PARTICLE_SMOKE);

    if (b->dashTimer <= 0.0f) b->dashAbility = -1;
}

static void RemoveInwardVelocity(Brawler *b, Vector3 normal)
{
    if (Vector3LengthSqr(normal) <= 0.0000001f) return;
    float inward = Vector3DotProduct(b->velocity, normal);
    if (inward < 0.0f)
        b->velocity = Vector3Subtract(
            b->velocity, Vector3Scale(normal, inward));
    b->velocity.y = 0.0f;
}

static void UpdateGrapple(GameContext w, int idx, float dt)
{
    Brawler *b = &w.session->brawlers[idx];
    const AbilityDefinition *ability =
        ContentAbility(w.content, b->grappleAbility);
    if (!ability || ability->behavior != ABILITY_BEHAVIOR_GRAPPLE)
    {
        b->grappleDelayTimer = 0.0f;
        b->grappleTimer = 0.0f;
        b->grappleAbility = -1;
        return;
    }

    b->velocity = (Vector3){ 0 };
    Vector3 toward = Vector3Subtract(b->grappleAnchor, b->position);
    toward.y = 0.0f;
    if (Vector3Length(toward) > 0.001f)
    {
        b->aimAngle = atan2f(toward.x, toward.z);
        b->renderYaw = AngleLerp(
            b->renderYaw, b->aimAngle, Clamp(24.0f*dt, 0.0f, 1.0f));
    }

    if (b->grappleDelayTimer > 0.0f)
    {
        float before = b->grappleDelayTimer;
        b->grappleDelayTimer =
            fmaxf(0.0f, b->grappleDelayTimer - dt);
        if (before > 0.0f && b->grappleDelayTimer <= 0.0f)
        {
            Vector3 anchor = {
                b->grappleAnchor.x, 0.18f, b->grappleAnchor.z
            };
            GameEmitVfx(
                w.session, VFX_LONGSHOT_GRAPPLE_HOOK,
                anchor, anchor, b->aimAngle, 1.0f,
                (Color){ 108, 238, 255, 255 });
            GameEmitVfxAttached(
                w.session, VFX_LONGSHOT_GRAPPLE_PULL,
                (Vector3){ b->position.x, 1.05f, b->position.z },
                anchor, b->aimAngle, 1.0f,
                (Color){ 72, 224, 255, 255 },
                idx, VFX_SOCKET_RIGHT_HAND, -1, VFX_SOCKET_NONE);
        }
        return;
    }

    if (b->grappleTimer <= 0.0f)
    {
        b->grappleAbility = -1;
        return;
    }

    float distance = Vector3Length(toward);
    float amount = b->grappleTimer > 0.0001f
                 ? Clamp(dt/b->grappleTimer, 0.0f, 1.0f)
                 : 1.0f;
    ArenaMoveResult move = ArenaMoveCircle(
        &w.session->arena, b->position,
        Vector3Scale(toward, amount), BRAWLER_RADIUS);
    b->position = move.position;
    b->grappleTimer = fmaxf(0.0f, b->grappleTimer - dt);
    b->bobPhase += dt*20.0f;

    if (b->grappleTimer <= 0.0f || distance < 0.08f || move.collided)
    {
        b->grappleTimer = 0.0f;
        b->grappleAbility = -1;
        GameEmitVfxAttached(
            w.session, VFX_LONGSHOT_GRAPPLE_LAND,
            (Vector3){ b->position.x, 0.22f, b->position.z },
            (Vector3){ b->position.x, 0.22f, b->position.z },
            b->aimAngle, 1.0f,
            (Color){ 108, 238, 255, 255 },
            idx, VFX_SOCKET_LEFT_FOOT, -1, VFX_SOCKET_NONE);
    }
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
        // The cooldown keeps its sub-frame overshoot for exactly one frame so a shot
        // fired the moment it expires can roll the remainder into the next cooldown;
        // after that the stale remainder is cleared so idle time earns no head start.
        if (b->attackCd > 0.0f) b->attackCd -= dt;
        else if (b->attackCd < 0.0f) b->attackCd = 0.0f;
        if (b->hitFlash > 0.0f) b->hitFlash = fmaxf(0.0f, b->hitFlash - dt * 5.0f);
        if (b->revealTimer > 0.0f) b->revealTimer -= dt;
        if (b->mobilityCooldown > 0.0f)
            b->mobilityCooldown = fmaxf(0.0f, b->mobilityCooldown - dt);
        if (b->aimHold > 0.0f) b->aimHold = fmaxf(0.0f, b->aimHold - dt);
        if (b->spawnScale < 1.0f) b->spawnScale = fminf(1.0f, b->spawnScale + dt * 4.5f);

        UpdateStatuses(w, i, dt);
        if (!b->alive) continue;

        const AbilityDefinition *shield =
            ContentAbility(w.content, b->shieldAbility);
        if (b->shieldActive)
            b->revealTimer = fmaxf(b->revealTimer, 0.1f);
        if (shield && shield->behavior == ABILITY_BEHAVIOR_SHIELD)
        {
            if (b->shieldCharge > shield->data.shield.capacity)
                b->shieldCharge = (float)shield->data.shield.capacity;
            if (b->shieldBrokenTimer > 0.0f)
            {
                b->shieldBrokenTimer =
                    fmaxf(0.0f, b->shieldBrokenTimer - dt);
                if (b->shieldBrokenTimer <= 0.0f)
                {
                    b->shieldCharge = (float)shield->data.shield.capacity;
                    b->shieldRechargeDelay = 0.0f;
                    GameEmitVfxAttached(
                        w.session, VFX_SCRAPPER_SHIELD_RESTORE,
                        (Vector3){ b->position.x, 1.0f, b->position.z },
                        (Vector3){ b->position.x, 1.0f, b->position.z },
                        b->aimAngle, 1.45f,
                        (Color){ 104, 226, 255, 255 },
                        i, VFX_SOCKET_CHEST, -1, VFX_SOCKET_NONE);
                }
            }
            else if (!b->shieldActive &&
                     b->shieldCharge < shield->data.shield.capacity)
            {
                if (b->shieldRechargeDelay > 0.0f)
                    b->shieldRechargeDelay =
                        fmaxf(0.0f, b->shieldRechargeDelay - dt);
                else
                {
                    float before = b->shieldCharge;
                    b->shieldCharge = fminf(
                        (float)shield->data.shield.capacity,
                        b->shieldCharge +
                            shield->data.shield.rechargeRate*dt);
                    if (before < shield->data.shield.capacity &&
                        b->shieldCharge >= shield->data.shield.capacity)
                        GameEmitVfxAttached(
                            w.session, VFX_SCRAPPER_SHIELD_RESTORE,
                            (Vector3){ b->position.x, 1.0f,
                                       b->position.z },
                            (Vector3){ b->position.x, 1.0f,
                                       b->position.z },
                            b->aimAngle, 1.25f,
                            (Color){ 104, 226, 255, 255 },
                            i, VFX_SOCKET_CHEST, -1,
                            VFX_SOCKET_NONE);
                }
            }
        }

        // Ammo regenerates continuously, one pip at a time.
        if (b->ammo > (float)character->maxAmmo) b->ammo = (float)character->maxAmmo;
        if (b->ammo < (float)character->maxAmmo)
        {
            b->ammo += dt/mainAbility->reloadPerAmmo;
            if (b->ammo > (float)character->maxAmmo) b->ammo = (float)character->maxAmmo;
        }

        if (BrawlerIsGrappling(b))
        {
            UpdateGrapple(w, i, dt);
            continue;
        }

        if (b->dashTimer > 0.0f)
        {
            UpdateDash(w, i, dt);
            b->bobPhase += dt * 22.0f;
            b->renderYaw = AngleLerp(b->renderYaw, atan2f(b->dashDir.x, b->dashDir.z),
                                     SmoothFactor(24.0f, dt));
            continue;
        }

        // Movement: accelerate toward the controller's intent, then resolve collisions.
        float moveMultiplier = 1.0f;
        if (b->shieldActive)
        {
            const AbilityDefinition *activeShield =
                ContentAbility(w.content, b->shieldAbility);
            if (activeShield &&
                activeShield->behavior == ABILITY_BEHAVIOR_SHIELD)
                moveMultiplier =
                    activeShield->data.shield.moveMultiplier;
        }
        Vector3 desired =
            Vector3Scale(b->moveIntent, w.tuning->moveSpeed*moveMultiplier);
        float acceleration = SmoothFactor(w.tuning->moveAccel, dt);
        b->velocity.x = Lerp(b->velocity.x, desired.x, acceleration);
        b->velocity.z = Lerp(b->velocity.z, desired.z, acceleration);

        ArenaMoveResult move = ArenaMoveCircle(
            &w.session->arena, b->position,
            Vector3Scale(b->velocity, dt), BRAWLER_RADIUS);
        b->position = move.position;
        if (move.collided) RemoveInwardVelocity(b, move.normal);

        float speed = Vector3Length(b->velocity);
        if (speed > 0.4f)
        {
            b->moveFacing = atan2f(b->velocity.x, b->velocity.z);
            b->bobPhase += dt * (8.0f + speed);
        }

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

        b->renderYaw = AngleLerp(b->renderYaw, targetYaw, SmoothFactor(turnRate, dt));
    }

    // Visibility is resolved from the player's side, for rendering and for the HUD.
    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *b = &w.session->brawlers[i];
        b->inBush = b->alive &&
                    ArenaBushAt(&w.session->arena,
                                b->position.x, b->position.z);

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

void BrawlersUpdateRegeneration(GameContext w)
{
    const float delay = w.tuning->healthRegenDelay;
    const float interval = w.tuning->healthRegenInterval;
    const float ratio = w.tuning->healthRegenRatio;
    const float now = w.session->time;

    if (interval <= 0.0f) return;

    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *b = &w.session->brawlers[i];
        if (!b->alive) continue;

        // A zero ratio is the explicit live-authoring off state. Keep its schedule
        // current so enabling it later cannot replay a backlog of disabled pulses.
        if (ratio <= 0.0f)
        {
            b->lastHealthRegenPulseTime = now;
            continue;
        }

        bool hasRegenerated =
            b->lastHealthRegenPulseTime > b->lastCombatTime + 0.0001f;
        float nextPulse = hasRegenerated
                        ? b->lastHealthRegenPulseTime + interval
                        : b->lastCombatTime + delay;

        while (now + 0.0001f >= nextPulse)
        {
            int amount = (int)floorf(b->maxHealth*ratio + 0.5f);
            if (amount < 1) amount = 1;
            Vector3 position = { b->position.x, 0.85f, b->position.z };
            BrawlerApplyHealing(w, i, amount, i, position);

            // Advance the authored pulse schedule even at full health. Damage will
            // reset both timestamps before recovery is evaluated later that frame.
            b->lastHealthRegenPulseTime = nextPulse;
            nextPulse += interval;
        }
    }
}
