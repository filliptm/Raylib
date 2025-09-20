#include "event_system.h"
#include "../player.h"
#include "../card.h"
#include <string.h>
#include <stdlib.h>

GameError InitEventSystem(EventSystem* system) {
    if (!system) return GAME_ERROR_INVALID_PARAMETER;
    
    memset(system, 0, sizeof(EventSystem));
    
    system->listener_count = 0;
    system->queue_write_index = 0;
    system->queue_read_index = 0;
    system->queued_event_count = 0;
    system->immediate_mode = false;
    system->initialized = true;
    
    return GAME_OK;
}

void CleanupEventSystem(EventSystem* system) {
    if (!system || !system->initialized) return;
    
    // Clear event queue
    ClearEventQueue(system);
    
    // Clear all listeners
    memset(system->listeners, 0, sizeof(system->listeners));
    system->listener_count = 0;
    
    system->initialized = false;
}

GameError SubscribeToEvent(EventSystem* system, EventType event_type, EventListener callback, void* user_data) {
    if (!system || !system->initialized || !callback) return GAME_ERROR_INVALID_PARAMETER;
    
    if (system->listener_count >= MAX_EVENT_LISTENERS) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    // Check for duplicate subscription
    for (int i = 0; i < system->listener_count; i++) {
        EventSubscription* sub = &system->listeners[i];
        if (sub->active && sub->event_type == event_type && sub->callback == callback && sub->user_data == user_data) {
            return GAME_OK; // Already subscribed
        }
    }
    
    // Find free slot
    for (int i = 0; i < MAX_EVENT_LISTENERS; i++) {
        EventSubscription* sub = &system->listeners[i];
        if (!sub->active) {
            sub->event_type = event_type;
            sub->callback = callback;
            sub->user_data = user_data;
            sub->active = true;
            
            if (i >= system->listener_count) {
                system->listener_count = i + 1;
            }
            
            return GAME_OK;
        }
    }
    
    return GAME_ERROR_OUT_OF_MEMORY;
}

GameError UnsubscribeFromEvent(EventSystem* system, EventType event_type, EventListener callback) {
    if (!system || !system->initialized || !callback) return GAME_ERROR_INVALID_PARAMETER;
    
    bool found = false;
    for (int i = 0; i < system->listener_count; i++) {
        EventSubscription* sub = &system->listeners[i];
        if (sub->active && sub->event_type == event_type && sub->callback == callback) {
            sub->active = false;
            found = true;
        }
    }
    
    // Compact listener array
    int write_index = 0;
    for (int read_index = 0; read_index < system->listener_count; read_index++) {
        if (system->listeners[read_index].active) {
            if (write_index != read_index) {
                system->listeners[write_index] = system->listeners[read_index];
            }
            write_index++;
        }
    }
    system->listener_count = write_index;
    
    return found ? GAME_OK : GAME_ERROR_INVALID_PARAMETER;
}

void UnsubscribeAll(EventSystem* system, void* user_data) {
    if (!system || !system->initialized) return;
    
    for (int i = 0; i < system->listener_count; i++) {
        EventSubscription* sub = &system->listeners[i];
        if (sub->active && sub->user_data == user_data) {
            sub->active = false;
        }
    }
    
    // Compact listener array
    int write_index = 0;
    for (int read_index = 0; read_index < system->listener_count; read_index++) {
        if (system->listeners[read_index].active) {
            if (write_index != read_index) {
                system->listeners[write_index] = system->listeners[read_index];
            }
            write_index++;
        }
    }
    system->listener_count = write_index;
}

GameError PublishEvent(EventSystem* system, EventType event_type, void* source, const void* data, size_t data_size) {
    if (!system || !system->initialized) return GAME_ERROR_INVALID_PARAMETER;
    
    if (data_size > MAX_EVENT_DATA_SIZE) {
        return GAME_ERROR_INVALID_PARAMETER;
    }
    
    if (system->immediate_mode) {
        // Create temporary event and process immediately
        GameEvent temp_event;
        temp_event.type = event_type;
        temp_event.source = source;
        temp_event.data_size = data_size;
        temp_event.timestamp = 0.0f; // TODO: Add proper timing
        temp_event.processed = false;
        
        if (data && data_size > 0) {
            memcpy(temp_event.data, data, data_size);
        }
        
        ProcessEvent(system, &temp_event);
        return GAME_OK;
    }
    
    // Queue the event
    if (system->queued_event_count >= MAX_EVENT_QUEUE) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    GameEvent* event = &system->event_queue[system->queue_write_index];
    event->type = event_type;
    event->source = source;
    event->data_size = data_size;
    event->timestamp = 0.0f; // TODO: Add proper timing
    event->processed = false;
    
    if (data && data_size > 0) {
        memcpy(event->data, data, data_size);
    }
    
    system->queue_write_index = (system->queue_write_index + 1) % MAX_EVENT_QUEUE;
    system->queued_event_count++;
    
    return GAME_OK;
}

GameError PublishSimpleEvent(EventSystem* system, EventType event_type, void* source) {
    return PublishEvent(system, event_type, source, NULL, 0);
}

void ProcessEvents(EventSystem* system) {
    if (!system || !system->initialized) return;
    
    while (system->queued_event_count > 0) {
        GameEvent* event = &system->event_queue[system->queue_read_index];
        
        ProcessEvent(system, event);
        
        system->queue_read_index = (system->queue_read_index + 1) % MAX_EVENT_QUEUE;
        system->queued_event_count--;
    }
}

void ProcessEvent(EventSystem* system, const GameEvent* event) {
    if (!system || !system->initialized || !event) return;
    
    // Call all listeners for this event type
    for (int i = 0; i < system->listener_count; i++) {
        EventSubscription* sub = &system->listeners[i];
        if (sub->active && sub->event_type == event->type) {
            sub->callback(event, sub->user_data);
        }
    }
}

void SetImmediateMode(EventSystem* system, bool immediate) {
    if (!system || !system->initialized) return;
    system->immediate_mode = immediate;
}

int GetQueuedEventCount(EventSystem* system) {
    if (!system || !system->initialized) return 0;
    return system->queued_event_count;
}

void ClearEventQueue(EventSystem* system) {
    if (!system || !system->initialized) return;
    
    system->queue_read_index = 0;
    system->queue_write_index = 0;
    system->queued_event_count = 0;
    memset(system->event_queue, 0, sizeof(system->event_queue));
}

GameError SetEventData(GameEvent* event, const void* data, size_t size) {
    if (!event || size > MAX_EVENT_DATA_SIZE) return GAME_ERROR_INVALID_PARAMETER;
    
    if (data && size > 0) {
        memcpy(event->data, data, size);
        event->data_size = size;
    } else {
        event->data_size = 0;
    }
    
    return GAME_OK;
}

const void* GetEventData(const GameEvent* event, size_t* size) {
    if (!event) return NULL;
    
    if (size) {
        *size = event->data_size;
    }
    
    return event->data_size > 0 ? event->data : NULL;
}

// Convenience functions for common events

GameError PublishHealthChanged(EventSystem* system, Player* player, int old_health, int new_health) {
    struct {
        int player_id;
        int old_health;
        int new_health;
    } data = { player->playerId, old_health, new_health };
    
    return PublishEvent(system, EVENT_PLAYER_HEALTH_CHANGED, player, &data, sizeof(data));
}

GameError PublishManaChanged(EventSystem* system, Player* player, int old_mana, int new_mana) {
    struct {
        int player_id;
        int old_mana;
        int new_mana;
    } data = { player->playerId, old_mana, new_mana };
    
    return PublishEvent(system, EVENT_PLAYER_MANA_CHANGED, player, &data, sizeof(data));
}

GameError PublishCardPlayed(EventSystem* system, Player* player, Card* card) {
    struct {
        int player_id;
        int card_id;
        int card_cost;
    } data = { player->playerId, card->id, card->cost };
    
    return PublishEvent(system, EVENT_CARD_PLAYED, player, &data, sizeof(data));
}

GameError PublishCardDrawn(EventSystem* system, Player* player, Card* card) {
    struct {
        int player_id;
        int card_id;
    } data = { player->playerId, card->id };
    
    return PublishEvent(system, EVENT_CARD_DRAWN, player, &data, sizeof(data));
}

GameError PublishDamageDealt(EventSystem* system, void* source, void* target, int damage) {
    struct {
        void* source;
        void* target;
        int damage;
    } data = { source, target, damage };
    
    return PublishEvent(system, EVENT_DAMAGE_DEALT, source, &data, sizeof(data));
}

GameError PublishMinionSummoned(EventSystem* system, Player* player, Card* minion) {
    struct {
        int player_id;
        int minion_id;
        int attack;
        int health;
    } data = { player->playerId, minion->id, minion->attack, minion->health };
    
    return PublishEvent(system, EVENT_MINION_SUMMONED, player, &data, sizeof(data));
}

GameError PublishMinionDied(EventSystem* system, Player* player, Card* minion) {
    struct {
        int player_id;
        int minion_id;
    } data = { player->playerId, minion->id };
    
    return PublishEvent(system, EVENT_MINION_DIED, player, &data, sizeof(data));
}

GameError PublishSpellCast(EventSystem* system, Player* player, Card* spell, void* target) {
    struct {
        int player_id;
        int spell_id;
        void* target;
    } data = { player->playerId, spell->id, target };
    
    return PublishEvent(system, EVENT_SPELL_CAST, player, &data, sizeof(data));
}

GameError PublishTurnStarted(EventSystem* system, Player* player) {
    struct {
        int player_id;
        int turn_count;
    } data = { player->playerId, player->turnCount };
    
    return PublishEvent(system, EVENT_TURN_STARTED, player, &data, sizeof(data));
}

GameError PublishTurnEnded(EventSystem* system, Player* player) {
    struct {
        int player_id;
        int turn_count;
    } data = { player->playerId, player->turnCount };
    
    return PublishEvent(system, EVENT_TURN_ENDED, player, &data, sizeof(data));
}

GameError PublishGameEnded(EventSystem* system, int winner) {
    struct {
        int winner_id;
    } data = { winner };
    
    return PublishEvent(system, EVENT_GAME_ENDED, NULL, &data, sizeof(data));
}

const char* EventTypeToString(EventType type) {
    switch (type) {
        case EVENT_GAME_STARTED: return "GAME_STARTED";
        case EVENT_GAME_ENDED: return "GAME_ENDED";
        case EVENT_TURN_STARTED: return "TURN_STARTED";
        case EVENT_TURN_ENDED: return "TURN_ENDED";
        case EVENT_PHASE_CHANGED: return "PHASE_CHANGED";
        case EVENT_PLAYER_HEALTH_CHANGED: return "PLAYER_HEALTH_CHANGED";
        case EVENT_PLAYER_MANA_CHANGED: return "PLAYER_MANA_CHANGED";
        case EVENT_PLAYER_DIED: return "PLAYER_DIED";
        case EVENT_CARD_DRAWN: return "CARD_DRAWN";
        case EVENT_CARD_PLAYED: return "CARD_PLAYED";
        case EVENT_CARD_DISCARDED: return "CARD_DISCARDED";
        case EVENT_MINION_SUMMONED: return "MINION_SUMMONED";
        case EVENT_MINION_DIED: return "MINION_DIED";
        case EVENT_DAMAGE_DEALT: return "DAMAGE_DEALT";
        case EVENT_HEALING_APPLIED: return "HEALING_APPLIED";
        case EVENT_ATTACK_DECLARED: return "ATTACK_DECLARED";
        case EVENT_COMBAT_RESOLVED: return "COMBAT_RESOLVED";
        case EVENT_SPELL_CAST: return "SPELL_CAST";
        case EVENT_BATTLECRY_TRIGGERED: return "BATTLECRY_TRIGGERED";
        case EVENT_DEATHRATTLE_TRIGGERED: return "DEATHRATTLE_TRIGGERED";
        case EVENT_PLAY_SOUND: return "PLAY_SOUND";
        case EVENT_SHOW_ANIMATION: return "SHOW_ANIMATION";
        case EVENT_SHAKE_SCREEN: return "SHAKE_SCREEN";
        case EVENT_CARD_SELECTED: return "CARD_SELECTED";
        case EVENT_CARD_DESELECTED: return "CARD_DESELECTED";
        case EVENT_TARGET_SELECTED: return "TARGET_SELECTED";
        case EVENT_INVALID_ACTION: return "INVALID_ACTION";
        case EVENT_GAME_SAVED: return "GAME_SAVED";
        case EVENT_GAME_LOADED: return "GAME_LOADED";
        case EVENT_PERFORMANCE_WARNING: return "PERFORMANCE_WARNING";
        case EVENT_MEMORY_WARNING: return "MEMORY_WARNING";
        default: return "UNKNOWN_EVENT";
    }
}

bool IsGameFlowEvent(EventType type) {
    return type >= EVENT_GAME_STARTED && type <= EVENT_PHASE_CHANGED;
}

bool IsPlayerEvent(EventType type) {
    return type >= EVENT_PLAYER_HEALTH_CHANGED && type <= EVENT_CARD_DISCARDED;
}

bool IsCombatEvent(EventType type) {
    return type >= EVENT_MINION_SUMMONED && type <= EVENT_DEATHRATTLE_TRIGGERED;
}