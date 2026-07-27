#include "arena.h"
#include "raymath.h"
#include <string.h>
#include <math.h>

//------------------------------------------------------------------------------------
// Hand-authored map.
//   '#' wall   'c' crate   'b' bush   '.' floor   'P' player spawn   'E' enemy spawn
//
// Helios-9 is a vertically mirrored open station atrium: short docking-bay shields,
// destructible flank stations, hydroponic concealment pockets, and four strategic
// reactor pylons leave broad movement bands around the gem vent. The first 'P' found
// is the human's, so the centre-back player spawn intentionally appears one row before
// the two side spawns.
//
// Rows shorter than ARENA_W are padded with floor, and the border is forced to wall,
// so the layout stays easy to edit without counting characters.
//------------------------------------------------------------------------------------
// Cover is clustered into landmarks rather than repeated across every row. This keeps
// the arena readable, gives long-range kits room to work, and still offers short walls,
// bushes, and breakable objects for rotations or retreats.
static const char *MAP[] = {
    "#################################",
    "#...............................#",
    "#......E.................E......#",
    "#...............E...............#",
    "#.....##.......###.......##.....#",
    "#.........cc.........cc.........#",
    "#...............................#",
    "#...bbb...##.........##...bbb...#",
    "#...bbb...#c.........c#...bbb...#",
    "#.......c...##.....##...c.......#",
    "#............#.....#............#",
    "#...bbb...c...........c...bbb...#",
    "#............#.....#............#",
    "#.......c...##.....##...c.......#",
    "#...bbb...#c.........c#...bbb...#",
    "#...bbb...##.........##...bbb...#",
    "#...............................#",
    "#.........cc.........cc.........#",
    "#.....##.......###.......##.....#",
    "#...............P...............#",
    "#......P.................P......#",
    "#...............................#",
    "#################################",
};

int ArenaTileX(float worldX) { return (int)floorf(worldX / TILE_SIZE + ARENA_W * 0.5f); }
int ArenaTileZ(float worldZ) { return (int)floorf(worldZ / TILE_SIZE + ARENA_H * 0.5f); }

Vector3 ArenaTileCenter(int tx, int tz)
{
    return (Vector3){
        (tx + 0.5f - ARENA_W * 0.5f) * TILE_SIZE,
        0.0f,
        (tz + 0.5f - ARENA_H * 0.5f) * TILE_SIZE
    };
}

bool ArenaInBounds(int tx, int tz)
{
    return tx >= 0 && tx < ARENA_W && tz >= 0 && tz < ARENA_H;
}

void ArenaLoad(Arena *a, int crateHealth)
{
    memset(a, 0, sizeof(Arena));

    int rows = (int)(sizeof(MAP) / sizeof(MAP[0]));
    if (rows > ARENA_H) rows = ARENA_H;

    for (int tz = 0; tz < ARENA_H; tz++)
    {
        const char *row = (tz < rows) ? MAP[tz] : "";
        int len = (int)strlen(row);

        for (int tx = 0; tx < ARENA_W; tx++)
        {
            char c = (tx < len) ? row[tx] : '.';
            Tile *t = &a->tiles[tz][tx];
            t->hitFlash = 0.0f;

            switch (c)
            {
                case '#': t->type = TILE_WALL; break;
                case 'c': t->type = TILE_CRATE; t->health = crateHealth; break;
                case 'b': t->type = TILE_BUSH; break;
                case 'P':
                    t->type = TILE_FLOOR;
                    if (a->playerSpawnCount < MAX_SPAWNS)
                        a->playerSpawns[a->playerSpawnCount++] = ArenaTileCenter(tx, tz);
                    break;
                case 'E':
                    t->type = TILE_FLOOR;
                    if (a->enemySpawnCount < MAX_SPAWNS)
                        a->enemySpawns[a->enemySpawnCount++] = ArenaTileCenter(tx, tz);
                    break;
                default: t->type = TILE_FLOOR; break;
            }

            // Force a sealed border regardless of what the map string says.
            if (tx == 0 || tz == 0 || tx == ARENA_W - 1 || tz == ARENA_H - 1)
            {
                t->type = TILE_WALL;
                t->health = 0;
            }
        }
    }

    // Never leave a side without somewhere to stand.
    if (a->playerSpawnCount == 0)
        a->playerSpawns[a->playerSpawnCount++] = (Vector3){ 0, 0, 16.0f };
    if (a->enemySpawnCount == 0)
        a->enemySpawns[a->enemySpawnCount++] = (Vector3){ 0, 0, -16.0f };

    // The vent sits dead centre, in the corridor the map opens up for it.
    a->gemVent = ArenaTileCenter(ARENA_W/2, ARENA_H/2);
}

Vector3 ArenaSpawnFor(const Arena *a, int team, int slot)
{
    const Vector3 *list = (team == TEAM_PLAYER) ? a->playerSpawns : a->enemySpawns;
    int count = (team == TEAM_PLAYER) ? a->playerSpawnCount : a->enemySpawnCount;
    if (count <= 0) return (Vector3){ 0, 0, 0 };
    if (slot < 0) slot = 0;
    return list[slot % count];
}

TileType ArenaTypeAt(const Arena *a, float x, float z)
{
    int tx = ArenaTileX(x), tz = ArenaTileZ(z);
    if (!ArenaInBounds(tx, tz)) return TILE_WALL;
    return a->tiles[tz][tx].type;
}

bool ArenaSolidAt(const Arena *a, float x, float z)
{
    TileType t = ArenaTypeAt(a, x, z);
    return (t == TILE_WALL || t == TILE_CRATE);
}

bool ArenaBushAt(const Arena *a, float x, float z)
{
    return ArenaTypeAt(a, x, z) == TILE_BUSH;
}

Vector3 ArenaResolveCircle(const Arena *a, Vector3 pos, float radius)
{
    int cx = ArenaTileX(pos.x), cz = ArenaTileZ(pos.z);

    // Two passes settle corner cases where pushing out of one tile pushes into another.
    for (int pass = 0; pass < 2; pass++)
    {
        for (int tz = cz - 1; tz <= cz + 1; tz++)
        {
            for (int tx = cx - 1; tx <= cx + 1; tx++)
            {
                if (!ArenaInBounds(tx, tz)) continue;
                TileType type = a->tiles[tz][tx].type;
                if (type != TILE_WALL && type != TILE_CRATE) continue;

                Vector3 c = ArenaTileCenter(tx, tz);
                float half = TILE_SIZE * 0.5f;
                float nearX = Clamp(pos.x, c.x - half, c.x + half);
                float nearZ = Clamp(pos.z, c.z - half, c.z + half);

                float dx = pos.x - nearX;
                float dz = pos.z - nearZ;
                float d2 = dx * dx + dz * dz;

                if (d2 < radius * radius)
                {
                    if (d2 > 0.00001f)
                    {
                        float d = sqrtf(d2);
                        float push = radius - d;
                        pos.x += (dx / d) * push;
                        pos.z += (dz / d) * push;
                    }
                    else
                    {
                        // Dead center inside the tile: eject along the shallowest axis.
                        float penX = half + radius - fabsf(pos.x - c.x);
                        float penZ = half + radius - fabsf(pos.z - c.z);
                        if (penX < penZ) pos.x += (pos.x < c.x ? -penX : penX);
                        else             pos.z += (pos.z < c.z ? -penZ : penZ);
                    }
                }
            }
        }
    }
    return pos;
}

bool ArenaLineOfSight(const Arena *a, Vector3 from, Vector3 to)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist < 0.001f) return true;

    float step = TILE_SIZE * 0.25f;
    int steps = (int)(dist / step);

    for (int i = 1; i < steps; i++)
    {
        float t = (float)i / (float)steps;
        float x = from.x + dx * t;
        float z = from.z + dz * t;
        if (ArenaSolidAt(a, x, z)) return false;
    }
    return true;
}

bool ArenaDamageAt(Arena *a, float x, float z, int damage)
{
    int tx = ArenaTileX(x), tz = ArenaTileZ(z);
    if (!ArenaInBounds(tx, tz)) return false;

    Tile *t = &a->tiles[tz][tx];
    if (t->type != TILE_CRATE) return false;

    t->health -= damage;
    t->hitFlash = 1.0f;

    if (t->health <= 0)
    {
        t->type = TILE_FLOOR;
        t->health = 0;
        return true;
    }
    return false;
}

int ArenaDamageRadius(Arena *a, Vector3 center, float radius, int damage)
{
    int destroyed = 0;
    int span = (int)(radius / TILE_SIZE) + 1;
    int cx = ArenaTileX(center.x), cz = ArenaTileZ(center.z);

    for (int tz = cz - span; tz <= cz + span; tz++)
    {
        for (int tx = cx - span; tx <= cx + span; tx++)
        {
            if (!ArenaInBounds(tx, tz)) continue;
            if (a->tiles[tz][tx].type != TILE_CRATE) continue;

            Vector3 c = ArenaTileCenter(tx, tz);
            float dx = c.x - center.x, dz = c.z - center.z;
            if (dx * dx + dz * dz <= radius * radius)
            {
                if (ArenaDamageAt(a, c.x, c.z, damage)) destroyed++;
            }
        }
    }
    return destroyed;
}

void ArenaUpdate(Arena *a, float dt)
{
    for (int tz = 0; tz < ARENA_H; tz++)
        for (int tx = 0; tx < ARENA_W; tx++)
        {
            Tile *t = &a->tiles[tz][tx];
            if (t->hitFlash > 0.0f)
            {
                t->hitFlash -= dt * 4.0f;
                if (t->hitFlash < 0.0f) t->hitFlash = 0.0f;
            }
        }
}
