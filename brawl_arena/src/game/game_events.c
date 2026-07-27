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

void GameEmitMatchShake(GameSession *session, float amount)
{
    GameEvent *event = Push(session, GAME_EVENT_MATCH_SHAKE);
    if (event) event->size = amount;
}
