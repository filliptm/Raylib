// Guard is deliberately not ARENA_H: types.h uses that name for the arena's tile height.
#ifndef BA_ARENA_MODULE_H
#define BA_ARENA_MODULE_H

#include "types.h"

void ArenaLoad(Arena *a, int crateHealth);

// Spawn point for a team slot, wrapping if the map has fewer points than brawlers.
Vector3 ArenaSpawnFor(const Arena *a, int team, int slot);

// Tile <-> world helpers. World space is centered on the arena origin.
int   ArenaTileX(float worldX);
int   ArenaTileZ(float worldZ);
Vector3 ArenaTileCenter(int tx, int tz);
bool  ArenaInBounds(int tx, int tz);

TileType ArenaTypeAt(const Arena *a, float x, float z);
bool ArenaSolidAt(const Arena *a, float x, float z);   // wall or intact crate
bool ArenaBushAt(const Arena *a, float x, float z);

// Pushes a circle out of any solid tile it overlaps. Returns the corrected position.
Vector3 ArenaResolveCircle(const Arena *a, Vector3 pos, float radius);

// True if nothing solid stands between the two points.
bool ArenaLineOfSight(const Arena *a, Vector3 from, Vector3 to);

// Damages the crate at a world position. Returns true if it was destroyed this call.
bool ArenaDamageAt(Arena *a, float x, float z, int damage);

// Damages every crate within `radius` of a point. Returns how many were destroyed.
int ArenaDamageRadius(Arena *a, Vector3 center, float radius, int damage);

void ArenaUpdate(Arena *a, float dt);

#endif // BA_ARENA_MODULE_H
