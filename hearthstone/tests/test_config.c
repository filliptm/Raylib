#include "test_framework.h"
#include "../config.h"
#include <string.h>

TEST(test_init_config) {
    GameConfig config;
    GameError result = InitConfig(&config);
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1920, config.screen_width);
    ASSERT_EQ(1080, config.screen_height);
    ASSERT_EQ(60, config.target_fps);
    ASSERT_EQ(1.0f, config.card_scale);
}

TEST(test_config_validation) {
    GameConfig config;
    InitConfig(&config);
    
    // Test valid screen sizes
    ASSERT_EQ(1920, GetConfigScreenWidth(&config));
    ASSERT_EQ(1080, GetConfigScreenHeight(&config));
    
    // Test card scale validation
    config.card_scale = 2.0f;
    ASSERT_EQ(2.0f, GetConfigCardScale(&config));
    
    config.card_scale = -1.0f;  // Invalid
    ASSERT_EQ(1.0f, GetConfigCardScale(&config));  // Should return default
}

TEST(test_update_screen_size) {
    GameConfig config;
    InitConfig(&config);
    
    GameError result = UpdateScreenSize(&config, 1600, 900);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(1600, config.screen_width);
    ASSERT_EQ(900, config.screen_height);
    
    // Test invalid sizes
    result = UpdateScreenSize(&config, 100, 100);  // Too small
    ASSERT_EQ(GAME_ERROR_INVALID_PARAMETER, result);
}

TEST(test_update_volume) {
    GameConfig config;
    InitConfig(&config);
    
    GameError result = UpdateVolume(&config, 0.8f, 0.6f, 0.4f);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(0.8f, config.master_volume);
    ASSERT_EQ(0.6f, config.sfx_volume);
    ASSERT_EQ(0.4f, config.music_volume);
    
    // Test invalid volumes
    result = UpdateVolume(&config, 2.0f, 0.5f, 0.5f);  // Too high
    ASSERT_EQ(GAME_ERROR_INVALID_PARAMETER, result);
}

int main() {
    RUN_TEST(test_init_config);
    RUN_TEST(test_config_validation);
    RUN_TEST(test_update_screen_size);
    RUN_TEST(test_update_volume);
    
    TEST_SUMMARY();
}