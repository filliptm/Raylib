#include "player.h"
#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "game_commands.h"
#include "game_events.h"
#include "content_catalog.h"
#include "raymath.h"
#include <math.h>

#define TAP_THRESHOLD 0.14f     // shorter than this counts as a tap-to-autoaim shot

static int AutoAimTarget(App *w, int idx)
{
    GameContext game = AppGameContext(w);
    Brawler *b = &w->session.brawlers[idx];
    const AbilityDefinition *ability = ContentMainAbility(&w->content, b->cls);

    if (ability->healing > 0)
    {
        int ally = BrawlerMostWoundedAlly(game, idx, ability->range);
        if (ally >= 0 &&
            (float)w->session.brawlers[ally].health/(float)w->session.brawlers[ally].maxHealth <
                w->tune.aiSupportHealth)
            return ally;
    }
    return BrawlerNearestVisibleEnemy(game, idx);
}

// One shared release-fire path for immediate and buffered attacks. A quick tap
// auto-aims (Guardians favor a badly hurt ally; everyone else picks the nearest
// visible enemy, mirroring mobile-style auto-aim); a held release fires down the
// deliberate aim line.
static bool FireMainAttack(App *w, int idx, bool tap)
{
    GameContext game = AppGameContext(w);
    Brawler *b = &w->session.brawlers[idx];
    float aimLen = w->controller.aimDist;

    if (tap)
    {
        int target = AutoAimTarget(w, idx);
        if (target >= 0)
        {
            Vector3 t = w->session.brawlers[target].position;
            Vector3 d = Vector3Subtract(t, b->position);
            d.y = 0.0f;

            const AbilityDefinition *mainAbility =
                ContentMainAbility(&w->content, b->cls);
            float shotSpeed =
                (mainAbility->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
                 mainAbility->behavior == ABILITY_BEHAVIOR_LOB)
                ? mainAbility->data.projectile.speed : 0.0f;
            if (mainAbility->behavior == ABILITY_BEHAVIOR_RETURNING)
                shotSpeed = mainAbility->data.returning.outboundSpeed;
            float travel = (shotSpeed > 0.1f) ? Vector3Length(d)/shotSpeed : 0.0f;
            Vector3 lead = Vector3Add(t, Vector3Scale(w->session.brawlers[target].velocity, travel * 0.7f));
            Vector3 ld = Vector3Subtract(lead, b->position);

            b->aimAngle = atan2f(ld.x, ld.z);
            if (BrawlerTryAttack(game, idx,
                                 Vector3Length((Vector3){ ld.x, 0.0f, ld.z })))
            {
                BrawlerFaceShot(game, idx, b->aimAngle, 0.45f);
                return true;
            }
            return false;
        }
    }

    if (BrawlerTryAttack(game, idx, aimLen))
    {
        BrawlerFaceShot(game, idx, b->aimAngle, 0.25f);
        return true;
    }
    return false;
}

void PlayerUpdate(App *w, const PlayerInput *input, float dt)
{
    GameContext game = AppGameContext(w);
    int idx = w->session.playerIdx;
    Brawler *b = &w->session.brawlers[idx];

    // Class switching goes through the shared command so the number keys and the
    // command center follow identical carry-over and lockout rules.
    int c = input->selectedClass;
    if (c >= 0 && c < CLASS_COUNT)
    {
        GameCommandExecute(w, (GameCommand){
            .type = GAME_COMMAND_SET_PLAYER_CLASS, .value = c });
        b = &w->session.brawlers[idx];
    }

    w->controller.aimPoint = input->aimPoint;

    if (!b->alive)
    {
        w->controller.charging = false;
        w->controller.aimingSuper = false;
        b->deliberateAim = false;
        b->moveIntent = (Vector3){ 0 };
        return;
    }

    b->moveIntent = input->moveIntent;

    //--- Aiming ----------------------------------------------------------------
    Vector3 toAim = Vector3Subtract(w->controller.aimPoint, b->position);
    toAim.y = 0.0f;
    float aimLen = Vector3Length(toAim);

    if (aimLen > 0.2f)
        b->aimAngle = atan2f(toAim.x, toAim.z);

    w->controller.aimDist = aimLen;

    if (b->dashTimer > 0.0f || BrawlerIsGrappling(b))
    {
        w->controller.charging = false;
        w->controller.aimingSuper = false;
        b->deliberateAim = false;
        return;
    }

    // Clicks meant for the command center must not also fire the weapon.
    if (input->actionsBlocked)
    {
        if (b->shieldActive || b->shieldRearmRequired)
            BrawlerReleaseShield(game, idx);
        w->controller.charging = false;
        w->controller.aimingSuper = false;
        b->deliberateAim = false;
        return;
    }

    const AbilityDefinition *secondary =
        ContentSecondaryAbility(&w->content, b->cls);

    // Shift/LB is a typed secondary: shields remain raised while held, while
    // dash, grapple, and mine abilities trigger once on the press edge.
    if (secondary && secondary->behavior == ABILITY_BEHAVIOR_SHIELD &&
        (input->secondaryReleased || !input->secondaryHeld))
        BrawlerReleaseShield(game, idx);

    if (b->shieldActive)
    {
        w->controller.charging = false;
        w->controller.aimingSuper = false;
        b->deliberateAim = false;
        return;
    }

    if (input->secondaryPressed || input->mobilityPressed)
    {
        Vector3 direction = input->moveIntent;
        if (secondary &&
            (secondary->behavior == ABILITY_BEHAVIOR_SHIELD ||
             secondary->behavior == ABILITY_BEHAVIOR_GRAPPLE))
            direction = toAim;
        if (Vector3Length(direction) < 0.001f)
            direction = (Vector3){ sinf(b->aimAngle), 0.0f, cosf(b->aimAngle) };

        if (BrawlerTrySecondary(game, idx, direction))
        {
            w->controller.charging = false;
            w->controller.aimingSuper = false;
            b->deliberateAim = false;
            return;
        }
    }

    //--- Super: hold right mouse to aim, release to fire ------------------------
    if (input->superHeld && b->superCharge >= 1.0f)
    {
        w->controller.aimingSuper = true;
    }
    else if (w->controller.aimingSuper)
    {
        w->controller.aimingSuper = false;
        if (BrawlerTrySuper(game, idx, aimLen))
            BrawlerFaceShot(game, idx, b->aimAngle, 0.4f);
    }

    //--- Main attack: hold to preview, release to fire; a quick tap auto-aims ----
    if (input->attackPressed)
    {
        w->controller.charging = true;
        w->controller.chargeTime = 0.0f;
    }

    if (w->controller.charging) w->controller.chargeTime += dt;

    if (input->attackReleased && w->controller.charging)
    {
        w->controller.charging = false;
        bool tap = w->controller.chargeTime < TAP_THRESHOLD;
        if (FireMainAttack(w, idx, tap))
            w->controller.attackBufferTimer = 0.0f;
        else
        {
            // Missed by a cooldown hair (or an ammo pip mid-refill): remember the
            // release briefly and keep retrying, so clicks near the edge still fire.
            w->controller.attackBufferTimer = w->tune.inputBuffer;
            w->controller.attackBufferTap = tap;
        }
    }
    else if (w->controller.attackBufferTimer > 0.0f && !w->controller.charging)
    {
        w->controller.attackBufferTimer -= dt;
        if (FireMainAttack(w, idx, w->controller.attackBufferTap) ||
            w->controller.attackBufferTimer <= 0.0f)
            w->controller.attackBufferTimer = 0.0f;
    }

    // Space is a straight auto-aim shot, handy while repositioning.
    if (input->autoAttackPressed)
    {
        int target = AutoAimTarget(w, idx);
        if (target >= 0)
        {
            Vector3 d = Vector3Subtract(w->session.brawlers[target].position, b->position);
            d.y = 0.0f;
            b->aimAngle = atan2f(d.x, d.z);
            if (BrawlerTryAttack(game, idx, Vector3Length(d)))
                BrawlerFaceShot(game, idx, b->aimAngle, 0.45f);
        }
        else if (BrawlerTryAttack(game, idx, aimLen))
            BrawlerFaceShot(game, idx, b->aimAngle, 0.25f);
    }

    b->deliberateAim = w->controller.charging || w->controller.aimingSuper;
}
