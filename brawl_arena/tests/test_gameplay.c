#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "game_events.h"
#include "game_random.h"
#include "map_content.h"
#include "player.h"
#include "weapons.h"

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

int main(void)
{
    App first, replay;
    CHECK(Initialize(&first) && Initialize(&replay), "could not initialize deterministic sessions");
    int emitted = 0;

    for (int frame = 0; frame < 120; frame++)
    {
        PlayerInput input = { 0 };
        input.selectedClass = -1;
        input.aimPoint = first.session.brawlers[1].position;
        input.moveIntent = frame < 45 ? (Vector3){ 1.0f, 0.0f, 0.0f } : (Vector3){ 0 };
        input.attackPressed = frame == 0;
        input.attackReleased = frame == 10;
        input.mobilityPressed = frame == 35;

        PlayerInput replayInput = input;
        replayInput.aimPoint = replay.session.brawlers[1].position;
        PlayerUpdate(&first, &input, 1.0f/60.0f);
        PlayerUpdate(&replay, &replayInput, 1.0f/60.0f);
        BrawlersUpdate(AppGameContext(&first), 1.0f/60.0f);
        BrawlersUpdate(AppGameContext(&replay), 1.0f/60.0f);
        ProjectilesUpdate(AppGameContext(&first), 1.0f/60.0f);
        ProjectilesUpdate(AppGameContext(&replay), 1.0f/60.0f);

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

    puts("deterministic input replay and simulation-event isolation passed");
    return 0;
}
