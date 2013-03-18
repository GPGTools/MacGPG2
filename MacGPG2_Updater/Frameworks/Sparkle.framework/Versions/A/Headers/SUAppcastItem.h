//
//  SUAppcastItem.h
//  Sparkle
//
//  Created by Andy Matuschak on 3/12/06.
//  Copyright 2006 Andy Matuschak. All rights reserved.
//

#ifndef SUAPPCASTITEM_H
#define SUAPPCASTITEM_H

@interface SUAppcastItem : NSObject
{
@private
	NSString *title;
	NSDate *date;
	NSString *itemDescription;
	
	NSURL *releaseNotesURL;
	
	NSString *DSASignature;	
	NSString *minimumSystemVersion;
    NSString *maximumSystemVersion;
	
	NSURL *fileURL;
	NSString *versionString;
	NSString *displayVersionString;

	NSDictionary *deltaUpdates;

	NSDictionary *propertiesDictionary;
	
	NSURL *infoURL;	// UK 2007-08-31
}

// Initializes with data from a dictionary provided by the RSS class.
- initWithDictionary:(NSDictionary *)dict;
- initWithDictionary:(NSDictionary *)dict failureReason:(NSString**)error;

@property (retain) NSString *title, *itemDescription, *DSASignature, *versionString, *displayVersionString, *minimumSystemVersion, *maximumSystemVersion;
@property (retain) NSDate *date;
@property (retain) NSURL *releaseNotesURL, *fileURL, *infoURL;
@property (retain) NSDictionary *deltaUpdates;

@property (readonly) BOOL isDeltaUpdate;
// Returns the dictionary provided in initWithDictionary; this might be useful later for extensions.
@property (readonly) NSDictionary *propertiesDictionary;



@end

#endif
