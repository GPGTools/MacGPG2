PROJECT = MacGPG2
TARGET = MacGPG2
PRODUCT = MacGPG2

include Dependencies/GPGTools_Core/newBuildSystem/Makefile.default

$(PRODUCT):
	@./build.sh

