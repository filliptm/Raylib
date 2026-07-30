#ifndef BRAWL_APP_PLATFORM_H
#define BRAWL_APP_PLATFORM_H

#include <stdbool.h>

typedef struct AppSafeInsets {
    float top;
    float left;
    float bottom;
    float right;
} AppSafeInsets;

// Establishes the platform's read-only resource root and writable profile paths.
// Desktop keeps the caller's working directory and existing environment overrides.
bool AppPlatformPreparePaths(void);
AppSafeInsets AppPlatformSafeInsets(void);

#endif
