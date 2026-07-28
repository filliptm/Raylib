#include "arena.h"
#include "map_content.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return false; } \
} while (0)

static bool Reachable(const Arena *arena, Vector3 start, Vector3 goal)
{
    int sx = ArenaTileX(arena, start.x), sz = ArenaTileZ(arena, start.z);
    int gx = ArenaTileX(arena, goal.x), gz = ArenaTileZ(arena, goal.z);
    bool seen[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH] = { 0 };
    short qx[MAX_ARENA_WIDTH*MAX_ARENA_HEIGHT];
    short qz[MAX_ARENA_WIDTH*MAX_ARENA_HEIGHT];
    int head = 0, tail = 0;

    qx[tail] = (short)sx;
    qz[tail++] = (short)sz;
    seen[sz][sx] = true;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dz[4] = { 0, 0, 1, -1 };
    while (head < tail)
    {
        int x = qx[head], z = qz[head++];
        if (x == gx && z == gz) return true;

        for (int i = 0; i < 4; i++)
        {
            int nx = x + dx[i], nz = z + dz[i];
            if (!ArenaInBounds(arena, nx, nz) || seen[nz][nx]) continue;
            TileType type = arena->tiles[nz][nx].type;
            if (type == TILE_WALL || type == TILE_CRATE) continue;
            seen[nz][nx] = true;
            qx[tail] = (short)nx;
            qz[tail++] = (short)nz;
        }
    }
    return false;
}

static bool CheckRuntimeMap(const MapDefinition *definition, int expectedPlayers,
                            int expectedEnemies, bool expectCover)
{
    Arena arena;
    ArenaLoad(&arena, definition, 1000);

    CHECK(strcmp(arena.mapId, definition->id) == 0, "runtime map lost its stable id");
    CHECK(arena.width == definition->width && arena.height == definition->height,
          "runtime map dimensions differ from content");
    CHECK(arena.playerSpawnCount == expectedPlayers, "player spawn count changed");
    CHECK(arena.enemySpawnCount == expectedEnemies, "enemy spawn count changed");
    CHECK(ArenaTypeAt(&arena, arena.gemVent.x, arena.gemVent.z) == TILE_FLOOR,
          "gem vent is not on walkable floor");

    int crates = 0, bushes = 0;
    for (int z = 0; z < arena.height; z++)
    {
        for (int x = 0; x < arena.width; x++)
        {
            TileType type = arena.tiles[z][x].type;
            if (type == TILE_CRATE)
            {
                crates++;
                CHECK(arena.tiles[z][x].health == 1000,
                      "crate health did not come from project settings");
            }
            if (type == TILE_BUSH) bushes++;
            if (x == 0 || z == 0 || x == arena.width - 1 || z == arena.height - 1)
                CHECK(type == TILE_WALL, "arena border is not sealed");
        }
    }
    if (expectCover)
    {
        CHECK(crates > 0, "primary map has no destructible cover");
        CHECK(bushes > 0, "primary map has no concealment");
    }

    for (int i = 0; i < arena.playerSpawnCount; i++)
        CHECK(Reachable(&arena, arena.playerSpawns[i], arena.gemVent),
              "a player spawn cannot reach the objective");
    for (int i = 0; i < arena.enemySpawnCount; i++)
        CHECK(Reachable(&arena, arena.enemySpawns[i], arena.gemVent),
              "an enemy spawn cannot reach the objective");
    return true;
}

static Arena CollisionFixture(void)
{
    Arena arena = { 0 };
    arena.width = 7;
    arena.height = 7;
    arena.tileSize = 2.0f;
    for (int tz = 0; tz < arena.height; tz++)
    {
        for (int tx = 0; tx < arena.width; tx++)
        {
            bool border = tx == 0 || tz == 0 ||
                          tx == arena.width - 1 || tz == arena.height - 1;
            arena.tiles[tz][tx].type = border ? TILE_WALL : TILE_FLOOR;
        }
    }
    arena.tiles[3][3].type = TILE_WALL;
    arena.tiles[5][5] = (Tile){ .type = TILE_CRATE, .health = 1000 };
    return arena;
}

static bool CheckCircleMovement(void)
{
    Arena arena = CollisionFixture();

    Vector3 pointClearBodyBlocked = { 0.0f, 0.0f, 1.5f };
    CHECK(!ArenaSolidAt(&arena, pointClearBodyBlocked.x,
                       pointClearBodyBlocked.z),
          "fixture point should be outside the wall tile");
    CHECK(!ArenaCircleClear(&arena, pointClearBodyBlocked, BRAWLER_RADIUS),
          "body-aware clearance ignored a wall corner/edge");

    ArenaMoveResult stopped = ArenaMoveCircle(
        &arena, (Vector3){ 0.0f, 0.0f, -3.0f },
        (Vector3){ 0.0f, 0.0f, 6.0f }, BRAWLER_RADIUS);
    CHECK(stopped.collided, "fast movement did not report the wall");
    CHECK(stopped.position.z < -1.64f,
          "fast movement tunneled through a full wall tile");
    CHECK(stopped.normal.z < -0.9f,
          "wall collision did not return an outward normal");
    CHECK(ArenaCircleClear(&arena, stopped.position, BRAWLER_RADIUS),
          "wall stop left the body penetrating cover");

    ArenaMoveResult slide = ArenaMoveCircle(
        &arena, (Vector3){ -1.65f, 0.0f, -0.5f },
        (Vector3){ 2.0f, 0.0f, 1.0f }, BRAWLER_RADIUS);
    CHECK(slide.collided, "diagonal wall contact was not detected");
    CHECK(slide.position.x < -1.64f && slide.position.z > 0.35f,
          "wall contact discarded tangential movement instead of sliding");
    CHECK(ArenaCircleClear(&arena, slide.position, BRAWLER_RADIUS),
          "wall slide ended inside cover");

    CHECK(!ArenaSweepCircleClear(
              &arena, (Vector3){ -2.5f, 0.0f, 1.5f },
              (Vector3){ 2.5f, 0.0f, 1.5f }, BRAWLER_RADIUS),
          "sweep reduced a brawler to a point beside cover");
    CHECK(ArenaSweepCircleClear(
              &arena, (Vector3){ -2.5f, 0.0f, 2.0f },
              (Vector3){ 2.5f, 0.0f, 2.0f }, BRAWLER_RADIUS),
          "clear sweep above cover was rejected");

    ArenaMoveResult crateStop = ArenaMoveCircle(
        &arena, (Vector3){ 0.0f, 0.0f, 4.0f },
        (Vector3){ 8.0f, 0.0f, 0.0f }, BRAWLER_RADIUS);
    CHECK(crateStop.collided && crateStop.position.x < 2.36f,
          "swept movement tunneled through a crate");
    CHECK(arena.tiles[5][5].type == TILE_CRATE &&
          arena.tiles[5][5].health == 1000,
          "movement collision changed destructible-cover state");

    return true;
}

static bool CheckLineOfSight(void)
{
    Arena arena = CollisionFixture();

    CHECK(!ArenaLineOfSight(&arena, (Vector3){ 0.0f, 0.0f, -2.5f },
                            (Vector3){ 0.0f, 0.0f, 2.5f }),
          "straight sight line ignored the wall tile");
    // Shorter than half a tile: the old fixed-interval sampling produced zero
    // samples here and reported the corner as visible.
    CHECK(!ArenaLineOfSight(&arena, (Vector3){ 1.2f, 0.0f, 0.5f },
                            (Vector3){ 0.5f, 0.0f, 1.2f }),
          "short diagonal saw through the wall corner");
    CHECK(ArenaLineOfSight(&arena, (Vector3){ 1.4f, 0.0f, 0.8f },
                           (Vector3){ 0.8f, 0.0f, 1.4f }),
          "clear diagonal beside the corner was blocked");
    CHECK(ArenaLineOfSight(&arena, (Vector3){ -2.5f, 0.0f, -2.5f },
                           (Vector3){ 2.5f, 0.0f, -2.5f }),
          "clear lane was blocked");
    return true;
}

static bool RunTests(void)
{
    ContentCatalog catalog = { 0 };
    char message[256];
    CHECK(MapCatalogLoad(&catalog, "data/maps/manifest.cfg", message, sizeof(message)), message);
    CHECK(catalog.mapCount == 2, "map manifest should expose both fixtures");
    CHECK(strcmp(MapCatalogSelected(&catalog)->id, "helios_9") == 0,
          "manifest default map changed");

    const MapDefinition *helios = MapCatalogFind(&catalog, "helios_9");
    const MapDefinition *training = MapCatalogFind(&catalog, "training_court");
    CHECK(helios && training, "stable map lookup failed");
    CHECK(CheckRuntimeMap(helios, 3, 3, true), "Helios-9 runtime validation failed");
    CHECK(CheckRuntimeMap(training, 2, 2, false), "training fixture validation failed");
    CHECK(CheckCircleMovement(), "circle movement validation failed");
    CHECK(CheckLineOfSight(), "line-of-sight validation failed");

    return true;
}

int main(void)
{
    if (!RunTests()) return 1;
    puts("external maps, circle sweeps, sliding, and cover collision passed");
    return 0;
}
