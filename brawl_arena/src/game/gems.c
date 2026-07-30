/*******************************************************************************************
*   GEM GRAB
*
*   The match layer: a vent at the contested centre coughs up gems, brawlers carry what
*   they walk over, and everything they were carrying scatters when they go down.
*
*   A team holding the target count starts a countdown. Drop below it at any point and
*   the clock resets entirely - holding the lead is the whole game, not reaching it.
********************************************************************************************/
#include "gems.h"
#include "arena.h"
#include "game_events.h"
#include "raymath.h"
#include "game_random.h"
#include <math.h>

#define GEM_PICKUP_RADIUS 1.15f
#define GEM_SETTLE_TIME 0.45f
#define GEM_DROP_GRACE 0.55f
#define GEM_GRAVITY 18.0f

//------------------------------------------------------------------------------------
static const Color GEM_COLOR = { 168, 96, 240, 255 };

void GemSpawnAt(GameContext w, Vector3 position, Vector3 impulse)
{
    for (int i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &w.session->gems[i];
        if (g->active) continue;

        g->position = position;
        g->position.y = 0.55f;
        g->velocity = impulse;
        g->spin = GameRandomInt(&w.session->random, 0, 628)/100.0f;
        g->bobPhase = GameRandomInt(&w.session->random, 0, 628)/100.0f;
        g->settleTimer = GEM_SETTLE_TIME;
        g->pickupDelay = GEM_DROP_GRACE;
        g->active = true;
        return;
    }
}

void GemsDropFrom(GameContext w, int idx)
{
    Brawler *b = &w.session->brawlers[idx];
    if (b->gems <= 0) return;

    int count = b->gems;
    b->gems = 0;

    for (int i = 0; i < count; i++)
    {
        // Fan them outward so a big carrier leaves a scatter worth fighting over
        // rather than a single stack anyone can hoover up in one step.
        float angle = (i/(float)count)*PI*2.0f + GameRandomInt(&w.session->random, -30, 30)/100.0f;
        float speed = 3.4f + GameRandomInt(&w.session->random, 0, 180)/100.0f;
        Vector3 impulse = { sinf(angle)*speed, 4.2f + GameRandomInt(&w.session->random, 0, 120)/100.0f, cosf(angle)*speed };
        GemSpawnAt(w, b->position, impulse);
    }

    GameEmitFloatText(w.session, b->position, TextFormat("-%d", count), GEM_COLOR);
}

int GemsNearestLoose(GameContext w, Vector3 from, float maxDistance)
{
    int best = -1;
    float bestDist = maxDistance;

    for (int i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &w.session->gems[i];
        if (!g->active) continue;
        // A gem still in its pickup delay cannot be collected; chasing it just
        // parks the bot on top of an inert target.
        if (g->pickupDelay > 0.0f) continue;

        float d = Vector3Distance((Vector3){ from.x, 0, from.z },
                                  (Vector3){ g->position.x, 0, g->position.z });
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

//------------------------------------------------------------------------------------
bool MatchIsOver(GameContext w)
{
    return w.tuning->gemGrab && !w.session->sandbox && w.session->match.phase == MATCH_OVER;
}

void MatchReset(GameContext w)
{
    for (int i = 0; i < MAX_GEMS; i++) w.session->gems[i].active = false;
    for (int i = 0; i < w.session->brawlerCount; i++) w.session->brawlers[i].gems = 0;

    w.session->match = (Match){ 0 };
    w.session->match.phase = MATCH_PLAYING;
    w.session->match.countdownTeam = -1;
    w.session->match.winner = -1;
    w.session->match.ventTimer = 2.0f;      // first gem lands early so the match opens fast
}

//------------------------------------------------------------------------------------
static void UpdateGems(GameContext w, float dt)
{
    for (int i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &w.session->gems[i];
        if (!g->active) continue;

        g->spin += dt*2.2f;
        g->bobPhase += dt*3.0f;
        if (g->pickupDelay > 0.0f) g->pickupDelay -= dt;

        // Arc out of a death, then settle to a fixed hover.
        if (g->settleTimer > 0.0f)
        {
            g->settleTimer -= dt;
            g->velocity.y -= GEM_GRAVITY*dt;
            g->position = Vector3Add(g->position, Vector3Scale(g->velocity, dt));

            if (g->position.y < 0.55f || g->settleTimer <= 0.0f)
            {
                g->position.y = 0.55f;
                g->velocity = (Vector3){ 0, 0, 0 };
                g->settleTimer = 0.0f;
                g->position = ArenaResolveCircle(&w.session->arena, g->position, 0.35f);
            }
        }

        if (g->pickupDelay > 0.0f) continue;

        for (int b = 0; b < w.session->brawlerCount; b++)
        {
            Brawler *br = &w.session->brawlers[b];
            if (!br->alive) continue;

            float d = Vector3Distance((Vector3){ br->position.x, 0, br->position.z },
                                      (Vector3){ g->position.x, 0, g->position.z });
            if (d > GEM_PICKUP_RADIUS) continue;

            br->gems++;
            g->active = false;

            GameEmitLight(w.session, g->position, GEM_COLOR, 1.6f, 0.20f);
            GameEmitImpact(w.session, g->position, GEM_COLOR, 7);
            if (br->isPlayer) GameEmitFloatText(w.session, br->position, "+1", GEM_COLOR);
            break;
        }
    }
}

//------------------------------------------------------------------------------------
static void UpdateVent(GameContext w, float dt)
{
    w.session->match.ventTimer -= dt;
    if (w.session->match.ventTimer > 0.0f) return;

    w.session->match.ventTimer = w.tuning->gemVentInterval;

    // A gentle upward pop so a fresh gem is visibly different from a dropped one.
    Vector3 impulse = { GameRandomInt(&w.session->random, -60, 60)/100.0f, 3.0f, GameRandomInt(&w.session->random, -60, 60)/100.0f };
    GemSpawnAt(w, w.session->arena.gemVent, impulse);

    GameEmitLight(w.session, w.session->arena.gemVent, GEM_COLOR, 2.2f, 0.35f);
    GameEmitShockwave(w.session, w.session->arena.gemVent, 1.5f, 0.40f, GEM_COLOR);
}

//------------------------------------------------------------------------------------
static void UpdateScore(GameContext w, float dt)
{
    Match *m = &w.session->match;

    m->teamGems[0] = 0;
    m->teamGems[1] = 0;
    for (int i = 0; i < w.session->brawlerCount; i++)
    {
        Brawler *b = &w.session->brawlers[i];
        if (b->alive) m->teamGems[b->team] += b->gems;
    }

    int target = w.tuning->gemsToWin;
    int leader = -1;
    if (m->teamGems[0] >= target && m->teamGems[0] > m->teamGems[1]) leader = 0;
    else if (m->teamGems[1] >= target && m->teamGems[1] > m->teamGems[0]) leader = 1;
    else if (m->teamGems[0] >= target && m->teamGems[0] == m->teamGems[1]) leader = -1;

    if (leader < 0)
    {
        // Losing the lead wipes the clock rather than pausing it: you have to hold the
        // count for the whole countdown, which is what makes the last gem tense.
        if (m->phase == MATCH_COUNTDOWN)
        {
            m->phase = MATCH_PLAYING;
            m->countdownTeam = -1;
            GameEmitMatchShake(w.session, 0.8f);
        }
        return;
    }

    if (m->phase != MATCH_COUNTDOWN || m->countdownTeam != leader)
    {
        m->phase = MATCH_COUNTDOWN;
        m->countdownTeam = leader;
        m->countdown = w.tuning->gemCountdown;
    }

    m->countdown -= dt;
    if (m->countdown <= 0.0f)
    {
        m->phase = MATCH_OVER;
        m->winner = leader;
        m->overTimer = 0.0f;

        // Clear the board as the whistle goes. Gameplay stops updating from here, so a
        // shot left in flight would hang in mid-air and read as a frozen bug.
        for (int i = 0; i < MAX_PROJECTILES; i++) w.session->projectiles[i].active = false;
        for (int i = 0; i < MAX_ABILITY_FIELDS; i++) w.session->abilityFields[i].active = false;
        for (int i = 0; i < w.session->brawlerCount; i++)
        {
            w.session->brawlers[i].velocity = (Vector3){ 0, 0, 0 };
            w.session->brawlers[i].moveIntent = (Vector3){ 0, 0, 0 };
            for (int effect = 0; effect < MAX_STATUS_EFFECTS; effect++)
                w.session->brawlers[i].statuses[effect] = (StatusEffect){ 0 };
        }

        GameEmitMatchShake(w.session, 3.0f);
    }
}

//------------------------------------------------------------------------------------
void MatchUpdate(GameContext w, float dt)
{
    if (!w.tuning->gemGrab || w.session->sandbox) return;

    // Once decided, the only thing still running is the clock on the result screen.
    // No vent, no pickups, no scoring.
    if (w.session->match.phase == MATCH_OVER)
    {
        w.session->match.overTimer += dt;
        return;
    }

    UpdateVent(w, dt);
    UpdateGems(w, dt);
    UpdateScore(w, dt);
}
