#ifndef BRAWL_GAME_EVENTS_H
#define BRAWL_GAME_EVENTS_H

#include "game_types.h"

void GameEventsClear(GameSession *session);
void GameEmitMuzzle(GameSession *session, Vector3 position, float angle, Color color);
void GameEmitImpact(GameSession *session, Vector3 position, Color color, int count);
void GameEmitExplosion(GameSession *session, Vector3 position, float radius, Color color);
void GameEmitDeath(GameSession *session, Vector3 position, Color color);
void GameEmitCrateBreak(GameSession *session, Vector3 position);
void GameEmitFloatText(GameSession *session, Vector3 position, const char *text, Color color);
void GameEmitLight(GameSession *session, Vector3 position, Color color, float radius, float life);
void GameEmitShockwave(GameSession *session, Vector3 position, float radius, float life, Color color);
void GameEmitParticle(GameSession *session, Vector3 position, Vector3 velocity, Color color,
                      float life, float size, ParticleType type);
void GameEmitVfx(GameSession *session, VfxEffectId id, Vector3 position,
                 Vector3 endPosition, float angle, float size, Color color);
void GameEmitVfxAttached(GameSession *session, VfxEffectId id, Vector3 position,
                         Vector3 endPosition, float angle, float size, Color color,
                         int sourceBrawler, VfxSocket startSocket,
                         int targetBrawler, VfxSocket endSocket);
void GameEmitCharacterAction(GameSession *session, int brawlerIndex,
                             CharacterActionId action);
void GameEmitCharacterActionTimed(GameSession *session, int brawlerIndex,
                                  CharacterActionId action, float duration);
void GameEmitMatchShake(GameSession *session, float amount);

#endif
