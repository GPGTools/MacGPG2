all: compile

compile:
	@./build-script.sh

install:
	@./build-script.sh --install

clean:
	rm -fr build/

