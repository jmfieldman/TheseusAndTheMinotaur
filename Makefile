.PHONY: run build configure clean

run: build
	build/theseus

build: build/Makefile
	cmake --build build

build/Makefile:
	cmake -B build

configure:
	cmake -B build

clean:
	rm -rf build
