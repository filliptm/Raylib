#include "arena.h"

#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int ArenaTileX(const Arena *a, float worldX)
{
    return (int)floorf(worldX/a->tileSize + a->width*0.5f);
}

int ArenaTileZ(const Arena *a, float worldZ)
{
    return (int)floorf(worldZ/a->tileSize + a->height*0.5f);
}

Vector3 ArenaTileCenter(const Arena *a, int tx, int tz)
{
    return (Vector3){
        (tx + 0.5f - a->width*0.5f)*a->tileSize,
        0.0f,
        (tz + 0.5f - a->height*0.5f)*a->tileSize
    };
}

bool ArenaInBounds(const Arena *a, int tx, int tz)
{
    return a && tx >= 0 && tx < a->width && tz >= 0 && tz < a->height;
}

void ArenaLoad(Arena *a, const MapDefinition *map, int crateHealth)
{
    memset(a, 0, sizeof(*a));
    if (!map) return;

    snprintf(a->mapId, sizeof(a->mapId), "%s", map->id);
    snprintf(a->mapName, sizeof(a->mapName), "%s", map->name);
    a->width = map->width;
    a->height = map->height;
    a->tileSize = map->tileSize;
    a->propCount = map->propCount;
    memcpy(a->visual, map->visual, sizeof(a->visual));
    memcpy(a->props, map->props, sizeof(MapPropDefinition)*(size_t)map->propCount);

    for (int tz = 0; tz < a->height; tz++)
    {
        for (int tx = 0; tx < a->width; tx++)
        {
            Tile *tile = &a->tiles[tz][tx];
            switch (map->terrain[tz][tx])
            {
                case '#': tile->type = TILE_WALL; break;
                case 'c': tile->type = TILE_CRATE; tile->health = crateHealth; break;
                case 'b': tile->type = TILE_BUSH; break;
                default: tile->type = TILE_FLOOR; break;
            }

            Vector3 center = ArenaTileCenter(a, tx, tz);
            switch (map->gameplay[tz][tx])
            {
                case 'P':
                    if (a->playerSpawnCount < MAX_SPAWNS)
                        a->playerSpawns[a->playerSpawnCount++] = center;
                    break;
                case 'E':
                    if (a->enemySpawnCount < MAX_SPAWNS)
                        a->enemySpawns[a->enemySpawnCount++] = center;
                    break;
                case 'V': a->gemVent = center; break;
                default: break;
            }
        }
    }
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
    int tx = ArenaTileX(a, x), tz = ArenaTileZ(a, z);
    if (!ArenaInBounds(a, tx, tz)) return TILE_WALL;
    return a->tiles[tz][tx].type;
}

bool ArenaSolidAt(const Arena *a, float x, float z)
{
    TileType type = ArenaTypeAt(a, x, z);
    return type == TILE_WALL || type == TILE_CRATE;
}

bool ArenaBushAt(const Arena *a, float x, float z)
{
    return ArenaTypeAt(a, x, z) == TILE_BUSH;
}

Vector3 ArenaResolveCircle(const Arena *a, Vector3 pos, float radius)
{
    int cx = ArenaTileX(a, pos.x), cz = ArenaTileZ(a, pos.z);

    for (int pass = 0; pass < 2; pass++)
    {
        for (int tz = cz - 1; tz <= cz + 1; tz++)
        {
            for (int tx = cx - 1; tx <= cx + 1; tx++)
            {
                if (!ArenaInBounds(a, tx, tz)) continue;
                TileType type = a->tiles[tz][tx].type;
                if (type != TILE_WALL && type != TILE_CRATE) continue;

                Vector3 center = ArenaTileCenter(a, tx, tz);
                float half = a->tileSize*0.5f;
                float nearX = Clamp(pos.x, center.x - half, center.x + half);
                float nearZ = Clamp(pos.z, center.z - half, center.z + half);
                float dx = pos.x - nearX;
                float dz = pos.z - nearZ;
                float distanceSquared = dx*dx + dz*dz;

                if (distanceSquared < radius*radius)
                {
                    if (distanceSquared > 0.00001f)
                    {
                        float distance = sqrtf(distanceSquared);
                        float push = radius - distance;
                        pos.x += (dx/distance)*push;
                        pos.z += (dz/distance)*push;
                    }
                    else
                    {
                        float penetrationX = half + radius - fabsf(pos.x - center.x);
                        float penetrationZ = half + radius - fabsf(pos.z - center.z);
                        if (penetrationX < penetrationZ)
                            pos.x += pos.x < center.x ? -penetrationX : penetrationX;
                        else
                            pos.z += pos.z < center.z ? -penetrationZ : penetrationZ;
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
    float distance = sqrtf(dx*dx + dz*dz);
    if (distance < 0.001f) return true;

    float step = a->tileSize*0.25f;
    int steps = (int)(distance/step);
    for (int i = 1; i < steps; i++)
    {
        float amount = (float)i/(float)steps;
        if (ArenaSolidAt(a, from.x + dx*amount, from.z + dz*amount)) return false;
    }
    return true;
}

bool ArenaDamageAt(Arena *a, float x, float z, int damage)
{
    int tx = ArenaTileX(a, x), tz = ArenaTileZ(a, z);
    if (!ArenaInBounds(a, tx, tz)) return false;

    Tile *tile = &a->tiles[tz][tx];
    if (tile->type != TILE_CRATE) return false;
    tile->health -= damage;
    tile->hitFlash = 1.0f;
    if (tile->health > 0) return false;

    tile->type = TILE_FLOOR;
    tile->health = 0;
    return true;
}

int ArenaDamageRadius(Arena *a, Vector3 center, float radius, int damage)
{
    int destroyed = 0;
    int span = (int)(radius/a->tileSize) + 1;
    int cx = ArenaTileX(a, center.x), cz = ArenaTileZ(a, center.z);

    for (int tz = cz - span; tz <= cz + span; tz++)
    {
        for (int tx = cx - span; tx <= cx + span; tx++)
        {
            if (!ArenaInBounds(a, tx, tz) || a->tiles[tz][tx].type != TILE_CRATE) continue;
            Vector3 tileCenter = ArenaTileCenter(a, tx, tz);
            float dx = tileCenter.x - center.x, dz = tileCenter.z - center.z;
            if (dx*dx + dz*dz <= radius*radius &&
                ArenaDamageAt(a, tileCenter.x, tileCenter.z, damage)) destroyed++;
        }
    }
    return destroyed;
}

void ArenaUpdate(Arena *a, float dt)
{
    for (int tz = 0; tz < a->height; tz++)
    {
        for (int tx = 0; tx < a->width; tx++)
        {
            Tile *tile = &a->tiles[tz][tx];
            if (tile->hitFlash <= 0.0f) continue;
            tile->hitFlash -= dt*4.0f;
            if (tile->hitFlash < 0.0f) tile->hitFlash = 0.0f;
        }
    }
}
