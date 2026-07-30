#import "BrawlIOSBridge.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

extern "C" const char *BrawlIOSApplicationSupportPath(void)
{
    static NSString *supportPath = nil;
    static const char *utf8Path = nullptr;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSFileManager *files = NSFileManager.defaultManager;
        NSURL *base = [files URLsForDirectory:NSApplicationSupportDirectory
                                    inDomains:NSUserDomainMask].firstObject;
        NSURL *directory = [base URLByAppendingPathComponent:@"BrawlArena"
                                                isDirectory:YES];
        NSError *error = nil;
        [files createDirectoryAtURL:directory
        withIntermediateDirectories:YES
                         attributes:nil
                              error:&error];
        if (error)
            NSLog(@"Brawl Arena could not create Application Support: %@", error);
        supportPath = directory.path;
        utf8Path = supportPath.fileSystemRepresentation;
    });
    return utf8Path;
}

extern "C" BrawlIOSSafeAreaInsets BrawlIOSGetSafeAreaInsets(void)
{
    UIWindow *window = nil;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
    {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        for (UIWindow *candidate in ((UIWindowScene *)scene).windows)
        {
            if (candidate.isKeyWindow)
            {
                window = candidate;
                break;
            }
        }
        if (window) break;
    }

    UIEdgeInsets native = window ? window.safeAreaInsets : UIEdgeInsetsZero;
    return (BrawlIOSSafeAreaInsets){
        (float)native.top,
        (float)native.left,
        (float)native.bottom,
        (float)native.right
    };
}
