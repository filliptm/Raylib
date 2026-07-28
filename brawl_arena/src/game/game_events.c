#include "game_events.h"

#include <stdio.h>

static GameEvent *Push(GameSession *session, GameEventType type)
{
    GameEventQueue *queue = &session->events;
    if (queue->count >= MAX_GAME_EVENTS)
    {
        queue->dropped++;
        return NULL;
    }
    GameEvent *event = &queue->items[queue->count++];
    *event = (GameEvent){ 0 };
    event->type = type;
    event->sourceBrawler = -1;
    event->targetBrawler = -1;
    return event;
}

void GameEventsClear(GameSession *session)
{
    session->events.count = 0;
    session->events.dropped = 0;
}

void GameEmitMuzzle(GameSession *session, Vector3 position, float angle, Color color)
{
    GameEvent *event = Push(session, GAME_EVENT_MUZZLE);
    if (!event) return;
    event->position = position;
    event->angle = angle;
    event->color = color;
}

void GameEmitImpact(GameSession *session, Vector3 position, Color color, int count)
{
    GameEvent *event = Push(session, GAME_EVENT_IMPACT);
    if (!event) return;
    event->position = position;
    event->color = color;
    event->count = count;
}

void GameEmitExplosion(GameSession *session, Vector3 position, float radius, Color color)
{
    GameEvent *event = Push(session, GAME_EVENT_EXPLOSION);
    if (!event) return;
    event->position = position;
    event->radius = radius;
    event->color = color;
}

void GameEmitDeath(GameSession *session, Vector3 position, Color color)
{
    GameEvent *event = Push(session, GAME_EVENT_DEATH);
    if (!event) return;
    event->position = position;
    event->color = color;
}

void GameEmitCrateBreak(GameSession *session, Vector3 position)
{
    GameEvent *event = Push(session, GAME_EVENT_CRATE_BREAK);
    if (event) event->position = position;
}

void GameEmitFloatText(GameSession *session, Vector3 position, const char *text, Color color)
{
    GameEvent *event = Push(session, GAME_EVENT_FLOAT_TEXT);
    if (!event) return;
    event->position = position;
    event->color = color;
    snprintf(event->text, sizeof(event->text), "%s", text ? text : "");
}

void GameEmitLight(GameSession *session, Vector3 position, Color color, float radius, float life)
{
    GameEvent *event = Push(session, GAME_EVENT_LIGHT);
    if (!event) return;
    event->position = position;
    event->color = color;
    event->radius = radius;
    event->life = life;
}

void GameEmitShockwave(GameSession *session, Vector3 position, float radius, float life, Color color)
{
    GameEvent *event = Push(session, GAME_EVENT_SHOCKWAVE);
    if (!event) return;
    event->position = position;
    event->radius = radius;
    event->life = life;
    event->color = color;
}

void GameEmitParticle(GameSession *session, Vector3 position, Vector3 velocity, Color color,
                      float life, float size, ParticleType type)
{
    GameEvent *event = Push(session, GAME_EVENT_PARTICLE);
    if (!event) return;
    event->position = position;
    event->velocity = velocity;
    event->color = color;
    event->life = life;
    event->size = size;
    event->particleType = type;
}

void GameEmitVfx(GameSession *session, VfxEffectId id, Vector3 position,
                 Vector3 endPosition, float angle, float size, Color color)
{
    GameEmitVfxAttached(session, id, position, endPosition, angle, size, color,
                        -1, VFX_SOCKET_NONE, -1, VFX_SOCKET_NONE);
}

void GameEmitVfxAttached(GameSession *session, VfxEffectId id, Vector3 position,
                         Vector3 endPosition, float angle, float size, Color color,
                         int sourceBrawler, VfxSocket startSocket,
                         int targetBrawler, VfxSocket endSocket)
{
    if (id <= VFX_NONE || id >= VFX_EFFECT_COUNT) return;
    GameEvent *event = Push(session, GAME_EVENT_VFX);
    if (!event) return;
    event->vfxId = id;
    event->position = position;
    event->endPosition = endPosition;
    event->angle = angle;
    event->size = size;
    event->color = color;
    event->sourceBrawler = sourceBrawler;
    event->targetBrawler = targetBrawler;
    event->startSocket = startSocket;
    event->endSocket = endSocket;
}

void GameEmitCharacterAction(GameSession *session, int brawlerIndex,
                             CharacterActionId action)
{
    GameEmitCharacterActionTimed(session, brawlerIndex, action, 0.0f);
}

void GameEmitCharacterActionTimed(GameSession *session, int brawlerIndex,
                                  CharacterActionId action, float duration)
{
    if (brawlerIndex < 0 || brawlerIndex >= session->brawlerCount ||
        action <= CHARACTER_ACTION_NONE || action >= CHARACTER_ACTION_COUNT)
        return;
    GameEvent *event = Push(session, GAME_EVENT_CHARACTER_ACTION);
    if (!event) return;
    event->sourceBrawler = brawlerIndex;
    event->characterAction = action;
    event->life = duration;
}

void GameEmitMatchShake(GameSession *session, float amount)
{
    GameEvent *event = Push(session, GAME_EVENT_MATCH_SHAKE);
    if (event) event->size = amount;
}
