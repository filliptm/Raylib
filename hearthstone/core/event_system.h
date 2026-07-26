#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include "../common.h"
#include "../errors.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_EVENT_LISTENERS 32
#define MAX_EVENT_QUEUE 128
#define MAX_EVENT_DATA_SIZE 256

typedef enum {
    // Game flow events
    EVENT_GAME_STARTED,
    EVENT_GAME_ENDED,
    EVENT_TURN_STARTED,
    EVENT_TURN_ENDED,
    EVENT_PHASE_CHANGED,
    
    // Player events
    EVENT_PLAYER_HEALTH_CHANGED,
    EVENT_PLAYER_MANA_CHANGED,
    EVENT_PLAYER_DIED,
    EVENT_CARD_DRAWN,
    EVENT_CARD_PLAYED,
    EVENT_CARD_DISCARDED,
    
    // Combat events
    EVENT_MINION_SUMMONED,
    EVENT_MINION_DIED,
    EVENT_DAMAGE_DEALT,
    EVENT_HEALING_APPLIED,
    EVENT_ATTACK_DECLARED,
    EVENT_COMBAT_RESOLVED,
    
    // Spell events
    EVENT_SPELL_CAST,
    EVENT_BATTLECRY_TRIGGERED,
    EVENT_DEATHRATTLE_TRIGGERED,
    
    // Audio/Visual events
    EVENT_PLAY_SOUND,
    EVENT_SHOW_ANIMATION,
    EVENT_SHAKE_SCREEN,
    
    // Input events
    EVENT_CARD_SELECTED,
    EVENT_CARD_DESELECTED,
    EVENT_TARGET_SELECTED,
    EVENT_INVALID_ACTION,
    
    // Save/Load events
    EVENT_GAME_SAVED,
    EVENT_GAME_LOADED,
    
    // Performance events
    EVENT_PERFORMANCE_WARNING,
    EVENT_MEMORY_WARNING,
    
    EVENT_TYPE_COUNT
} EventType;

typedef struct {
    EventType type;
    void* source;
    char data[MAX_EVENT_DATA_SIZE];
    size_t data_size;
    float timestamp;
    bool processed;
} GameEvent;

// Event listener callback function type
typedef void (*EventListener)(const GameEvent* event, void* user_data);

typedef struct {
    EventType event_type;
    EventListener callback;
    void* user_data;
    bool active;
} EventSubscription;

typedef struct {
    EventSubscription listeners[MAX_EVENT_LISTENERS];
    int listener_count;
    
    GameEvent event_queue[MAX_EVENT_QUEUE];
    int queue_write_index;
    int queue_read_index;
    int queued_event_count;
    
    bool immediate_mode;
    bool initialized;
} EventSystem;

// Event system initialization
GameError InitEventSystem(EventSystem* system);
void CleanupEventSystem(EventSystem* system);

// Event subscription management
GameError SubscribeToEvent(EventSystem* system, EventType event_type, EventListener callback, void* user_data);
GameError UnsubscribeFromEvent(EventSystem* system, EventType event_type, EventListener callback);
void UnsubscribeAll(EventSystem* system, void* user_data);

// Event publishing
GameError PublishEvent(EventSystem* system, EventType event_type, void* source, const void* data, size_t data_size);
GameError PublishSimpleEvent(EventSystem* system, EventType event_type, void* source);

// Event processing
void ProcessEvents(EventSystem* system);
void ProcessEvent(EventSystem* system, const GameEvent* event);
void SetImmediateMode(EventSystem* system, bool immediate);

// Event queue management
int GetQueuedEventCount(EventSystem* system);
void ClearEventQueue(EventSystem* system);

// Event data helpers
GameError SetEventData(GameEvent* event, const void* data, size_t size);
const void* GetEventData(const GameEvent* event, size_t* size);

// Convenience functions for common events
GameError PublishHealthChanged(EventSystem* system, Player* player, int old_health, int new_health);
GameError PublishManaChanged(EventSystem* system, Player* player, int old_mana, int new_mana);
GameError PublishCardPlayed(EventSystem* system, Player* player, Card* card);
GameError PublishCardDrawn(EventSystem* system, Player* player, Card* card);
GameError PublishDamageDealt(EventSystem* system, void* source, void* target, int damage);
GameError PublishMinionSummoned(EventSystem* system, Player* player, Card* minion);
GameError PublishMinionDied(EventSystem* system, Player* player, Card* minion);
GameError PublishSpellCast(EventSystem* system, Player* player, Card* spell, void* target);
GameError PublishTurnStarted(EventSystem* system, Player* player);
GameError PublishTurnEnded(EventSystem* system, Player* player);
GameError PublishGameEnded(EventSystem* system, int winner);

// Event type utilities
const char* EventTypeToString(EventType type);
bool IsGameFlowEvent(EventType type);
bool IsPlayerEvent(EventType type);
bool IsCombatEvent(EventType type);

#endif // EVENT_SYSTEM_H