#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TestResults;

static TestResults g_test_results = {0, 0, 0};

#define TEST(name) void name()

#define ASSERT_TRUE(condition) do { \
    g_test_results.total_tests++; \
    if (!(condition)) { \
        printf("  ✗ %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
        g_test_results.failed_tests++; \
    } else { \
        g_test_results.passed_tests++; \
    } \
} while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) do { \
    g_test_results.total_tests++; \
    if ((expected) != (actual)) { \
        printf("  ✗ %s:%d: Expected %d but got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        g_test_results.failed_tests++; \
    } else { \
        g_test_results.passed_tests++; \
    } \
} while(0)

#define ASSERT_STR_EQ(expected, actual) do { \
    g_test_results.total_tests++; \
    if (strcmp(expected, actual) != 0) { \
        printf("  ✗ %s:%d: Expected '%s' but got '%s'\n", __FILE__, __LINE__, expected, actual); \
        g_test_results.failed_tests++; \
    } else { \
        g_test_results.passed_tests++; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("Running %s...\n", #test_func); \
    test_func(); \
} while(0)

#define TEST_SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Test Summary:\n"); \
    printf("Total tests: %d\n", g_test_results.total_tests); \
    printf("Passed: %d\n", g_test_results.passed_tests); \
    printf("Failed: %d\n", g_test_results.failed_tests); \
    printf("========================================\n"); \
    return g_test_results.failed_tests > 0 ? 1 : 0; \
} while(0)

#endif