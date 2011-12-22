all: compile

update:
	@git submodule foreach git pull origin master
	@git pull

compile:
	@./build.sh clean

clean:
	rm -fr build/

dmg:
	./Dependencies/GPGTools_Core/scripts/create_dmg.sh
