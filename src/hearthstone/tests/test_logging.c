#include "test_framework.h"
#include "../utils/logging.h"
#include <string.h>
#include <unistd.h>

TEST(test_logger_init) {
    Logger logger;
    GameError result = InitLogger(&logger, LOG_LEVEL_INFO, true, false, NULL);
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(LOG_LEVEL_INFO, logger.min_level);
    ASSERT_TRUE(logger.console_output);
    ASSERT_FALSE(logger.file_output);
    ASSERT_EQ(0, logger.entry_count);
    ASSERT_TRUE(logger.initialized);
    
    CleanupLogger(&logger);
}

TEST(test_logger_init_with_file) {
    Logger logger;
    GameError result = InitLogger(&logger, LOG_LEVEL_DEBUG, true, true, "test_log.txt");
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(LOG_LEVEL_DEBUG, logger.min_level);
    ASSERT_TRUE(logger.console_output);
    ASSERT_TRUE(logger.file_output);
    ASSERT_STR_EQ("test_log.txt", logger.log_file_path);
    ASSERT_TRUE(logger.log_file != NULL);
    
    CleanupLogger(&logger);
    
    // Clean up test file
    remove("test_log.txt");
}

TEST(test_log_level_to_string) {
    ASSERT_STR_EQ("TRACE", LogLevelToString(LOG_LEVEL_TRACE));
    ASSERT_STR_EQ("DEBUG", LogLevelToString(LOG_LEVEL_DEBUG));
    ASSERT_STR_EQ("INFO ", LogLevelToString(LOG_LEVEL_INFO));
    ASSERT_STR_EQ("WARN ", LogLevelToString(LOG_LEVEL_WARN));
    ASSERT_STR_EQ("ERROR", LogLevelToString(LOG_LEVEL_ERROR));
    ASSERT_STR_EQ("FATAL", LogLevelToString(LOG_LEVEL_FATAL));
}

TEST(test_set_get_log_level) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_INFO, true, false, NULL);
    
    ASSERT_EQ(LOG_LEVEL_INFO, GetLogLevel(&logger));
    
    SetLogLevel(&logger, LOG_LEVEL_WARN);
    ASSERT_EQ(LOG_LEVEL_WARN, GetLogLevel(&logger));
    
    SetLogLevel(&logger, LOG_LEVEL_DEBUG);
    ASSERT_EQ(LOG_LEVEL_DEBUG, GetLogLevel(&logger));
    
    CleanupLogger(&logger);
}

TEST(test_log_message_basic) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL); // No console/file output for test
    
    // Test basic logging
    LOG_INFO(&logger, LOG_CAT_GAME, "Test message");
    ASSERT_EQ(1, GetLogEntryCount(&logger));
    
    LogEntry* entry = GetLogEntry(&logger, 0);
    ASSERT_TRUE(entry != NULL);
    ASSERT_EQ(LOG_LEVEL_INFO, entry->level);
    ASSERT_STR_EQ(LOG_CAT_GAME, entry->category);
    ASSERT_STR_EQ("Test message", entry->message);
    
    CleanupLogger(&logger);
}

TEST(test_log_message_formatting) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LOG_DEBUG(&logger, LOG_CAT_DEBUG, "Player %d has %d health", 1, 25);
    ASSERT_EQ(1, GetLogEntryCount(&logger));
    
    LogEntry* entry = GetLogEntry(&logger, 0);
    ASSERT_TRUE(entry != NULL);
    ASSERT_STR_EQ("Player 1 has 25 health", entry->message);
    
    CleanupLogger(&logger);
}

TEST(test_log_level_filtering) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_WARN, false, false, NULL); // Only WARN and above
    
    LOG_TRACE(&logger, LOG_CAT_DEBUG, "Trace message"); // Should be filtered
    LOG_DEBUG(&logger, LOG_CAT_DEBUG, "Debug message"); // Should be filtered
    LOG_INFO(&logger, LOG_CAT_DEBUG, "Info message");   // Should be filtered
    LOG_WARN(&logger, LOG_CAT_DEBUG, "Warn message");   // Should pass
    LOG_ERROR(&logger, LOG_CAT_DEBUG, "Error message"); // Should pass
    
    ASSERT_EQ(2, GetLogEntryCount(&logger));
    
    LogEntry* entry1 = GetLogEntry(&logger, 0);
    ASSERT_TRUE(entry1 != NULL);
    ASSERT_EQ(LOG_LEVEL_WARN, entry1->level);
    ASSERT_STR_EQ("Warn message", entry1->message);
    
    LogEntry* entry2 = GetLogEntry(&logger, 1);
    ASSERT_TRUE(entry2 != NULL);
    ASSERT_EQ(LOG_LEVEL_ERROR, entry2->level);
    ASSERT_STR_EQ("Error message", entry2->message);
    
    CleanupLogger(&logger);
}

TEST(test_log_entry_circular_buffer) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    // Fill beyond buffer capacity
    for (int i = 0; i < MAX_LOG_ENTRIES + 100; i++) {
        LOG_INFO(&logger, LOG_CAT_DEBUG, "Message %d", i);
    }
    
    // Should have exactly MAX_LOG_ENTRIES
    ASSERT_EQ(MAX_LOG_ENTRIES, GetLogEntryCount(&logger));
    
    // First entry should be from the end of our writes (oldest was overwritten)
    LogEntry* first_entry = GetLogEntry(&logger, 0);
    ASSERT_TRUE(first_entry != NULL);
    
    CleanupLogger(&logger);
}

TEST(test_find_log_entries) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LOG_INFO(&logger, LOG_CAT_GAME, "Game message 1");
    LOG_ERROR(&logger, LOG_CAT_GAME, "Game error");
    LOG_INFO(&logger, LOG_CAT_COMBAT, "Combat message");
    LOG_WARN(&logger, LOG_CAT_GAME, "Game warning");
    
    ASSERT_EQ(4, GetLogEntryCount(&logger));
    
    // Find all GAME category entries
    LogEntry results[10];
    int found = FindLogEntries(&logger, LOG_LEVEL_TRACE, LOG_CAT_GAME, results, 10);
    ASSERT_EQ(3, found);
    
    // Find ERROR level and above
    found = FindLogEntries(&logger, LOG_LEVEL_ERROR, NULL, results, 10);
    ASSERT_EQ(1, found);
    ASSERT_EQ(LOG_LEVEL_ERROR, results[0].level);
    
    CleanupLogger(&logger);
}

TEST(test_count_log_entries_by_level) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LOG_INFO(&logger, LOG_CAT_DEBUG, "Info 1");
    LOG_INFO(&logger, LOG_CAT_DEBUG, "Info 2");
    LOG_WARN(&logger, LOG_CAT_DEBUG, "Warning");
    LOG_ERROR(&logger, LOG_CAT_DEBUG, "Error");
    
    ASSERT_EQ(2, CountLogEntriesByLevel(&logger, LOG_LEVEL_INFO));
    ASSERT_EQ(1, CountLogEntriesByLevel(&logger, LOG_LEVEL_WARN));
    ASSERT_EQ(1, CountLogEntriesByLevel(&logger, LOG_LEVEL_ERROR));
    ASSERT_EQ(0, CountLogEntriesByLevel(&logger, LOG_LEVEL_FATAL));
    
    CleanupLogger(&logger);
}

TEST(test_clear_log_entries) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LOG_INFO(&logger, LOG_CAT_DEBUG, "Message 1");
    LOG_INFO(&logger, LOG_CAT_DEBUG, "Message 2");
    ASSERT_EQ(2, GetLogEntryCount(&logger));
    
    ClearLogEntries(&logger);
    ASSERT_EQ(0, GetLogEntryCount(&logger));
    
    CleanupLogger(&logger);
}

TEST(test_log_game_event) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LogGameEvent(&logger, "GAME_START", "New game started");
    ASSERT_EQ(1, GetLogEntryCount(&logger));
    
    LogEntry* entry = GetLogEntry(&logger, 0);
    ASSERT_TRUE(entry != NULL);
    ASSERT_EQ(LOG_LEVEL_INFO, entry->level);
    ASSERT_STR_EQ(LOG_CAT_GAME, entry->category);
    ASSERT_TRUE(strstr(entry->message, "GAME_START") != NULL);
    ASSERT_TRUE(strstr(entry->message, "New game started") != NULL);
    
    CleanupLogger(&logger);
}

TEST(test_log_performance_metric) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LogPerformanceMetric(&logger, "frame_time", 16.67, "ms");
    ASSERT_EQ(1, GetLogEntryCount(&logger));
    
    LogEntry* entry = GetLogEntry(&logger, 0);
    ASSERT_TRUE(entry != NULL);
    ASSERT_EQ(LOG_LEVEL_INFO, entry->level);
    ASSERT_STR_EQ(LOG_CAT_PERF, entry->category);
    ASSERT_TRUE(strstr(entry->message, "frame_time") != NULL);
    ASSERT_TRUE(strstr(entry->message, "16.670") != NULL);
    ASSERT_TRUE(strstr(entry->message, "ms") != NULL);
    
    CleanupLogger(&logger);
}

TEST(test_log_user_action) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_TRACE, false, false, NULL);
    
    LogUserAction(&logger, "CARD_PLAYED", "Fireball on enemy hero");
    ASSERT_EQ(1, GetLogEntryCount(&logger));
    
    LogEntry* entry = GetLogEntry(&logger, 0);
    ASSERT_TRUE(entry != NULL);
    ASSERT_EQ(LOG_LEVEL_INFO, entry->level);
    ASSERT_STR_EQ(LOG_CAT_INPUT, entry->category);
    ASSERT_TRUE(strstr(entry->message, "CARD_PLAYED") != NULL);
    ASSERT_TRUE(strstr(entry->message, "Fireball on enemy hero") != NULL);
    
    CleanupLogger(&logger);
}

TEST(test_set_log_file) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_INFO, false, false, NULL);
    
    ASSERT_FALSE(logger.file_output);
    
    GameError result = SetLogFile(&logger, "test_new_log.txt");
    ASSERT_EQ(GAME_OK, result);
    ASSERT_TRUE(logger.file_output);
    ASSERT_STR_EQ("test_new_log.txt", logger.log_file_path);
    
    CleanupLogger(&logger);
    
    // Clean up test file
    remove("test_new_log.txt");
}

TEST(test_enable_disable_outputs) {
    Logger logger;
    InitLogger(&logger, LOG_LEVEL_INFO, true, true, "test_output.txt");
    
    ASSERT_TRUE(logger.console_output);
    ASSERT_TRUE(logger.file_output);
    
    EnableConsoleOutput(&logger, false);
    ASSERT_FALSE(logger.console_output);
    
    EnableFileOutput(&logger, false);
    ASSERT_FALSE(logger.file_output);
    
    EnableConsoleOutput(&logger, true);
    ASSERT_TRUE(logger.console_output);
    
    CleanupLogger(&logger);
    
    // Clean up test file
    remove("test_output.txt");
}

int main() {
    RUN_TEST(test_logger_init);
    RUN_TEST(test_logger_init_with_file);
    RUN_TEST(test_log_level_to_string);
    RUN_TEST(test_set_get_log_level);
    RUN_TEST(test_log_message_basic);
    RUN_TEST(test_log_message_formatting);
    RUN_TEST(test_log_level_filtering);
    RUN_TEST(test_log_entry_circular_buffer);
    RUN_TEST(test_find_log_entries);
    RUN_TEST(test_count_log_entries_by_level);
    RUN_TEST(test_clear_log_entries);
    RUN_TEST(test_log_game_event);
    RUN_TEST(test_log_performance_metric);
    RUN_TEST(test_log_user_action);
    RUN_TEST(test_set_log_file);
    RUN_TEST(test_enable_disable_outputs);
    
    TEST_SUMMARY();
}