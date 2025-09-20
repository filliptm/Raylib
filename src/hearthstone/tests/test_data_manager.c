#include "test_framework.h"
#include "../core/data_manager.h"
#include <string.h>

TEST(test_data_manager_init) {
    DataManager dm;
    GameError result = InitDataManager(&dm);
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(0, dm.card_count);
    ASSERT_EQ(0, dm.deck_template_count);
    ASSERT_EQ(30, dm.game_settings.starting_health);
    ASSERT_EQ(10, dm.game_settings.max_mana);
    
    CleanupDataManager(&dm);
}

TEST(test_load_card_database) {
    DataManager dm;
    InitDataManager(&dm);
    
    GameError result = LoadCardDatabase(&dm, "data/cards.json");
    ASSERT_EQ(GAME_OK, result);
    ASSERT_TRUE(dm.card_count > 0);
    
    // Check first card
    CardData* goblin = FindCardByName(&dm, "Goblin Warrior");
    ASSERT_TRUE(goblin != NULL);
    ASSERT_EQ(1, goblin->id);
    ASSERT_EQ(CARD_TYPE_MINION, goblin->type);
    ASSERT_EQ(1, goblin->cost);
    ASSERT_EQ(2, goblin->attack);
    ASSERT_EQ(1, goblin->health);
    ASSERT_TRUE(goblin->charge);
    
    CleanupDataManager(&dm);
}

TEST(test_find_card_functions) {
    DataManager dm;
    InitDataManager(&dm);
    LoadCardDatabase(&dm, "data/cards.json");
    
    // Test find by ID
    CardData* card = FindCardById(&dm, 3);
    ASSERT_TRUE(card != NULL);
    ASSERT_STR_EQ("Fireball", card->name);
    ASSERT_EQ(CARD_TYPE_SPELL, card->type);
    
    // Test find by name
    card = FindCardByName(&dm, "Shield Bearer");
    ASSERT_TRUE(card != NULL);
    ASSERT_EQ(2, card->id);
    ASSERT_TRUE(card->taunt);
    
    // Test not found
    card = FindCardById(&dm, 999);
    ASSERT_TRUE(card == NULL);
    
    card = FindCardByName(&dm, "Nonexistent Card");
    ASSERT_TRUE(card == NULL);
    
    CleanupDataManager(&dm);
}

TEST(test_create_card_from_data) {
    DataManager dm;
    InitDataManager(&dm);
    LoadCardDatabase(&dm, "data/cards.json");
    
    CardData* data = FindCardByName(&dm, "Dragon");
    ASSERT_TRUE(data != NULL);
    
    // For now, just test that we can find the card data
    // Full Card creation will be tested in integration tests
    ASSERT_STR_EQ("Dragon", data->name);
    ASSERT_EQ(CARD_TYPE_MINION, data->type);
    ASSERT_EQ(7, data->cost);
    ASSERT_EQ(8, data->attack);
    ASSERT_EQ(8, data->health);
    ASSERT_TRUE(data->divine_shield);
    
    CleanupDataManager(&dm);
}

TEST(test_validate_card_data) {
    CardData valid_card = {
        .id = 1,
        .name = "Test Card",
        .type = CARD_TYPE_MINION,
        .cost = 3,
        .attack = 3,
        .health = 3
    };
    
    ASSERT_TRUE(ValidateCardData(&valid_card));
    
    // Test invalid cost
    CardData invalid_card = valid_card;
    invalid_card.cost = -1;
    ASSERT_FALSE(ValidateCardData(&invalid_card));
    
    // Test invalid health for minion
    invalid_card = valid_card;
    invalid_card.health = 0;
    ASSERT_FALSE(ValidateCardData(&invalid_card));
    
    // Test empty name
    invalid_card = valid_card;
    invalid_card.name[0] = '\0';
    ASSERT_FALSE(ValidateCardData(&invalid_card));
}

int main() {
    RUN_TEST(test_data_manager_init);
    RUN_TEST(test_load_card_database);
    RUN_TEST(test_find_card_functions);
    RUN_TEST(test_create_card_from_data);
    RUN_TEST(test_validate_card_data);
    
    TEST_SUMMARY();
}