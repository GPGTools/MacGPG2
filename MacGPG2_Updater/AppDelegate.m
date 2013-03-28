#import "AppDelegate.h"

@implementation AppDelegate

- (void)terminateIfIdle {
	if (![updater updateInProgress]) {
		[NSApp terminate:nil];
	}
}
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	updater = [SUUpdater sharedUpdater];
	updater.delegate = self;
	[NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(terminateIfIdle) userInfo:nil repeats:YES];
}

- (void)updateAlert:(SUUpdateAlert *)updateAlert willShowReleaseNotesWithSize:(NSSize *)size {
	size->width = 600;
	size->height = 350;
}

@end
