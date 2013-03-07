#import "AppDelegate.h"

@implementation AppDelegate

- (void)terminateIfIdle {
	if (![updater updateInProgress]) {
		[NSApp terminate:nil];
	}
}
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	updater = [SUUpdater sharedUpdater];
	[NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(terminateIfIdle) userInfo:nil repeats:YES];
}

@end
