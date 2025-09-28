# Makefile

.PHONY: build clean

build:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j

clean:
	rm -rf build
