#ifndef BRAWL_IOS_BRIDGE_H
#define BRAWL_IOS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BrawlIOSSafeAreaInsets {
    float top;
    float left;
    float bottom;
    float right;
} BrawlIOSSafeAreaInsets;

const char *BrawlIOSApplicationSupportPath(void);
BrawlIOSSafeAreaInsets BrawlIOSGetSafeAreaInsets(void);

#ifdef __cplusplus
}
#endif

#endif
