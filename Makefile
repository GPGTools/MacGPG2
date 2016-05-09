PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2
SKS_CA_CERT = build/MacGPG2/share/sks-keyservers.netCA.pem

all: $(PRODUCT) $(UPDATER_PRODUCT)
	./CreateVersionPlist.sh

$(PRODUCT):
	./build.sh

clean:
	rm -rf "./build"
