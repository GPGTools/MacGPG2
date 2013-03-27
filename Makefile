PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2
UPDATER_PROJECT = MacGPG2_Updater
UPDATER_TARGET = MacGPG2_Updater
MAKE_DEFAULT = Dependencies/GPGTools_Core/newBuildSystem/Makefile.default


-include $(MAKE_DEFAULT)

$(MAKE_DEFAULT):
	@bash -c "$$(curl -fsSL https://raw.github.com/GPGTools/GPGTools_Core/master/newBuildSystem/prepare-core.sh)"

init: $(MAKE_DEFAULT)

$(PRODUCT):
	@./build.sh

updater:
	@xcodebuild -project $(UPDATER_PROJECT).xcodeproj -target $(UPDATER_TARGET) -configuration $(CONFIG) build $(XCCONFIG)

clean-updater:
	@xcodebuild -project $(UPDATER_PROJECT).xcodeproj -target $(UPDATER_TARGET) -configuration $(CONFIG) clean > /dev/null

