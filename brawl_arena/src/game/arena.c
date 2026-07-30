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

    ArenaRefreshNavigable(a, 0, 0, a->width - 1, a->height - 1);
}

static bool ComputeNavigable(const Arena *a, int tx, int tz)
{
    if (!ArenaInBounds(a, tx, tz)) return false;
    TileType type = a->tiles[tz][tx].type;
    if (type == TILE_WALL || type == TILE_CRATE) return false;
    return ArenaCircleClear(a, ArenaTileCenter(a, tx, tz), ARENA_ROUTE_CLEARANCE);
}

void ArenaRefreshNavigable(Arena *a, int x0, int z0, int x1, int z1)
{
    if (x0 < 0) x0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 >= a->width) x1 = a->width - 1;
    if (z1 >= a->height) z1 = a->height - 1;
    for (int tz = z0; tz <= z1; tz++)
        for (int tx = x0; tx <= x1; tx++)
            a->navigable[tz][tx] = ComputeNavigable(a, tx, tz);
    a->navVersion++;
}

bool ArenaNavigableAt(const Arena *a, int tx, int tz)
{
    if (!ArenaInBounds(a, tx, tz)) return false;
    return a->navigable[tz][tx];
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
    int span = (int)ceilf(radius/a->tileSize) + 1;

    for (int pass = 0; pass < 4; pass++)
    {
        for (int tz = cz - span; tz <= cz + span; tz++)
        {
            for (int tx = cx - span; tx <= cx + span; tx++)
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

bool ArenaCircleClear(const Arena *a, Vector3 pos, float radius)
{
    if (!a || a->tileSize <= 0.0f || radius < 0.0f) return false;
    float halfWidth = a->width*a->tileSize*0.5f;
    float halfHeight = a->height*a->tileSize*0.5f;
    if (pos.x - radius < -halfWidth || pos.x + radius > halfWidth ||
        pos.z - radius < -halfHeight || pos.z + radius > halfHeight)
        return false;

    int cx = ArenaTileX(a, pos.x), cz = ArenaTileZ(a, pos.z);
    int span = (int)ceilf(radius/a->tileSize) + 1;
    float contactRadius = fmaxf(0.0f, radius - 0.0001f);
    float radiusSquared = contactRadius*contactRadius;

    for (int tz = cz - span; tz <= cz + span; tz++)
    {
        for (int tx = cx - span; tx <= cx + span; tx++)
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
            if (dx*dx + dz*dz < radiusSquared) return false;
        }
    }
    return true;
}

static int MovementSteps(const Arena *a, float distance, float radius)
{
    if (distance <= 0.00001f) return 1;
    float maxStep = fminf(a->tileSize*0.25f, fmaxf(radius*0.5f, 0.05f));
    int steps = (int)ceilf(distance/maxStep);
    return steps > 0 ? steps : 1;
}

bool ArenaSweepCircleClear(const Arena *a, Vector3 from, Vector3 to, float radius)
{
    if (!a || a->tileSize <= 0.0f) return false;
    Vector3 delta = Vector3Subtract(to, from);
    delta.y = 0.0f;
    int steps = MovementSteps(a, Vector3Length(delta), radius);

    for (int i = 0; i <= steps; i++)
    {
        float amount = (float)i/(float)steps;
        Vector3 sample = Vector3Add(from, Vector3Scale(delta, amount));
        if (!ArenaCircleClear(a, sample, radius)) return false;
    }
    return true;
}

ArenaMoveResult ArenaMoveCircle(const Arena *a, Vector3 start, Vector3 displacement,
                                float radius)
{
    ArenaMoveResult result = { 0 };
    if (!a || a->tileSize <= 0.0f)
    {
        result.position = start;
        return result;
    }

    start.y = 0.0f;
    displacement.y = 0.0f;
    result.position = ArenaResolveCircle(a, start, radius);

    int steps = MovementSteps(a, Vector3Length(displacement), radius);
    Vector3 step = Vector3Scale(displacement, 1.0f/(float)steps);
    Vector3 normalSum = { 0 };

    for (int i = 0; i < steps; i++)
    {
        Vector3 proposed = Vector3Add(result.position, step);
        Vector3 resolved = ArenaResolveCircle(a, proposed, radius);
        Vector3 correction = Vector3Subtract(resolved, proposed);
        correction.y = 0.0f;

        if (Vector3LengthSqr(correction) <= 0.0000001f)
        {
            result.position = proposed;
            continue;
        }

        Vector3 normal = Vector3Normalize(correction);
        result.collided = true;
        normalSum = Vector3Add(normalSum, normal);

        // Retry this substep with only the tangential component. This keeps contact
        // responsive instead of repeatedly pushing the body into the same wall.
        Vector3 slide = step;
        float into = Vector3DotProduct(slide, normal);
        if (into < 0.0f)
            slide = Vector3Subtract(slide, Vector3Scale(normal, into));

        Vector3 slideProposed = Vector3Add(result.position, slide);
        Vector3 slideResolved = ArenaResolveCircle(a, slideProposed, radius);
        Vector3 slideCorrection = Vector3Subtract(slideResolved, slideProposed);
        slideCorrection.y = 0.0f;
        if (Vector3LengthSqr(slideCorrection) > 0.0000001f)
            normalSum = Vector3Add(normalSum, Vector3Normalize(slideCorrection));
        result.position = slideResolved;
    }

    if (Vector3LengthSqr(normalSum) > 0.0000001f)
        result.normal = Vector3Normalize(normalSum);
    result.position.y = 0.0f;
    return result;
}

// Exact DDA walk over every tile the sight line crosses. The previous fixed-interval
// sampling produced zero samples at short range and could step across thin corner
// clips, letting close-quarters shots see through wall corners.
bool ArenaLineOfSight(const Arena *a, Vector3 from, Vector3 to)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float distance = sqrtf(dx*dx + dz*dz);
    if (distance < 0.001f) return true;

    int tx = ArenaTileX(a, from.x), tz = ArenaTileZ(a, from.z);
    int endX = ArenaTileX(a, to.x), endZ = ArenaTileZ(a, to.z);
    if (tx == endX && tz == endZ) return true;

    float dirX = dx/distance, dirZ = dz/distance;
    int stepX = (dirX > 0.0f) ? 1 : -1;
    int stepZ = (dirZ > 0.0f) ? 1 : -1;

    // Segment distance between successive x/z tile-boundary crossings, and to the
    // first crossing from the start point.
    float tDeltaX = (dirX != 0.0f) ? a->tileSize/fabsf(dirX) : INFINITY;
    float tDeltaZ = (dirZ != 0.0f) ? a->tileSize/fabsf(dirZ) : INFINITY;
    float boundaryX = (tx + (stepX > 0 ? 1 : 0) - a->width*0.5f)*a->tileSize;
    float boundaryZ = (tz + (stepZ > 0 ? 1 : 0) - a->height*0.5f)*a->tileSize;
    float tMaxX = (dirX != 0.0f) ? (boundaryX - from.x)/dirX : INFINITY;
    float tMaxZ = (dirZ != 0.0f) ? (boundaryZ - from.z)/dirZ : INFINITY;

    // Neither endpoint tile blocks its own sight line; only tiles strictly between
    // them do. The distance guard ends the walk if float error keeps the indices
    // from landing exactly on the end tile.
    while ((tx != endX || tz != endZ) && fminf(tMaxX, tMaxZ) <= distance)
    {
        if (tMaxX < tMaxZ) { tx += stepX; tMaxX += tDeltaX; }
        else               { tz += stepZ; tMaxZ += tDeltaZ; }
        if (tx == endX && tz == endZ) break;
        if (!ArenaInBounds(a, tx, tz)) return false;
        TileType type = a->tiles[tz][tx].type;
        if (type == TILE_WALL || type == TILE_CRATE) return false;
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
    // The broken crate can only affect clearance in its immediate neighbourhood.
    ArenaRefreshNavigable(a, tx - 2, tz - 2, tx + 2, tz + 2);
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
