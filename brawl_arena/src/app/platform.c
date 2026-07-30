#include "platform.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(PLATFORM_IOS)
#include "BrawlIOSBridge.h"
static bool SetWritablePath(const char *environmentName, const char *directory,
                            const char *filename)
{
    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/%s", directory, filename);
    if (written <= 0 || written >= (int)sizeof(path)) return false;
    return setenv(environmentName, path, 1) == 0;
}
#endif

bool AppPlatformPreparePaths(void)
{
#if defined(PLATFORM_IOS)
    char resources[1024];
    int written = snprintf(resources, sizeof(resources), "%sBrawlAssets",
                           GetApplicationDirectory());
    if (written <= 0 || written >= (int)sizeof(resources) ||
        !ChangeDirectory(resources))
    {
        TraceLog(LOG_ERROR, "iOS resource root is unavailable: %s", resources);
        return false;
    }

    const char *support = BrawlIOSApplicationSupportPath();
    if (!support || !support[0] ||
        !SetWritablePath("BRAWL_TUNING", support, "tuning.local.cfg") ||
        !SetWritablePath("BRAWL_PROFILE", support, "profile.cfg") ||
        !SetWritablePath("BRAWL_LEGACY_TUNING", support, "tuning.cfg"))
    {
        TraceLog(LOG_ERROR, "iOS Application Support paths are unavailable");
        return false;
    }
#endif
    return true;
}

AppSafeInsets AppPlatformSafeInsets(void)
{
#if defined(PLATFORM_IOS)
    BrawlIOSSafeAreaInsets insets = BrawlIOSGetSafeAreaInsets();
    return (AppSafeInsets){
        insets.top, insets.left, insets.bottom, insets.right
    };
#else
    return (AppSafeInsets){ 0 };
#endif
}

bool AppPlatformIsMobile(void)
{
#if defined(BRAWL_MOBILE)
    return true;
#else
    return false;
#endif
}
