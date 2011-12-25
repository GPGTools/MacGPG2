all: compile

update:
	@git submodule foreach git pull origin master
	@git pull

compile:
	@./build.sh

clean:
	@./build.sh clean

pinentry:
	 $(MAKE) -C ./Dependencies/pinentry-mac

dmg: compile pinentry
	./Dependencies/GPGTools_Core/scripts/create_dmg.sh
