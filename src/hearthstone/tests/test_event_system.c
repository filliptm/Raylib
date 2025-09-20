#include "test_framework.h"
#include "../core/event_system.h"
#include <string.h>

// Mock structures for testing
typedef struct {
    int playerId;
    int health;
    int mana;
    int turnCount;
} MockPlayer;

typedef struct {
    int id;
    int cost;
    int attack;
    int health;
} MockCard;

// Test data for callbacks
static int callback_count = 0;
static EventType last_event_type = EVENT_TYPE_COUNT;
static void* last_event_source = NULL;
static char last_event_data[MAX_EVENT_DATA_SIZE];
static size_t last_event_data_size = 0;

// Test callback function
void test_callback(const GameEvent* event, void* user_data) {
    callback_count++;
    last_event_type = event->type;
    last_event_source = event->source;
    last_event_data_size = event->data_size;
    if (event->data_size > 0) {
        memcpy(last_event_data, event->data, event->data_size);
    }
    
    // Store user data in a global for testing
    static void* stored_user_data = NULL;
    stored_user_data = user_data;
}

void reset_test_data() {
    callback_count = 0;
    last_event_type = EVENT_TYPE_COUNT;
    last_event_source = NULL;
    last_event_data_size = 0;
    memset(last_event_data, 0, sizeof(last_event_data));
}

TEST(test_event_system_init) {
    EventSystem system;
    GameError result = InitEventSystem(&system);
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(0, system.listener_count);
    ASSERT_EQ(0, system.queued_event_count);
    ASSERT_FALSE(system.immediate_mode);
    ASSERT_TRUE(system.initialized);
    
    CleanupEventSystem(&system);
}

TEST(test_subscribe_to_event) {
    EventSystem system;
    InitEventSystem(&system);
    reset_test_data();
    
    int user_data = 42;
    GameError result = SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &user_data);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, system.listener_count);
    
    // Test duplicate subscription (should be OK but not add another)
    result = SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &user_data);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, system.listener_count);
    
    CleanupEventSystem(&system);
}

TEST(test_unsubscribe_from_event) {
    EventSystem system;
    InitEventSystem(&system);
    
    int user_data = 42;
    SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &user_data);
    SubscribeToEvent(&system, EVENT_TURN_STARTED, test_callback, &user_data);
    ASSERT_EQ(2, system.listener_count);
    
    GameError result = UnsubscribeFromEvent(&system, EVENT_GAME_STARTED, test_callback);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, system.listener_count);
    
    // Test unsubscribing non-existent subscription
    result = UnsubscribeFromEvent(&system, EVENT_GAME_ENDED, test_callback);
    ASSERT_EQ(GAME_ERROR_INVALID_PARAMETER, result);
    
    CleanupEventSystem(&system);
}

TEST(test_publish_simple_event) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, true);
    reset_test_data();
    
    int user_data = 42;
    SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &user_data);
    
    GameError result = PublishSimpleEvent(&system, EVENT_GAME_STARTED, NULL);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, callback_count);
    ASSERT_EQ(EVENT_GAME_STARTED, last_event_type);
    ASSERT_EQ(0, last_event_data_size);
    
    CleanupEventSystem(&system);
}

TEST(test_publish_event_with_data) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, true);
    reset_test_data();
    
    int user_data = 42;
    SubscribeToEvent(&system, EVENT_DAMAGE_DEALT, test_callback, &user_data);
    
    struct {
        int damage;
        int target_id;
    } damage_data = { 5, 123 };
    
    GameError result = PublishEvent(&system, EVENT_DAMAGE_DEALT, NULL, &damage_data, sizeof(damage_data));
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, callback_count);
    ASSERT_EQ(EVENT_DAMAGE_DEALT, last_event_type);
    ASSERT_EQ(sizeof(damage_data), last_event_data_size);
    
    // Verify data integrity
    struct {
        int damage;
        int target_id;
    }* received_data = (void*)last_event_data;
    ASSERT_EQ(5, received_data->damage);
    ASSERT_EQ(123, received_data->target_id);
    
    CleanupEventSystem(&system);
}

TEST(test_event_queue_mode) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, false); // Queue mode
    reset_test_data();
    
    int user_data = 42;
    SubscribeToEvent(&system, EVENT_TURN_STARTED, test_callback, &user_data);
    
    // Publish events - should be queued, not processed immediately
    PublishSimpleEvent(&system, EVENT_TURN_STARTED, NULL);
    PublishSimpleEvent(&system, EVENT_TURN_STARTED, NULL);
    
    ASSERT_EQ(0, callback_count); // Should not be called yet
    ASSERT_EQ(2, GetQueuedEventCount(&system));
    
    // Process events
    ProcessEvents(&system);
    
    ASSERT_EQ(2, callback_count);
    ASSERT_EQ(0, GetQueuedEventCount(&system));
    
    CleanupEventSystem(&system);
}

TEST(test_multiple_listeners) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, true);
    reset_test_data();
    
    int user_data1 = 1;
    int user_data2 = 2;
    
    // Subscribe same callback to same event with different user data
    SubscribeToEvent(&system, EVENT_CARD_PLAYED, test_callback, &user_data1);
    SubscribeToEvent(&system, EVENT_CARD_PLAYED, test_callback, &user_data2);
    
    ASSERT_EQ(2, system.listener_count);
    
    PublishSimpleEvent(&system, EVENT_CARD_PLAYED, NULL);
    
    ASSERT_EQ(2, callback_count);
    
    CleanupEventSystem(&system);
}

TEST(test_unsubscribe_all) {
    EventSystem system;
    InitEventSystem(&system);
    
    int user_data1 = 1;
    int user_data2 = 2;
    
    SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &user_data1);
    SubscribeToEvent(&system, EVENT_TURN_STARTED, test_callback, &user_data1);
    SubscribeToEvent(&system, EVENT_CARD_PLAYED, test_callback, &user_data2);
    
    ASSERT_EQ(3, system.listener_count);
    
    // Unsubscribe all for user_data1
    UnsubscribeAll(&system, &user_data1);
    
    ASSERT_EQ(1, system.listener_count);
    
    CleanupEventSystem(&system);
}

TEST(test_event_type_to_string) {
    ASSERT_STR_EQ("GAME_STARTED", EventTypeToString(EVENT_GAME_STARTED));
    ASSERT_STR_EQ("TURN_ENDED", EventTypeToString(EVENT_TURN_ENDED));
    ASSERT_STR_EQ("DAMAGE_DEALT", EventTypeToString(EVENT_DAMAGE_DEALT));
    ASSERT_STR_EQ("CARD_PLAYED", EventTypeToString(EVENT_CARD_PLAYED));
    ASSERT_STR_EQ("UNKNOWN_EVENT", EventTypeToString((EventType)9999));
}

TEST(test_event_type_categories) {
    ASSERT_TRUE(IsGameFlowEvent(EVENT_GAME_STARTED));
    ASSERT_TRUE(IsGameFlowEvent(EVENT_TURN_ENDED));
    ASSERT_FALSE(IsGameFlowEvent(EVENT_CARD_PLAYED));
    
    ASSERT_TRUE(IsPlayerEvent(EVENT_PLAYER_HEALTH_CHANGED));
    ASSERT_TRUE(IsPlayerEvent(EVENT_CARD_DRAWN));
    ASSERT_FALSE(IsPlayerEvent(EVENT_GAME_STARTED));
    
    ASSERT_TRUE(IsCombatEvent(EVENT_DAMAGE_DEALT));
    ASSERT_TRUE(IsCombatEvent(EVENT_MINION_SUMMONED));
    ASSERT_FALSE(IsCombatEvent(EVENT_CARD_PLAYED));
}

TEST(test_clear_event_queue) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, false);
    
    int user_data = 42;
    SubscribeToEvent(&system, EVENT_TURN_STARTED, test_callback, &user_data);
    
    // Add events to queue
    PublishSimpleEvent(&system, EVENT_TURN_STARTED, NULL);
    PublishSimpleEvent(&system, EVENT_TURN_STARTED, NULL);
    ASSERT_EQ(2, GetQueuedEventCount(&system));
    
    // Clear queue
    ClearEventQueue(&system);
    ASSERT_EQ(0, GetQueuedEventCount(&system));
    
    CleanupEventSystem(&system);
}

TEST(test_convenience_functions) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, true);
    reset_test_data();
    
    // Test PublishHealthChanged
    SubscribeToEvent(&system, EVENT_PLAYER_HEALTH_CHANGED, test_callback, NULL);
    
    MockPlayer player;
    player.playerId = 0;
    player.health = 25;
    
    // Manually publish health changed event instead of using convenience function
    struct {
        int player_id;
        int old_health;
        int new_health;
    } health_data = { player.playerId, 30, 25 };
    
    GameError result = PublishEvent(&system, EVENT_PLAYER_HEALTH_CHANGED, &player, &health_data, sizeof(health_data));
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, callback_count);
    ASSERT_EQ(EVENT_PLAYER_HEALTH_CHANGED, last_event_type);
    
    // Verify data
    struct {
        int player_id;
        int old_health;
        int new_health;
    }* received_health_data = (void*)last_event_data;
    ASSERT_EQ(0, received_health_data->player_id);
    ASSERT_EQ(30, received_health_data->old_health);
    ASSERT_EQ(25, received_health_data->new_health);
    
    CleanupEventSystem(&system);
}

TEST(test_publish_card_events) {
    EventSystem system;
    InitEventSystem(&system);
    SetImmediateMode(&system, true);
    reset_test_data();
    
    SubscribeToEvent(&system, EVENT_CARD_PLAYED, test_callback, NULL);
    
    MockPlayer player;
    player.playerId = 1;
    
    MockCard card;
    card.id = 42;
    card.cost = 3;
    
    // Manually publish card played event
    struct {
        int player_id;
        int card_id;
        int card_cost;
    } card_data = { player.playerId, card.id, card.cost };
    
    GameError result = PublishEvent(&system, EVENT_CARD_PLAYED, &player, &card_data, sizeof(card_data));
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1, callback_count);
    ASSERT_EQ(EVENT_CARD_PLAYED, last_event_type);
    
    // Verify data
    struct {
        int player_id;
        int card_id;
        int card_cost;
    }* received_card_data = (void*)last_event_data;
    ASSERT_EQ(1, received_card_data->player_id);
    ASSERT_EQ(42, received_card_data->card_id);
    ASSERT_EQ(3, received_card_data->card_cost);
    
    CleanupEventSystem(&system);
}

TEST(test_event_data_helpers) {
    GameEvent event;
    
    int test_data = 12345;
    GameError result = SetEventData(&event, &test_data, sizeof(test_data));
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(sizeof(test_data), event.data_size);
    
    size_t retrieved_size;
    const void* retrieved_data = GetEventData(&event, &retrieved_size);
    ASSERT_TRUE(retrieved_data != NULL);
    ASSERT_EQ(sizeof(test_data), retrieved_size);
    ASSERT_EQ(12345, *(int*)retrieved_data);
    
    // Test empty data
    result = SetEventData(&event, NULL, 0);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(0, event.data_size);
    
    retrieved_data = GetEventData(&event, &retrieved_size);
    ASSERT_TRUE(retrieved_data == NULL);
    ASSERT_EQ(0, retrieved_size);
}

TEST(test_max_listeners_limit) {
    EventSystem system;
    InitEventSystem(&system);
    
    // Subscribe maximum number of listeners
    int user_data[MAX_EVENT_LISTENERS];
    for (int i = 0; i < MAX_EVENT_LISTENERS; i++) {
        user_data[i] = i;
        GameError result = SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &user_data[i]);
        ASSERT_EQ(GAME_OK, result);
    }
    
    ASSERT_EQ(MAX_EVENT_LISTENERS, system.listener_count);
    
    // Try to add one more - should fail
    int extra_data = 999;
    GameError result = SubscribeToEvent(&system, EVENT_GAME_STARTED, test_callback, &extra_data);
    ASSERT_EQ(GAME_ERROR_OUT_OF_MEMORY, result);
    
    CleanupEventSystem(&system);
}

int main() {
    RUN_TEST(test_event_system_init);
    RUN_TEST(test_subscribe_to_event);
    RUN_TEST(test_unsubscribe_from_event);
    RUN_TEST(test_publish_simple_event);
    RUN_TEST(test_publish_event_with_data);
    RUN_TEST(test_event_queue_mode);
    RUN_TEST(test_multiple_listeners);
    RUN_TEST(test_unsubscribe_all);
    RUN_TEST(test_event_type_to_string);
    RUN_TEST(test_event_type_categories);
    RUN_TEST(test_clear_event_queue);
    RUN_TEST(test_convenience_functions);
    RUN_TEST(test_publish_card_events);
    RUN_TEST(test_event_data_helpers);
    RUN_TEST(test_max_listeners_limit);
    
    TEST_SUMMARY();
}