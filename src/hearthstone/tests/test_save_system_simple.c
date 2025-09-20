#include "test_framework.h"
#include "../core/save_system.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

TEST(test_save_system_init) {
    SaveManager manager;
    GameError result = InitSaveSystem(&manager, "test_saves/");
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_STR_EQ("test_saves/", manager.save_directory);
    ASSERT_EQ(0, manager.slot_count);
    
    CleanupSaveSystem(&manager);
}

TEST(test_validate_save_name) {
    // Valid names
    ASSERT_TRUE(ValidateSaveName("test_save"));
    ASSERT_TRUE(ValidateSaveName("my_game_123"));
    ASSERT_TRUE(ValidateSaveName("Save Game"));
    
    // Invalid names
    ASSERT_FALSE(ValidateSaveName(NULL));
    ASSERT_FALSE(ValidateSaveName(""));
    ASSERT_FALSE(ValidateSaveName("save/with/slash"));
    ASSERT_FALSE(ValidateSaveName("save\\with\\backslash"));
    ASSERT_FALSE(ValidateSaveName("save:with:colon"));
    ASSERT_FALSE(ValidateSaveName("save*with*asterisk"));
    ASSERT_FALSE(ValidateSaveName("save?with?question"));
    ASSERT_FALSE(ValidateSaveName("save\"with\"quote"));
    ASSERT_FALSE(ValidateSaveName("save<with>brackets"));
    ASSERT_FALSE(ValidateSaveName("save|with|pipe"));
    
    // Too long name
    char long_name[MAX_SAVE_NAME + 10];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    ASSERT_FALSE(ValidateSaveName(long_name));
}

TEST(test_generate_save_filename) {
    char* filename = GenerateSaveFilename("test_save");
    ASSERT_TRUE(filename != NULL);
    ASSERT_STR_EQ("test_save.hsv", filename);
    free(filename);
    
    filename = GenerateSaveFilename("my_game");
    ASSERT_TRUE(filename != NULL);
    ASSERT_STR_EQ("my_game.hsv", filename);
    free(filename);
    
    // Test NULL input
    filename = GenerateSaveFilename(NULL);
    ASSERT_TRUE(filename == NULL);
}

TEST(test_create_save_directory) {
    GameError result = CreateSaveDirectory("test_temp_dir/");
    ASSERT_EQ(GAME_OK, result);
    
    // Check directory exists
    struct stat st = {0};
    ASSERT_EQ(0, stat("test_temp_dir/", &st));
    
    // Clean up
    rmdir("test_temp_dir/");
}

TEST(test_is_valid_save_file) {
    // Create a test file with valid JSON structure
    FILE* test_file = fopen("test_valid.hsv", "w");
    ASSERT_TRUE(test_file != NULL);
    
    fprintf(test_file, "{\n");
    fprintf(test_file, "  \"version\": \"1.0\",\n");
    fprintf(test_file, "  \"game_state\": {\n");
    fprintf(test_file, "    \"turn_number\": 1\n");
    fprintf(test_file, "  }\n");
    fprintf(test_file, "}\n");
    fclose(test_file);
    
    // Test valid file
    ASSERT_TRUE(IsValidSaveFile("test_valid.hsv"));
    
    // Test non-existent file
    ASSERT_FALSE(IsValidSaveFile("nonexistent.hsv"));
    
    // Create invalid file
    test_file = fopen("test_invalid.hsv", "w");
    ASSERT_TRUE(test_file != NULL);
    fprintf(test_file, "This is not valid JSON\n");
    fclose(test_file);
    
    // Test invalid file
    ASSERT_FALSE(IsValidSaveFile("test_invalid.hsv"));
    
    // Clean up
    remove("test_valid.hsv");
    remove("test_invalid.hsv");
}

TEST(test_save_slot_refresh) {
    SaveManager manager;
    InitSaveSystem(&manager, "test_saves/");
    
    // Create test save files
    FILE* test_file1 = fopen("test_saves/save1.hsv", "w");
    ASSERT_TRUE(test_file1 != NULL);
    fprintf(test_file1, "{\"version\": \"1.0\", \"game_state\": {\"turn_number\": 1}}\n");
    fclose(test_file1);
    
    FILE* test_file2 = fopen("test_saves/save2.hsv", "w");
    ASSERT_TRUE(test_file2 != NULL);
    fprintf(test_file2, "{\"version\": \"1.0\", \"game_state\": {\"turn_number\": 2}}\n");
    fclose(test_file2);
    
    // Refresh slots
    GameError result = RefreshSaveSlots(&manager);
    ASSERT_EQ(GAME_OK, result);
    ASSERT_TRUE(manager.slot_count >= 2);
    
    // Test slot retrieval
    SaveSlot* slot = GetSaveSlot(&manager, "save1");
    ASSERT_TRUE(slot != NULL);
    ASSERT_STR_EQ("save1", slot->name);
    ASSERT_TRUE(slot->is_valid);
    
    slot = GetSaveSlotByIndex(&manager, 0);
    ASSERT_TRUE(slot != NULL);
    
    // Test non-existent save
    slot = GetSaveSlot(&manager, "nonexistent");
    ASSERT_TRUE(slot == NULL);
    
    // Clean up
    remove("test_saves/save1.hsv");
    remove("test_saves/save2.hsv");
    CleanupSaveSystem(&manager);
}

TEST(test_delete_save) {
    SaveManager manager;
    InitSaveSystem(&manager, "test_saves/");
    
    // Create a test save file
    FILE* test_file = fopen("test_saves/delete_test.hsv", "w");
    ASSERT_TRUE(test_file != NULL);
    fprintf(test_file, "{\"version\": \"1.0\", \"game_state\": {\"turn_number\": 1}}\n");
    fclose(test_file);
    
    // Refresh to pick up the file
    RefreshSaveSlots(&manager);
    
    // Verify it exists
    SaveSlot* slot = GetSaveSlot(&manager, "delete_test");
    ASSERT_TRUE(slot != NULL);
    
    // Delete it
    GameError result = DeleteSave(&manager, "delete_test");
    ASSERT_EQ(GAME_OK, result);
    
    // Verify it's gone
    slot = GetSaveSlot(&manager, "delete_test");
    ASSERT_TRUE(slot == NULL);
    
    CleanupSaveSystem(&manager);
}

int main() {
    RUN_TEST(test_save_system_init);
    RUN_TEST(test_validate_save_name);
    RUN_TEST(test_generate_save_filename);
    RUN_TEST(test_create_save_directory);
    RUN_TEST(test_is_valid_save_file);
    RUN_TEST(test_save_slot_refresh);
    RUN_TEST(test_delete_save);
    
    TEST_SUMMARY();
}