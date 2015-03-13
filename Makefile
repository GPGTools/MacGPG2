PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2
UPDATER_PROJECT = MacGPG2_Updater
UPDATER_TARGET = MacGPG2_Updater
UPDATER_PRODUCT = MacGPG2_Updater.app
INSTALLED_UPDATER = build/MacGPG2/libexec/$(UPDATER_PRODUCT)
SKS_CA_CERT = build/MacGPG2/share/sks-keyservers.netCA.pem

all: $(PRODUCT) $(UPDATER_PRODUCT)
	rm -rf "$(INSTALLED_UPDATER)"
	cp -R "build/Release/$(UPDATER_PRODUCT)" "$(INSTALLED_UPDATER)"

$(PRODUCT):
	./build.sh

clean:
	rm -rf "./build"

$(UPDATER_PRODUCT): MacGPG2_Updater/* MacGPG2_Updater/*/* MacGPG2_Updater.xcodeproj
	xcodebuild -project $(UPDATER_PROJECT).xcodeproj -target $(UPDATER_TARGET) -configuration $(CONFIG) build $(XCCONFIG)
