#include "test_framework.h"
#include "../errors.h"

TEST(test_error_strings) {
    ASSERT_STR_EQ("Success", GetErrorString(GAME_OK));
    ASSERT_STR_EQ("Out of memory", GetErrorString(GAME_ERROR_OUT_OF_MEMORY));
    ASSERT_STR_EQ("File not found", GetErrorString(GAME_ERROR_FILE_NOT_FOUND));
    ASSERT_STR_EQ("Invalid parameter", GetErrorString(GAME_ERROR_INVALID_PARAMETER));
}

TEST(test_log_error) {
    // This test just ensures LogError doesn't crash
    LogError(GAME_OK, "test context");
    LogError(GAME_ERROR_OUT_OF_MEMORY, "test context");
}

int main() {
    RUN_TEST(test_error_strings);
    RUN_TEST(test_log_error);
    
    TEST_SUMMARY();
}