#include "studio_session.h"
#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "game_events.h"
#include "raymath.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void Initialize(App *app)
{
    *app = (App){ 0 };
    TuningSetDefaults(&app->tune);
    ContentCatalogResetAll(&app->content);
}

// Runs frames, counting events by type and clearing the queue each frame the way
// the presentation consumer does.
static void RunFrames(App *app, StudioSession *studio, int frames,
                      int *casts, int *impacts, bool *sawProjectile)
{
    for (int frame = 0; frame < frames; frame++)
    {
        StudioSessionTick(app, studio, 1.0f/60.0f);
        for (int i = 0; i < app->session.events.count; i++)
        {
            GameEventType type = app->session.events.items[i].type;
            if (type == GAME_EVENT_MUZZLE && casts) (*casts)++;
            if (type == GAME_EVENT_IMPACT && impacts) (*impacts)++;
        }
        if (sawProjectile)
            for (int i = 0; i < MAX_PROJECTILES; i++)
                if (app->session.projectiles[i].active) { *sawProjectile = true; break; }
        GameEventsClear(&app->session);
    }
}

int main(void)
{
    App app;
    Initialize(&app);

    StudioSession studio = { 0 };
    StudioSessionEnter(&app, &studio);

    CHECK(StudioSessionActive(&app), "studio stage was not active after enter");
    CHECK(app.session.brawlerCount == 2, "stage should hold character and dummy");
    CHECK(app.session.brawlers[0].isPlayer, "slot 0 should be the studio character");
    CHECK(!app.session.brawlers[1].isPlayer &&
          app.session.brawlers[1].team == TEAM_ENEMY,
          "slot 1 should be the enemy dummy");
    CHECK(app.session.sandbox, "the studio stage must count as a sandbox");

    //--- The metronome fires and produces real projectiles, hits, and events ----
    int casts = 0, impacts = 0;
    bool sawProjectile = false;
    RunFrames(&app, &studio, 600, &casts, &impacts, &sawProjectile);
    CHECK(casts >= 4, "metronome did not fire repeatedly");
    CHECK(sawProjectile, "cast produced no live projectile");
    CHECK(impacts > 0, "no impacts were emitted against the dummy");
    CHECK(app.session.brawlers[1].alive &&
          app.session.brawlers[1].health == app.session.brawlers[1].maxHealth,
          "dummy did not recover between hits");
    CHECK(app.session.brawlers[0].ammo >= 1.0f,
          "the studio loop ran the character out of ammo");

    //--- Pause stops the clock; a queued step advances exactly once -------------
    studio.paused = true;
    float paused = StudioSessionTick(&app, &studio, 1.0f/60.0f);
    CHECK(paused == 0.0f, "paused studio still advanced time");
    studio.pendingStep = 1.0f/60.0f;
    float stepped = StudioSessionTick(&app, &studio, 1.0f/60.0f);
    CHECK(stepped > 0.0f, "queued step did not advance the stage");
    float again = StudioSessionTick(&app, &studio, 1.0f/60.0f);
    CHECK(again == 0.0f, "step advanced more than one frame");
    studio.paused = false;

    //--- Slow motion scales the simulated dt ------------------------------------
    studio.timeScale = 0.25f;
    float slow = StudioSessionTick(&app, &studio, 1.0f/60.0f);
    CHECK(slow > 0.003f && slow < 0.005f, "time scale did not slow the stage");
    studio.timeScale = 1.0f;

    //--- Super slot fires the super and recharges it every loop -----------------
    studio.slot = STUDIO_SLOT_SUPER;
    StudioSessionEnter(&app, &studio);
    int superCasts = 0;
    RunFrames(&app, &studio, 400, &superCasts, NULL, NULL);
    CHECK(superCasts >= 2, "super metronome did not fire repeatedly");

    //--- Dummy off leaves a single-actor stage ----------------------------------
    studio.dummyEnabled = false;
    StudioSessionEnter(&app, &studio);
    CHECK(app.session.brawlerCount == 1, "dummy toggle left extra actors");
    RunFrames(&app, &studio, 200, NULL, NULL, NULL);
    CHECK(app.session.brawlers[0].alive, "solo stage harmed the character");

    puts("studio stage, metronome, dummy recovery, pause/step, and slots passed");
    return 0;
}
