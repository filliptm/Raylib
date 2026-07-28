#ifndef BA_ARENA_MODULE_H
#define BA_ARENA_MODULE_H

#include "game_types.h"

typedef struct ArenaMoveResult {
    Vector3 position;
    Vector3 normal;
    bool collided;
} ArenaMoveResult;

void ArenaLoad(Arena *a, const MapDefinition *map, int crateHealth);

// Spawn point for a team slot, wrapping if the map has fewer points than brawlers.
Vector3 ArenaSpawnFor(const Arena *a, int team, int slot);

// Tile <-> world helpers. App space is centered on the arena origin.
int   ArenaTileX(const Arena *a, float worldX);
int   ArenaTileZ(const Arena *a, float worldZ);
Vector3 ArenaTileCenter(const Arena *a, int tx, int tz);
bool  ArenaInBounds(const Arena *a, int tx, int tz);

TileType ArenaTypeAt(const Arena *a, float x, float z);
bool ArenaSolidAt(const Arena *a, float x, float z);   // wall or intact crate
bool ArenaBushAt(const Arena *a, float x, float z);

// Pushes a circle out of any solid tile it overlaps. Returns the corrected position.
Vector3 ArenaResolveCircle(const Arena *a, Vector3 pos, float radius);

// Body-aware collision queries used by movement and navigation.
bool ArenaCircleClear(const Arena *a, Vector3 pos, float radius);
bool ArenaSweepCircleClear(const Arena *a, Vector3 from, Vector3 to, float radius);

// Moves a circle through the arena in bounded steps and slides along contact surfaces.
ArenaMoveResult ArenaMoveCircle(const Arena *a, Vector3 start, Vector3 displacement,
                                float radius);

// True if nothing solid stands between the two points.
bool ArenaLineOfSight(const Arena *a, Vector3 from, Vector3 to);

// Routing clearance used by the precomputed navigable bitmap.
#define ARENA_ROUTE_CLEARANCE (BRAWLER_RADIUS + 0.05f)

// Recomputes the navigable bitmap over the inclusive tile window and bumps the
// arena's nav version. ArenaLoad refreshes the whole map; a broken crate refreshes
// only its neighbourhood.
void ArenaRefreshNavigable(Arena *a, int x0, int z0, int x1, int z1);

// Reads the precomputed body-clearance routing bitmap.
bool ArenaNavigableAt(const Arena *a, int tx, int tz);

// Damages the crate at a world position. Returns true if it was destroyed this call.
bool ArenaDamageAt(Arena *a, float x, float z, int damage);

// Damages every crate within `radius` of a point. Returns how many were destroyed.
int ArenaDamageRadius(Arena *a, Vector3 center, float radius, int damage);

void ArenaUpdate(Arena *a, float dt);

#endif // BA_ARENA_MODULE_H
