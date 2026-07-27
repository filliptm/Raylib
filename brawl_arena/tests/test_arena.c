#include "arena.h"
#include "map_content.h"
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

    return true;
}

int main(void)
{
    if (!RunTests()) return 1;
    puts("external map catalog, Helios-9, and secondary fixture passed");
    return 0;
}
