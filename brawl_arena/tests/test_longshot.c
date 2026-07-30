#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "game_random.h"
#include "weapons.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void SetupOpenArena(Arena *arena)
{
    memset(arena, 0, sizeof(*arena));
    arena->width = 63;
    arena->height = 63;
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

static int ActiveProjectileCount(GameSession *session,
                                 Projectile **first, Projectile **second)
{
    int count = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (!session->projectiles[i].active) continue;
        if (count == 0 && first) *first = &session->projectiles[i];
        if (count == 1 && second) *second = &session->projectiles[i];
        count++;
    }
    return count;
}

int main(void)
{
    Tuning tuning;
    ContentCatalog content = { 0 };
    GameSession session = { 0 };
    TuningSetDefaults(&tuning);
    ContentCatalogResetAll(&content);
    SetupOpenArena(&session.arena);
    GameRandomSeed(&session.random, 0x7477696eu);

    GameContext game = { &session, &tuning, &content };
    session.playerIdx = 0;
    session.brawlerCount = 2;
    BrawlerSpawn(game, 0, TEAM_PLAYER, CLASS_SNIPER,
                 (Vector3){ 0.0f, 0.0f, 0.0f }, true);
    BrawlerSpawn(game, 1, TEAM_ENEMY, CLASS_BRUISER,
                 (Vector3){ 0.0f, 0.0f, 12.0f }, false);

    const AbilityDefinition *main =
        ContentMainAbility(&content, CLASS_SNIPER);
    CHECK(main && main->behavior == ABILITY_BEHAVIOR_PROJECTILE &&
          main->data.projectile.pellets == 2 &&
          main->data.projectile.spreadDegrees == 0.0f &&
          main->damage == 800 &&
          fabsf(main->superPerHit - 0.15f) < 0.0001f,
          "Longshot fallback content does not preserve the paired-shot totals");

    Brawler *longshot = &session.brawlers[0];
    Brawler *target = &session.brawlers[1];
    longshot->aimAngle = 0.0f;
    WeaponsFire(game, 0, false, main->range);

    Projectile *left = NULL;
    Projectile *right = NULL;
    CHECK(ActiveProjectileCount(&session, &left, &right) == 2 &&
          left && right,
          "one Longshot cast did not create exactly two projectiles");
    CHECK(left->damage + right->damage == 1600,
          "fallback paired bolts changed Longshot's combined base damage");
    CHECK(fabsf(left->velocity.x - right->velocity.x) < 0.0001f &&
          fabsf(left->velocity.z - right->velocity.z) < 0.0001f,
          "Longshot's paired bolts are not traveling in parallel");

    float separation = Vector3Distance(left->position, right->position);
    CHECK(separation > 0.20f && separation < 0.25f &&
          fabsf((left->position.x + right->position.x)*0.5f -
                longshot->position.x) < 0.0001f,
          "Longshot's paired bolts are not tightly centered side by side");

    int healthBefore = target->health;
    for (int frame = 0; frame < 240; frame++)
    {
        ProjectilesUpdate(game, 1.0f/240.0f);
        if (ActiveProjectileCount(&session, NULL, NULL) == 0) break;
    }
    CHECK(target->health < healthBefore,
          "Longshot's paired bolts did not damage a centered target");
    CHECK(fabsf(longshot->superCharge - 0.30f) < 0.0001f,
          "two paired hits changed Longshot's combined super gain");

    puts("Longshot tight twin-shot damage, spacing, trajectory, and charge passed");
    return 0;
}
