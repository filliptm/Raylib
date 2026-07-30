#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "game_random.h"
#include "weapons.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void SetupOpenArena(Arena *arena)
{
    memset(arena, 0, sizeof(*arena));
    arena->width = 31;
    arena->height = 31;
    arena->tileSize = 1.0f;

    for (int z = 0; z < arena->height; z++)
    {
        for (int x = 0; x < arena->width; x++)
        {
            bool edge = x == 0 || z == 0 ||
                        x == arena->width - 1 ||
                        z == arena->height - 1;
            arena->tiles[z][x].type = edge ? TILE_WALL : TILE_FLOOR;
        }
    }
}

static void SetupBrawler(const ContentCatalog *content, Brawler *brawler,
                         Team team, BrawlerClass cls, Vector3 position,
                         bool player)
{
    *brawler = (Brawler){ 0 };
    brawler->position = position;
    brawler->team = team;
    brawler->cls = cls;
    brawler->isPlayer = player;
    brawler->maxHealth = ContentCharacter(content, cls)->maxHealth;
    brawler->health = brawler->maxHealth;
    brawler->ammo = (float)ContentCharacter(content, cls)->maxAmmo;
    brawler->alive = true;
    brawler->visible = true;
    brawler->spawnScale = 1.0f;
    brawler->dashAbility = -1;
    brawler->grappleAbility = -1;
    brawler->shieldAbility = -1;
    brawler->aiTarget = -1;
}

static void SetupSession(GameSession *session, const ContentCatalog *content,
                         BrawlerClass ownerClass)
{
    memset(session, 0, sizeof(*session));
    SetupOpenArena(&session->arena);
    GameRandomSeed(&session->random, 0x6d696e65u);
    session->brawlerCount = 3;
    session->playerIdx = 0;
    SetupBrawler(content, &session->brawlers[0], TEAM_PLAYER, ownerClass,
                 (Vector3){ 0.0f, 0.0f, 0.0f }, true);
    SetupBrawler(content, &session->brawlers[1], TEAM_ENEMY,
                 CLASS_SHOTGUNNER,
                 (Vector3){ 8.0f, 0.0f, 0.0f }, false);
    SetupBrawler(content, &session->brawlers[2], TEAM_PLAYER,
                 CLASS_SHOTGUNNER,
                 (Vector3){ 1.0f, 0.0f, 0.0f }, false);
}

static void AdvanceBrawlers(GameContext game, float seconds)
{
    int frames = (int)ceilf(seconds*120.0f);
    for (int frame = 0; frame < frames; frame++)
        BrawlersUpdate(game, 1.0f/120.0f);
}

static void AdvanceFields(GameContext game, float seconds)
{
    int frames = (int)ceilf(seconds*120.0f);
    for (int frame = 0; frame < frames; frame++)
        ProjectilesUpdate(game, 1.0f/120.0f);
}

static int ActiveMineCount(const GameSession *session, int owner)
{
    int count = 0;
    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
        if (session->abilityFields[i].active &&
            session->abilityFields[i].type == ABILITY_FIELD_MINE &&
            session->abilityFields[i].owner == owner)
            count++;
    return count;
}

int main(void)
{
    Tuning tuning;
    ContentCatalog content = { 0 };
    GameSession session;
    TuningSetDefaults(&tuning);
    ContentCatalogResetAll(&content);

    const AbilityDefinition *grapple =
        ContentSecondaryAbility(&content, CLASS_SNIPER);
    CHECK(grapple && grapple->behavior == ABILITY_BEHAVIOR_GRAPPLE &&
          fabsf(grapple->cooldown - 7.5f) < 0.0001f &&
          fabsf(grapple->range - 10.0f) < 0.0001f &&
          fabsf(grapple->data.grapple.launchDelay - 0.25f) < 0.0001f &&
          fabsf(grapple->data.grapple.pullDuration - 0.45f) < 0.0001f,
          "Longshot Mag-Line Grapple typed content is incomplete");

    const AbilityDefinition *mine =
        ContentSecondaryAbility(&content, CLASS_LOBBER);
    CHECK(mine && mine->behavior == ABILITY_BEHAVIOR_MINE &&
          fabsf(mine->cooldown - 8.0f) < 0.0001f &&
          fabsf(mine->data.mine.armTime - 0.55f) < 0.0001f &&
          fabsf(mine->data.mine.triggerRadius - 2.4f) < 0.0001f &&
          fabsf(mine->radius - 3.2f) < 0.0001f &&
          mine->damage == 400 &&
          fabsf(mine->data.mine.knockback - 4.5f) < 0.0001f,
          "Mortar Concussion Mine typed content is incomplete");

    // Grapple locks actions during launch/pull, crosses open space, and spends its
    // cooldown at activation.
    SetupSession(&session, &content, CLASS_SNIPER);
    GameContext game = { &session, &tuning, &content };
    Brawler *longshot = &session.brawlers[0];
    session.brawlerCount = 1;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "ready grapple did not activate");
    CHECK(BrawlerIsGrappling(longshot) &&
          fabsf(longshot->mobilityCooldown - 7.5f) < 0.0001f,
          "grapple did not enter its timed state and cooldown");
    CHECK(!BrawlerTryAttack(game, 0, 5.0f) &&
          !BrawlerTrySuper(game, 0, 5.0f),
          "grapple did not lock attacks during traversal");
    AdvanceBrawlers(game, 0.20f);
    CHECK(fabsf(longshot->position.z) < 0.01f,
          "grapple moved before its launch delay elapsed");
    AdvanceBrawlers(game, 0.55f);
    CHECK(!BrawlerIsGrappling(longshot) &&
          fabsf(longshot->position.z - 10.0f) < 0.08f,
          "grapple did not complete its open-space pull");
    CHECK(!BrawlerTrySecondary(game, 0, (Vector3){ 0.0f, 0.0f, -1.0f }),
          "grapple ignored its cooldown");
    AdvanceBrawlers(game, 6.90f);
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0.0f, 0.0f, -1.0f }),
          "grapple did not become ready after its cooldown");

    // A body-safe grapple endpoint stops before permanent or destructible cover and
    // does not change the cover tile.
    SetupSession(&session, &content, CLASS_SNIPER);
    game.session = &session;
    longshot = &session.brawlers[0];
    session.brawlerCount = 1;
    int coverX = ArenaTileX(&session.arena, 0.0f);
    int coverZ = ArenaTileZ(&session.arena, 4.0f);
    session.arena.tiles[coverZ][coverX] =
        (Tile){ .type = TILE_CRATE, .health = tuning.crateHealth };
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "cover-limited grapple did not activate");
    AdvanceBrawlers(game, 0.70f);
    CHECK(longshot->position.z < 3.0f &&
          session.arena.tiles[coverZ][coverX].type == TILE_CRATE &&
          session.arena.tiles[coverZ][coverX].health == tuning.crateHealth,
          "grapple crossed or damaged cover");

    // External knockback uses the common displacement path and cancels the pull.
    SetupSession(&session, &content, CLASS_SNIPER);
    game.session = &session;
    longshot = &session.brawlers[0];
    session.brawlerCount = 1;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 1.0f, 0.0f, 0.0f }),
          "displacement-cancel grapple did not activate");
    BrawlerDisplace(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f });
    CHECK(!BrawlerIsGrappling(longshot) &&
          fabsf(longshot->position.z - 1.0f) < 0.02f &&
          longshot->mobilityCooldown > 7.4f,
          "hard displacement did not cancel grapple while preserving cooldown");

    // Mine arms without reacting to allies, then damages and pushes enemies once
    // they enter its trigger radius.
    SetupSession(&session, &content, CLASS_LOBBER);
    game.session = &session;
    Brawler *mortar = &session.brawlers[0];
    Brawler *enemy = &session.brawlers[1];
    bool armed = true;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0 }),
          "ready mine did not deploy");
    CHECK(WeaponsMineActive(game, 0, &armed) && !armed &&
          fabsf(mortar->mobilityCooldown - 8.0f) < 0.0001f,
          "mine did not enter its arming state and cooldown");
    AdvanceFields(game, 0.56f);
    CHECK(WeaponsMineActive(game, 0, &armed) && armed,
          "mine did not arm after its configured delay");
    AdvanceFields(game, 0.20f);
    CHECK(WeaponsMineActive(game, 0, NULL),
          "mine triggered on its nearby ally");
    enemy->position = (Vector3){ 1.5f, 0.0f, 0.0f };
    int healthBefore = enemy->health;
    AdvanceFields(game, 0.02f);
    CHECK(!WeaponsMineActive(game, 0, NULL) &&
          enemy->health == healthBefore - 400 &&
          enemy->position.x > 5.8f,
          "armed mine did not damage and knock back its enemy");

    // One mine per owner: once the cooldown expires, placing another replaces the
    // persistent old mine rather than consuming another fixed-pool slot.
    SetupSession(&session, &content, CLASS_LOBBER);
    game.session = &session;
    mortar = &session.brawlers[0];
    session.brawlers[1].position = (Vector3){ 9.0f, 0.0f, 0.0f };
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0 }),
          "replacement test mine did not deploy");
    AdvanceBrawlers(game, 8.05f);
    mortar->position = (Vector3){ 4.0f, 0.0f, 0.0f };
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0 }),
          "mine did not become replaceable after cooldown");
    CHECK(ActiveMineCount(&session, 0) == 1,
          "placing a new mine did not replace the owner's old mine");
    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
        if (session.abilityFields[i].active &&
            session.abilityFields[i].type == ABILITY_FIELD_MINE)
            CHECK(fabsf(session.abilityFields[i].position.x - 4.0f) < 0.01f,
                  "replacement mine was not placed at the owner's new position");

    // Cover blocks both mine detection and blast damage.
    SetupSession(&session, &content, CLASS_LOBBER);
    game.session = &session;
    enemy = &session.brawlers[1];
    enemy->position = (Vector3){ 2.2f, 0.0f, 0.0f };
    int wallX = ArenaTileX(&session.arena, 1.0f);
    int wallZ = ArenaTileZ(&session.arena, 0.0f);
    session.arena.tiles[wallZ][wallX].type = TILE_WALL;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0 }),
          "line-of-sight mine did not deploy");
    healthBefore = enemy->health;
    AdvanceFields(game, 0.80f);
    CHECK(WeaponsMineActive(game, 0, NULL) &&
          enemy->health == healthBefore,
          "mine detected or damaged an enemy through permanent cover");

    // Owner death removes the persistent field on the next simulation update.
    session.brawlers[0].alive = false;
    AdvanceFields(game, 0.02f);
    CHECK(!WeaponsMineActive(game, 0, NULL),
          "mine survived its owner's death");

    puts("Longshot Grapple and Mortar Concussion Mine behavior passed");
    return 0;
}
