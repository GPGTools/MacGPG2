PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2
UPDATER_PROJECT = MacGPG2_Updater
UPDATER_TARGET = MacGPG2_Updater

include Dependencies/GPGTools_Core/newBuildSystem/Makefile.default

$(PRODUCT):
	@./build.sh

updater:
	@xcodebuild -project $(UPDATER_PROJECT).xcodeproj -target $(UPDATER_TARGET) -configuration $(CONFIG) build $(XCCONFIG)

clean-updater:
	@xcodebuild -project $(UPDATER_PROJECT).xcodeproj -target $(UPDATER_TARGET) -configuration $(CONFIG) clean > /dev/null

