PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2
UPDATER_PROJECT = MacGPG2_Updater
UPDATER_TARGET = MacGPG2_Updater
UPDATER_PRODUCT = MacGPG2_Updater.app
INSTALLED_UPDATER = build/MacGPG2/libexec/$(UPDATER_PRODUCT)
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


$(UPDATER_PRODUCT): MacGPG2_Updater/* MacGPG2_Updater/*/* MacGPG2_Updater.xcodeproj
	@xcodebuild -project $(UPDATER_PROJECT).xcodeproj -target $(UPDATER_TARGET) -configuration $(CONFIG) build $(XCCONFIG)

compile: $(UPDATER_PRODUCT)

$(INSTALLED_UPDATER): $(PRODUCT) $(UPDATER_PRODUCT)
	rm -rf $(INSTALLED_UPDATER)
	cp -R build/Release/$(UPDATER_PRODUCT) $(INSTALLED_UPDATER)

pkg-core: $(INSTALLED_UPDATER)

install: $(PRODUCT)
	@echo Installing MacGPG2...
	@[[ $$UID -eq 0 ]] || ( echo "This command needs to be run as root!"; exit 1 )
	@rsync -rltDE build/MacGPG2 /usr/local/
	@[[ ! -h /usr/local/bin/gpg2 ]] || ln -sfh /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg2
	@[[ ! -h /usr/local/bin/gpg-agent ]] || ln -sfh /usr/local/MacGPG2/bin/gpg-agent /usr/local/bin/gpg-agent
	@[[ -f /usr/local/bin/gpg && -x /usr/local/bin/gpg ]] || ln -sfh /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg
	@rsync -rltDE installer/Payload/etc/ /private/etc/
	@mkdir -p /Library/LaunchAgents
	@cp build/org.gpgtools.macgpg2.updater.plist /Library/LaunchAgents/
	@echo Done
	

