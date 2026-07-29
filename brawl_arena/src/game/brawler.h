#ifndef BRAWLER_H
#define BRAWLER_H

#include "game_types.h"

void BrawlerSpawn(GameContext game, int idx, Team team, BrawlerClass cls,
                  Vector3 position, bool isPlayer);
void BrawlerRespawn(GameContext game, int idx);

typedef struct BrawlerDamageResult {
    int healthRemoved;
    int shieldAbsorbed;
} BrawlerDamageResult;

// Applies hostile combat damage through an active shield before health. The detailed
// result keeps shield damage useful for hit-confirm rules without allowing it to feed
// health-damage-only mechanics such as Tank's Reclaim.
BrawlerDamageResult BrawlerApplyDamageDetailed(
    GameContext game, int idx, int damage, int attacker, Vector3 hitPosition);
// Compatibility helper that returns only health actually removed.
int BrawlerApplyDamage(GameContext game, int idx, int damage, int attacker,
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
// Starts the character's optional Shift/LB ability along `direction`.
bool BrawlerTrySecondary(GameContext game, int idx, Vector3 direction);
// Compatibility helper for callers that specifically require a dash secondary.
bool BrawlerTryMobility(GameContext game, int idx, Vector3 direction);
bool BrawlerIsGrappling(const Brawler *brawler);
// Resolves the exact body-safe grapple endpoint used by both activation and the
// player's targeting preview. Cover shortens the result without sliding it.
Vector3 BrawlerGrappleEndpoint(const Arena *arena, Vector3 start,
                               Vector3 direction, float range);
// Applies an external pull/knockback through terrain and cancels an active grapple.
void BrawlerDisplace(GameContext game, int idx, Vector3 displacement);
// Lowers a held shield and clears its release-to-rearm latch.
void BrawlerReleaseShield(GameContext game, int idx);
bool BrawlerProjectileThreat(GameContext game, int idx, float horizon);

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
// Runs after projectile/ability resolution so damage on this frame always interrupts
// regeneration before a scheduled health pulse can be applied.
void BrawlersUpdateRegeneration(GameContext game);

#endif // BRAWLER_H
