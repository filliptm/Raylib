#ifndef BRAWLER_H
#define BRAWLER_H

#include "types.h"

void BrawlerSpawn(World *w, int idx, Team team, BrawlerClass cls, Vector3 pos, bool isPlayer);
void BrawlerRespawn(World *w, int idx);

void BrawlerApplyDamage(World *w, int idx, int damage, int attacker, Vector3 hitPos);
// Restores up to `amount` health without reviving. Returns the health actually restored.
int BrawlerApplyHealing(World *w, int idx, int amount, int healer, Vector3 hitPos);
void BrawlerAwardSuper(World *w, int idx, float amount);
// Applies the Guardian sound-wave mark. Allies receive healing pulses and enemies
// receive damage pulses for `duration`; a new mark refreshes the existing one.
void BrawlerApplyResonance(World *w, int idx, Team sourceTeam, int source,
                           int damage, int healing, float duration, float tickRate);

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

// Most wounded living teammate in range, excluding `idx`; ties prefer the nearer ally.
int BrawlerMostWoundedAlly(World *w, int idx, float maxDistance);

void BrawlersUpdate(World *w, float dt);

#endif // BRAWLER_H
