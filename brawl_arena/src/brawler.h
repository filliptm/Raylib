#ifndef BRAWLER_H
#define BRAWLER_H

#include "types.h"

#define DASH_SPEED 26.0f

void BrawlerSpawn(World *w, int idx, Team team, BrawlerClass cls, Vector3 pos, bool isPlayer);
void BrawlerRespawn(World *w, int idx);

void BrawlerApplyDamage(World *w, int idx, int damage, int attacker, Vector3 hitPos);
void BrawlerAwardSuper(World *w, int idx, float amount);

// True when the attack actually went off (ammo and cooldown allowing).
bool BrawlerTryAttack(World *w, int idx, float aimDist);
bool BrawlerTrySuper(World *w, int idx, float aimDist);

// Can `viewer` see `target`? Accounts for line of sight, bushes and recent firing.
bool BrawlerCanSee(World *w, int viewer, int target);

// Snap the body toward a shot direction and hold it there for `holdTime` seconds,
// after which it eases back to whichever way the brawler is moving.
void BrawlerFaceShot(World *w, int idx, float yaw, float holdTime);

// Nearest living enemy that `idx` can currently see, or -1.
int BrawlerNearestVisibleEnemy(World *w, int idx);

void BrawlersUpdate(World *w, float dt);

#endif // BRAWLER_H
