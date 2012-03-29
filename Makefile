include Dependencies/GPGTools_Core/make/default

all: compile

update:
	@git submodule foreach git pull origin master
	@git pull

compile:
	@./build.sh

clean:
	@./build.sh clean

dmg: compile
	./Dependencies/GPGTools_Core/scripts/create_dmg.sh
