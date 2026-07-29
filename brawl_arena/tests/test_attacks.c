#include "attack_content.h"
#include "content_catalog.h"
#include "game_events.h"
#include "studio_session.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void Initialize(App *app)
{
    *app = (App){ 0 };
    TuningSetDefaults(&app->tune);
    ContentCatalogResetAll(&app->content);
    AttackContentDefaults(&app->content);
}

static int CountEvents(const App *app, GameEventType type)
{
    int total = 0;
    for (int i = 0; i < app->session.events.count; i++)
        if (app->session.events.items[i].type == type) total++;
    return total;
}

int main(void)
{
    App app;
    Initialize(&app);

    int mainAbility = app.content.characters[CLASS_SHOTGUNNER].mainAbility;
    CHECK(mainAbility >= 0 && mainAbility < app.content.abilityCount,
          "scrapper main ability missing");
    CHECK(!AttackAuthored(&app.content, mainAbility),
          "default catalog should have no authored attacks");

    //--- Template: never a blank page ------------------------------------------
    AttackPresentation doc;
    AttackPresentationTemplate(&app.content, mainAbility, &doc);
    CHECK(doc.authored, "template is not authored");
    int usedLayers = 0;
    for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
        if (doc.layers[i].used) usedLayers++;
    CHECK(usedLayers >= 3, "template should provide starter layers");

    char message[256] = { 0 };
    app.content.attacks[mainAbility] = doc;
    CHECK(AttackContentValidate(&app.content, message, sizeof(message)), message);

    //--- Validation rejects a poisoned layer ------------------------------------
    app.content.attacks[mainAbility].layers[0].duration = 0.0f;
    CHECK(!AttackContentValidate(&app.content, message, sizeof(message)),
          "zero-duration layer unexpectedly validated");
    app.content.attacks[mainAbility].layers[0].duration = 0.3f;

    //--- Save / load round trip -------------------------------------------------
    char directory[128], path[256];
    snprintf(directory, sizeof(directory), "/tmp/brawl-attacks-%ld", (long)getpid());
    CHECK(mkdir(directory, 0700) == 0 || errno == EEXIST, "mkdir failed");
    snprintf(path, sizeof(path), "%s/presentation.cfg", directory);

    app.content.attacks[mainAbility].layers[1].colorStart = (Color){ 12, 34, 56, 200 };
    app.content.attacks[mainAbility].projectile.trailLength = 0.4f;
    app.content.attacks[mainAbility].projectile.spin = 2.5f;
    app.content.attacks[mainAbility].motions[0] =
        (AttackMotion){ .used = true, .kind = ATTACK_MOTION_RECOIL,
                        .delay = 0.02f, .duration = 0.3f, .amplitude = 1.2f };
    CHECK(AttackContentSaveFile(&app.content, path, message, sizeof(message)), message);

    App reload;
    Initialize(&reload);
    CHECK(AttackContentLoadFile(&reload.content, path, message, sizeof(message)), message);
    CHECK(AttackAuthored(&reload.content, mainAbility), "authored flag lost in round trip");
    const AttackPresentation *loaded = &reload.content.attacks[mainAbility];
    CHECK(loaded->layers[1].used &&
          loaded->layers[1].colorStart.r == 12 &&
          loaded->layers[1].colorStart.g == 34 &&
          loaded->layers[1].colorStart.b == 56 &&
          loaded->layers[1].colorStart.a == 200,
          "layer color did not round trip");
    CHECK(loaded->layers[2].pattern == ATTACK_PATTERN_BURST &&
          loaded->layers[2].count == 6,
          "burst layer did not round trip");
    CHECK(loaded->projectile.trailLength == 0.4f &&
          loaded->projectile.spin == 2.5f,
          "projectile visuals did not round trip");
    CHECK(loaded->motions[0].used &&
          loaded->motions[0].kind == ATTACK_MOTION_RECOIL &&
          loaded->motions[0].amplitude == 1.2f,
          "motion did not round trip");

    //--- A missing file is fine; a malformed one is rejected untouched ----------
    char absent[256];
    snprintf(absent, sizeof(absent), "%s/none.cfg", directory);
    CHECK(AttackContentLoadFile(&reload.content, absent, message, sizeof(message)),
          "missing file should be a no-op success");
    char bad[256];
    snprintf(bad, sizeof(bad), "%s/bad.cfg", directory);
    FILE *file = fopen(bad, "w");
    fprintf(file, "attack scrapper.main\nlayer 99 anchor=cast\n");
    fclose(file);
    CHECK(!AttackContentLoadFile(&reload.content, bad, message, sizeof(message)),
          "out-of-range layer slot unexpectedly loaded");
    CHECK(AttackAuthored(&reload.content, mainAbility),
          "failed load damaged existing documents");

    //--- Authored gating in the live sim ----------------------------------------
    // Unauthored: the legacy muzzle event fires. Authored: replaced by ATTACK_CAST,
    // and projectile impacts arrive as ATTACK_IMPACT.
    StudioSession studio = { 0 };
    StudioSessionEnter(&app, &studio);
    studio.cls = CLASS_SHOTGUNNER;
    app.content.attacks[mainAbility].authored = false;
    StudioSessionEnter(&app, &studio);

    int legacyMuzzle = 0, authoredCasts = 0, authoredImpacts = 0;
    for (int frame = 0; frame < 240; frame++)
    {
        StudioSessionTick(&app, &studio, 1.0f/60.0f);
        legacyMuzzle += CountEvents(&app, GAME_EVENT_MUZZLE);
        authoredCasts += CountEvents(&app, GAME_EVENT_ATTACK_CAST);
        GameEventsClear(&app.session);
    }
    CHECK(legacyMuzzle > 0, "unauthored ability stopped emitting legacy muzzle");
    CHECK(authoredCasts == 0, "unauthored ability emitted authored casts");

    app.content.attacks[mainAbility].authored = true;
    StudioSessionEnter(&app, &studio);
    legacyMuzzle = 0;
    for (int frame = 0; frame < 600; frame++)
    {
        StudioSessionTick(&app, &studio, 1.0f/60.0f);
        legacyMuzzle += CountEvents(&app, GAME_EVENT_MUZZLE);
        authoredCasts += CountEvents(&app, GAME_EVENT_ATTACK_CAST);
        authoredImpacts += CountEvents(&app, GAME_EVENT_ATTACK_IMPACT);
        GameEventsClear(&app.session);
    }
    CHECK(authoredCasts > 0, "authored ability emitted no cast events");
    CHECK(authoredImpacts > 0, "authored ability emitted no impact events");
    CHECK(legacyMuzzle == 0, "authored ability still emitted the legacy muzzle");

    puts("attack templates, validation, round trip, and authored gating passed");
    return 0;
}
