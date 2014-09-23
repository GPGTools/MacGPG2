#ifndef Sparkle_ClassPrefixes_h
#define Sparkle_ClassPrefixes_h


#define ClassPrefix GPGT

#ifdef ClassPrefix
#define _ClassPrefix_CONCAT_2(c,d) c ## d
#define _ClassPrefix_CONCAT(a,b) _ClassPrefix_CONCAT_2(a,b)
#define ClassPrefix_PREPEND(x) _ClassPrefix_CONCAT(ClassPrefix, x)

//Update Control
#define SUUpdater ClassPrefix_PREPEND(SUUpdater)
//Drivers
#define SUUpdateDriver ClassPrefix_PREPEND(SUUpdateDriver)
#define SUBasicUpdateDriver ClassPrefix_PREPEND(SUBasicUpdateDriver)
#define SUUIBasedUpdateDriver ClassPrefix_PREPEND(SUUIBasedUpdateDriver)
#define SUAutomaticUpdateDriver ClassPrefix_PREPEND(SUAutomaticUpdateDriver)
#define SUScheduledUpdateDriver ClassPrefix_PREPEND(SUScheduledUpdateDriver)
#define SUProbingUpdateDriver ClassPrefix_PREPEND(SUProbingUpdateDriver)
#define SUUserInitiatedUpdateDriver ClassPrefix_PREPEND(SUUserInitiatedUpdateDriver)
//Support
#define SUDSAVerifier ClassPrefix_PREPEND(SUDSAVerifier)
#define SUPasswordPrompt ClassPrefix_PREPEND(SUPasswordPrompt)

//Appcast Support
#define SUAppcast ClassPrefix_PREPEND(SUAppcast)
#define SUAppcastExtensions ClassPrefix_PREPEND(SUAppcastExtensions)
#define SUAppcastItem ClassPrefix_PREPEND(SUAppcastItem)
#define SUSystemProfiler ClassPrefix_PREPEND(SUSystemProfiler)
#define SUVersionComparison ClassPrefix_PREPEND(SUVersionComparison)
#define SUStandardVersionComparator ClassPrefix_PREPEND(SUStandardVersionComparator)

//Installation
#define SUInstaller ClassPrefix_PREPEND(SUInstaller)
#define SUPlainInstaller ClassPrefix_PREPEND(SUPlainInstaller)
#define SUPlainInstallerInternals ClassPrefix_PREPEND(SUPlainInstallerInternals)
#define SUPackageInstaller ClassPrefix_PREPEND(SUPackageInstaller)


//Unarchiving
//Binary Delta
#define SUBinaryDeltaUnarchiver ClassPrefix_PREPEND(SUBinaryDeltaUnarchiver)
#define SUBinaryDeltaTool ClassPrefix_PREPEND(SUBinaryDeltaTool)

#define SUUnarchiver ClassPrefix_PREPEND(SUUnarchiver)
#define SUPipedUnarchiver ClassPrefix_PREPEND(SUPipedUnarchiver)
#define SUDiskImageUnarchiver ClassPrefix_PREPEND(SUDiskImageUnarchiver)
#define NTSynchronousTask ClassPrefix_PREPEND(NTSynchronousTask)


//User Interface
#define SUUpdateAlert ClassPrefix_PREPEND(SUUpdateAlert)
#define SUAutomaticUpdateAlert ClassPrefix_PREPEND(SUAutomaticUpdateAlert)
#define SUStatusController ClassPrefix_PREPEND(SUStatusController)
#define SUUpdatePermissionPrompt ClassPrefix_PREPEND(SUUpdatePermissionPrompt)
#define SUWindowController ClassPrefix_PREPEND(SUWindowController)

//Code Signing
#define SUCodeSigningVerifier ClassPrefix_PREPEND(SUCodeSigningVerifier)


//Other Sources
#define SUHost ClassPrefix_PREPEND(SUHost)

#endif



#endif
