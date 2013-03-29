#import <Cocoa/Cocoa.h>
#import <Sparkle/Sparkle.h>

@interface AppDelegate : NSObject <NSApplicationDelegate> {
	SUUpdater *updater;
}
- (void)terminateIfIdle;

@end
