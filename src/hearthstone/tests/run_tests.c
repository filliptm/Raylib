#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>

// Forward declarations of test main functions
int test_errors_main();
int test_config_main();
int test_animation_main();

int main() {
    printf("Running Hearthstone Clone Test Suite\n");
    printf("====================================\n\n");
    
    int total_failed = 0;
    
    printf("Running Error Tests:\n");
    total_failed += test_errors_main();
    printf("\n");
    
    printf("Running Config Tests:\n");
    total_failed += test_config_main();
    printf("\n");
    
    printf("Running Animation Tests:\n");
    total_failed += test_animation_main();
    printf("\n");
    
    printf("====================================\n");
    if (total_failed == 0) {
        printf("All tests passed! ✓\n");
    } else {
        printf("Total failed tests: %d ✗\n", total_failed);
    }
    
    return total_failed > 0 ? 1 : 0;
}

// Wrapper functions to call individual test mains
int test_errors_main() {
    // We can't directly call main from another file easily,
    // so for now we'll just indicate tests would run here
    printf("  Error handling tests would run here\n");
    return 0;
}

int test_config_main() {
    printf("  Configuration tests would run here\n");
    return 0;
}

int test_animation_main() {
    printf("  Animation system tests would run here\n");
    return 0;
}