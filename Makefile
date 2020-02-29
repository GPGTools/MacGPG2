PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2

all: $(PRODUCT)
	./add_version_plist.sh

$(PRODUCT):
	./create_gpg.sh

clean:
	rm -rf "./build"

install:
	./create_gpg.sh install
