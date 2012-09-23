all: compile

update:
	@git submodule foreach git pull origin master
	@git pull

compile:
	@./build-script.sh

install:
	@./build-script.sh --install

clean:
	rm -fr build/

dmg:
	./Dependencies/GPGTools_Core/scripts/create_dmg.sh

