PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2
UPDATER_PROJECT = MacGPG2_Updater
UPDATER_TARGET = MacGPG2_Updater
UPDATER_PRODUCT = MacGPG2_Updater.app
INSTALLED_UPDATER = build/MacGPG2/libexec/$(UPDATER_PRODUCT)
INSTALLED_FIX = build/MacGPG2/libexec/fixGpgHome
GET_THE_SOURCE = build/MacGPG2/SOURCE
SKS_CA_CERT = build/MacGPG2/share/sks-keyservers.netCA.pem
MAKE_DEFAULT = Dependencies/GPGTools_Core/newBuildSystem/Makefile.default
NEED_LIBMACGPG = 1


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

$(INSTALLED_FIX):
	rm -rf $(INSTALLED_FIX)
	cp -R fixGpgHome/fixGpgHome $(INSTALLED_FIX)

$(GET_THE_SOURCE):
	cp -f "installer/Payload/How to get the source code" "$(GET_THE_SOURCE)"
	
$(SKS_CA_CERT):
	cp -f "installer/Payload/sks-keyservers.netCA.pem" "$(SKS_CA_CERT)"


pkg-core: $(INSTALLED_UPDATER) $(INSTALLED_FIX) $(GET_THE_SOURCE) $(SKS_CA_CERT)

install: pkg
	open -b com.apple.installer build/MacGPG2.pkg
	

