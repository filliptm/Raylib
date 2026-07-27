#ifndef BRAWLER_H
#define BRAWLER_H

#include "game_types.h"

void BrawlerSpawn(GameContext game, int idx, Team team, BrawlerClass cls,
                  Vector3 position, bool isPlayer);
void BrawlerRespawn(GameContext game, int idx);

void BrawlerApplyDamage(GameContext game, int idx, int damage, int attacker,
                        Vector3 hitPosition);
// Restores up to `amount` health without reviving. Returns the health actually restored.
int BrawlerApplyHealing(GameContext game, int idx, int amount, int healer,
                        Vector3 hitPosition);
void BrawlerAwardSuper(GameContext game, int idx, float amount);
// Generic team-aware periodic status. The same behavior supports healing-over-time on
// allies and damage-over-time on enemies; future abilities reuse it without new actor
// fields.
void BrawlerApplyPulseStatus(GameContext game, int idx, Team sourceTeam, int source,
                             int damage, int healing, float duration, float tickRate);

// True when the attack actually went off (ammo and cooldown allowing).
bool BrawlerTryAttack(GameContext game, int idx, float aimDist);
bool BrawlerTrySuper(GameContext game, int idx, float aimDist);

// Can `viewer` see `target`? Accounts for line of sight, bushes and recent firing.
bool BrawlerCanSee(GameContext game, int viewer, int target);

// Snap the body toward a shot direction and hold it there for `holdTime` seconds,
// after which it eases back to whichever way the brawler is moving.
void BrawlerFaceShot(GameContext game, int idx, float yaw, float holdTime);

// Nearest living enemy that `idx` can currently see, or -1.
int BrawlerNearestVisibleEnemy(GameContext game, int idx);

// Most wounded living teammate in range, excluding `idx`; ties prefer the nearer ally.
int BrawlerMostWoundedAlly(GameContext game, int idx, float maxDistance);

void BrawlersUpdate(GameContext game, float dt);

#endif // BRAWLER_H
