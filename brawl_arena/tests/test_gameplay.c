#include "ai.h"
#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "game_commands.h"
#include "game_events.h"
#include "game_random.h"
#include "map_content.h"
#include "player.h"
#include "weapons.h"
#include "raymath.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static bool Initialize(App *app)
{
    *app = (App){ 0 };
    TuningSetDefaults(&app->tune);
    ContentCatalogResetAll(&app->content);
    GameContext game = AppGameContext(app);

    char message[256];
    if (!MapCatalogLoad(&app->content, "data/maps/manifest.cfg", message, sizeof(message)))
    {
        fprintf(stderr, "map catalog: %s\n", message);
        return false;
    }
    ArenaLoad(&app->session.arena, MapCatalogSelected(&app->content), app->tune.crateHealth);
    GameRandomSeed(&app->session.random, 0x12345678u);
    app->session.playerIdx = 0;
    app->session.brawlerCount = 2;
    BrawlerSpawn(game, 0, TEAM_PLAYER, CLASS_BRUISER,
                 ArenaSpawnFor(&app->session.arena, TEAM_PLAYER, 0), true);
    BrawlerSpawn(game, 1, TEAM_ENEMY, CLASS_SHOTGUNNER,
                 ArenaSpawnFor(&app->session.arena, TEAM_ENEMY, 0), false);
    GameEventsClear(&app->session);
    return true;
}

static int SameSimulation(const App *a, const App *b)
{
    return memcmp(a->session.brawlers, b->session.brawlers,
                  sizeof(a->session.brawlers)) == 0 &&
           memcmp(a->session.projectiles, b->session.projectiles,
                  sizeof(a->session.projectiles)) == 0 &&
           memcmp(a->session.abilityFields, b->session.abilityFields,
                  sizeof(a->session.abilityFields)) == 0 &&
           a->session.random.state == b->session.random.state;
}

static int CheckMovementRegressions(void)
{
    App app;
    CHECK(Initialize(&app), "could not initialize movement regression session");
    GameContext game = AppGameContext(&app);

    app.session.brawlerCount = 2;
    Team passThroughTeams[] = { TEAM_ENEMY, TEAM_PLAYER };
    const char *passThroughFailures[] = {
        "opponents could not walk through each other",
        "allies could not walk through each other"
    };
    for (int fixture = 0; fixture < 2; fixture++)
    {
        Brawler *left = &app.session.brawlers[0];
        Brawler *right = &app.session.brawlers[1];
        left->team = TEAM_PLAYER;
        right->team = passThroughTeams[fixture];
        left->position = app.session.arena.gemVent;
        right->position = app.session.arena.gemVent;
        left->velocity = (Vector3){ 0 };
        right->velocity = (Vector3){ 0 };
        left->moveIntent = (Vector3){ 0 };
        right->moveIntent = (Vector3){ 0 };
        BrawlersUpdate(game, 1.0f/60.0f);
        CHECK(Vector3Distance(left->position, right->position) < 0.001f,
              "overlapping brawlers were separated");

        left->position = Vector3Add(
            app.session.arena.gemVent, (Vector3){ -1.25f, 0.0f, 0.0f });
        right->position = Vector3Add(
            app.session.arena.gemVent, (Vector3){ 1.25f, 0.0f, 0.0f });
        left->velocity = (Vector3){ 0 };
        right->velocity = (Vector3){ 0 };
        left->moveIntent = (Vector3){ 1.0f, 0.0f, 0.0f };
        right->moveIntent = (Vector3){ -1.0f, 0.0f, 0.0f };
        for (int frame = 0; frame < 45; frame++)
            BrawlersUpdate(game, 1.0f/60.0f);
        CHECK(left->position.x > right->position.x,
              passThroughFailures[fixture]);
        CHECK(ArenaCircleClear(&app.session.arena, left->position,
                               BRAWLER_RADIUS) &&
              ArenaCircleClear(&app.session.arena, right->position,
                               BRAWLER_RADIUS),
              "actor pass-through bypassed terrain collision");
    }

    // A clamped response cannot overshoot the authored speed during a slow frame.
    app.session.brawlerCount = 1;
    Brawler *mover = &app.session.brawlers[0];
    mover->position = app.session.arena.gemVent;
    mover->velocity = (Vector3){ -app.tune.moveSpeed, 0.0f, 0.0f };
    mover->moveIntent = (Vector3){ 1.0f, 0.0f, 0.0f };
    BrawlersUpdate(game, 0.05f);
    CHECK(mover->velocity.x <= app.tune.moveSpeed + 0.001f,
          "slow-frame acceleration extrapolated past desired speed");

    return 0;
}

static int CheckBotNavigation(void)
{
    App app;
    CHECK(Initialize(&app), "could not initialize navigation regression session");
    GameContext game = AppGameContext(&app);
    Vector3 start = { -28.0f, 0.0f, -20.0f };
    Vector3 goal = { -16.0f, 0.0f, -12.0f };

    app.session.brawlerCount = 1;
    app.session.playerIdx = 0;
    BrawlerSpawn(game, 0, TEAM_ENEMY, CLASS_SHOTGUNNER, start, false);
    app.session.match.phase = MATCH_PLAYING;
    app.session.gems[0] = (Gem){
        .position = { goal.x, 0.55f, goal.z },
        .active = true
    };
    GameEventsClear(&app.session);

    bool reached = false;
    for (int frame = 0; frame < 600; frame++)
    {
        const float dt = 1.0f/60.0f;
        app.session.time += dt;
        AIUpdate(game, dt);
        BrawlersUpdate(game, dt);
        if (Vector3Distance(app.session.brawlers[0].position, goal) < 1.0f)
        {
            reached = true;
            break;
        }
    }

    CHECK(reached, "body-aware bot route stalled against Helios-9 cover");
    CHECK(ArenaCircleClear(&app.session.arena,
                           app.session.brawlers[0].position,
                           BRAWLER_RADIUS),
          "bot navigation ended inside cover");
    return 0;
}

static int CheckClassSwap(void)
{
    App app;
    CHECK(Initialize(&app), "could not initialize class-swap session");

    // Spawned as Bruiser: swap out of combat carries health ratio, super, and gems.
    Brawler *player = &app.session.brawlers[0];
    player->health = player->maxHealth/2;
    player->superCharge = 0.6f;
    player->gems = 4;
    app.session.time = 100.0f;
    player->lastCombatTime = 90.0f;

    CHECK(GameCommandExecute(&app, (GameCommand){
              .type = GAME_COMMAND_SET_PLAYER_CLASS, .value = CLASS_SHOTGUNNER }),
          "out-of-combat class swap was rejected");
    player = &app.session.brawlers[0];
    CHECK(player->cls == CLASS_SHOTGUNNER, "class swap did not apply");
    float ratio = (float)player->health/(float)player->maxHealth;
    CHECK(ratio > 0.45f && ratio < 0.55f,
          "class swap did not carry the health ratio");
    CHECK(player->superCharge == 0.6f && player->gems == 4,
          "class swap dropped super charge or gems");

    // Recent combat blocks the swap outside the sandbox: no more instant heal.
    player->lastCombatTime = app.session.time;
    CHECK(!GameCommandExecute(&app, (GameCommand){
              .type = GAME_COMMAND_SET_PLAYER_CLASS, .value = CLASS_BRUISER }),
          "in-combat class swap was not blocked");
    CHECK(player->cls == CLASS_SHOTGUNNER, "blocked swap still changed class");

    // A dead player only records the class; the respawn timer keeps running.
    player->alive = false;
    player->respawnTimer = 2.5f;
    CHECK(GameCommandExecute(&app, (GameCommand){
              .type = GAME_COMMAND_SET_PLAYER_CLASS, .value = CLASS_BRUISER }),
          "dead class swap failed");
    CHECK(!player->alive && player->respawnTimer == 2.5f,
          "dead class swap revived the player or reset the respawn timer");
    CHECK(player->cls == CLASS_BRUISER, "dead class swap did not record the class");
    return 0;
}

int main(void)
{
    App first, replay;
    CHECK(Initialize(&first) && Initialize(&replay), "could not initialize deterministic sessions");
    int emitted = 0;

    for (int frame = 0; frame < 120; frame++)
    {
        const float dt = 1.0f/60.0f;
        first.session.time += dt;
        replay.session.time += dt;

        PlayerInput input = { 0 };
        input.selectedClass = -1;
        input.aimPoint = first.session.brawlers[1].position;
        input.moveIntent = frame < 45 ? (Vector3){ 1.0f, 0.0f, 0.0f } : (Vector3){ 0 };
        input.attackPressed = frame == 0;
        input.attackReleased = frame == 10;
        input.mobilityPressed = frame == 35;

        PlayerInput replayInput = input;
        replayInput.aimPoint = replay.session.brawlers[1].position;
        PlayerUpdate(&first, &input, dt);
        PlayerUpdate(&replay, &replayInput, dt);
        BrawlersUpdate(AppGameContext(&first), dt);
        BrawlersUpdate(AppGameContext(&replay), dt);
        ProjectilesUpdate(AppGameContext(&first), dt);
        ProjectilesUpdate(AppGameContext(&replay), dt);
        BrawlersUpdateRegeneration(AppGameContext(&first));
        BrawlersUpdateRegeneration(AppGameContext(&replay));

        CHECK(first.session.events.count == replay.session.events.count,
              "replay emitted a different event count");
        CHECK(memcmp(first.session.events.items, replay.session.events.items,
                     sizeof(GameEvent)*(size_t)first.session.events.count) == 0,
              "replay emitted different event payloads");
        emitted += first.session.events.count;
        GameEventsClear(&first.session);
        GameEventsClear(&replay.session);
        CHECK(SameSimulation(&first, &replay), "same commands diverged across deterministic sessions");
    }

    CHECK(emitted > 0, "simulation did not emit presentation events");
    for (int i = 0; i < MAX_PARTICLES; i++)
        CHECK(!first.presentation.particles[i].active,
              "headless simulation mutated presentation particle state");
    CHECK(first.presentation.shake == 0.0f,
          "headless simulation mutated presentation camera state");
    CHECK(CheckMovementRegressions() == 0,
          "movement regression checks failed");
    CHECK(CheckBotNavigation() == 0,
          "navigation regression checks failed");
    CHECK(CheckClassSwap() == 0,
          "class swap checks failed");

    puts("deterministic replay, sliding movement, actor pass-through, and bot routing passed");
    return 0;
}
