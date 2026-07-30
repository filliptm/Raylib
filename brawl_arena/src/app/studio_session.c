#include "studio_session.h"

#include "arena.h"
#include "brawler.h"
#include "weapons.h"
#include "game_events.h"
#include "game_random.h"
#include "content_catalog.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define STUDIO_STAGE_SIZE 15
#define STUDIO_STAGE_ID "studio_stage"
#define STUDIO_DUMMY_CLASS CLASS_BRUISER

static MapDefinition StudioStageMap(void)
{
    MapDefinition map = { 0 };
    map.formatVersion = 1;
    snprintf(map.id, sizeof(map.id), "%s", STUDIO_STAGE_ID);
    snprintf(map.name, sizeof(map.name), "VFX Studio");
    map.width = STUDIO_STAGE_SIZE;
    map.height = STUDIO_STAGE_SIZE;
    map.tileSize = 2.0f;

    for (int tz = 0; tz < map.height; tz++)
        for (int tx = 0; tx < map.width; tx++)
        {
            map.terrain[tz][tx] = '.';
            map.gameplay[tz][tx] = '.';
        }

    int mid = STUDIO_STAGE_SIZE/2;
    map.gameplay[mid][mid] = 'P';
    map.gameplay[mid - 3][mid] = 'E';
    map.gameplay[mid + 3][mid] = 'V';
    return map;
}

static Vector3 StudioDummySpot(const StudioSession *studio)
{
    return (Vector3){ 0.0f, 0.0f, studio->dummyDistance };
}

void StudioSessionEnter(App *app, StudioSession *studio)
{
    if (studio->interval <= 0.0f)
    {
        studio->cls = (BrawlerClass)app->tune.selectedKit;
        if (studio->cls < 0 || studio->cls >= CLASS_COUNT)
            studio->cls = CLASS_SHOTGUNNER;
        studio->slot = STUDIO_SLOT_MAIN;
        studio->interval = 1.6f;
        studio->dummyDistance = 8.0f;
        studio->dummyEnabled = true;
        studio->timeScale = 1.0f;
    }

    memset(&app->session, 0, sizeof(app->session));
    memset(&app->controller, 0, sizeof(app->controller));
    memset(&app->presentation, 0, sizeof(app->presentation));
    app->session.sandbox = true;
    GameRandomSeed(&app->session.random, 0x57D10u);

    MapDefinition stage = StudioStageMap();
    ArenaLoad(&app->session.arena, &stage, app->tune.crateHealth);

    GameContext game = AppGameContext(app);
    app->session.playerIdx = 0;
    BrawlerSpawn(game, 0, TEAM_PLAYER, studio->cls, (Vector3){ 0 }, true);
    app->session.brawlerCount = 1;

    if (studio->dummyEnabled)
    {
        BrawlerSpawn(game, app->session.brawlerCount, TEAM_ENEMY,
                     STUDIO_DUMMY_CLASS, StudioDummySpot(studio), false);
        app->session.brawlerCount++;
    }
    if (studio->allyEnabled)
    {
        // Beside the enemy dummy so fields and cones catch both teams at once.
        Vector3 spot = { 2.4f, 0.0f, studio->dummyDistance*0.85f };
        BrawlerSpawn(game, app->session.brawlerCount, TEAM_PLAYER,
                     STUDIO_DUMMY_CLASS, spot, false);
        app->session.brawlerCount++;
    }

    studio->castTimer = 0.6f;
    studio->pendingStep = 0.0f;
}

bool StudioSessionActive(const App *app)
{
    return strcmp(app->session.arena.mapId, STUDIO_STAGE_ID) == 0;
}

static void StudioFire(App *app, StudioSession *studio)
{
    GameContext game = AppGameContext(app);
    Brawler *b = &app->session.brawlers[0];
    if (!b->alive) return;

    // The studio loops presentation, not economy: resources are always stocked.
    const CharacterDefinition *character = ContentCharacter(&app->content, b->cls);
    if (character) b->ammo = (float)character->maxAmmo;
    b->attackCd = 0.0f;
    b->mobilityCooldown = 0.0f;

    Vector3 aim = { 0.0f, 0.0f, 1.0f };
    float aimDist = studio->dummyDistance;
    if (studio->dummyEnabled && app->session.brawlerCount > 1 &&
        app->session.brawlers[1].alive)
    {
        Vector3 to = Vector3Subtract(app->session.brawlers[1].position, b->position);
        to.y = 0.0f;
        float length = Vector3Length(to);
        if (length > 0.05f)
        {
            aim = Vector3Scale(to, 1.0f/length);
            aimDist = length;
        }
    }
    b->aimAngle = atan2f(aim.x, aim.z);

    switch (studio->slot)
    {
        case STUDIO_SLOT_SUPER:
            b->superCharge = 1.0f;
            BrawlerTrySuper(game, 0, aimDist);
            break;
        case STUDIO_SLOT_SECONDARY:
            // Shield-style secondaries gate on their rearm/lockout state; the loop
            // clears it so every cycle shows the raise.
            if (b->shieldActive) BrawlerReleaseShield(game, 0);
            b->shieldRearmRequired = false;
            b->shieldBrokenTimer = 0.0f;
            BrawlerTrySecondary(game, 0, aim);
            break;
        default:
            BrawlerTryAttack(game, 0, aimDist);
            break;
    }
    BrawlerFaceShot(game, 0, b->aimAngle, 0.4f);
}

float StudioSessionTick(App *app, StudioSession *studio, float realDt)
{
    float dt = studio->paused ? studio->pendingStep : realDt*studio->timeScale;
    studio->pendingStep = 0.0f;
    if (dt <= 0.0f) return 0.0f;
    if (dt > 0.05f) dt = 0.05f;

    GameContext game = AppGameContext(app);
    app->session.time += dt;

    studio->castTimer -= dt;
    if (studio->castTimer <= 0.0f)
    {
        StudioFire(app, studio);
        studio->castTimer += fmaxf(studio->interval, 0.2f);
    }

    // Hold-style shields lower themselves partway through the cycle so the next
    // cast always shows the full raise.
    Brawler *b = &app->session.brawlers[0];
    if (b->shieldActive && studio->castTimer < studio->interval*0.35f)
        BrawlerReleaseShield(game, 0);

    b->moveIntent = (Vector3){ 0 };
    BrawlersUpdate(game, dt);
    ProjectilesUpdate(game, dt);
    ArenaUpdate(&app->session.arena, dt);

    // The friendly dummy is held at low health so every heal pulse has something
    // to restore; the spring-back keeps knockback readable without drift.
    for (int i = 1; i < app->session.brawlerCount; i++)
    {
        Brawler *ally = &app->session.brawlers[i];
        if (ally->team != TEAM_PLAYER) continue;
        if (!ally->alive)
            BrawlerSpawn(game, i, TEAM_PLAYER, STUDIO_DUMMY_CLASS,
                         (Vector3){ 2.4f, 0.0f, studio->dummyDistance*0.85f },
                         false);
        int floor = (int)(ally->maxHealth*0.45f);
        if (ally->health > floor) ally->health = floor;
        ally->moveIntent = (Vector3){ 0 };
    }

    // The dummy soaks hits forever: stand it back up if a burst finished it and
    // top it up after the damage numbers have already been emitted.
    for (int i = 1; studio->dummyEnabled && i < app->session.brawlerCount; i++)
    {
        Brawler *dummy = &app->session.brawlers[i];
        if (dummy->team != TEAM_ENEMY) continue;
        if (!dummy->alive)
            BrawlerSpawn(game, i, TEAM_ENEMY, STUDIO_DUMMY_CLASS,
                         StudioDummySpot(studio), false);
        else dummy->health = dummy->maxHealth;
        dummy->moveIntent = (Vector3){ 0 };

        // Knockback still reads, but the dummy springs back to its mark so the
        // loop never slowly shoves it out of frame.
        Vector3 spot = StudioDummySpot(studio);
        Vector3 back = Vector3Subtract(spot, dummy->position);
        back.y = 0.0f;
        float drift = Vector3Length(back);
        if (drift > 0.01f)
        {
            float pull = fminf(drift, 6.0f*dt);
            dummy->position = Vector3Add(dummy->position,
                                         Vector3Scale(back, pull/drift));
        }
    }
    return dt;
}
