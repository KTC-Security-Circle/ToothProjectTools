# Makefile

.PHONY: build clean

build:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j

clean:
	rm -rf build

SERVE_APP_TEST_SCRIPT := scripts/serve_app_flow_test.sh
SERVE_APP_BIN ?= ./build/src/serve/tooth-backend
LEFT_CAMERA ?= 0
RIGHT_CAMERA ?= 2
MJPEG_HOST ?= 127.0.0.1
MJPEG_PORT ?= 39010
OUT_DIR ?= ./data/serve_app_test
TIMEOUT_SEC ?= 5
ENABLE_WINDOW_TEST ?= 0

serve_app_test:
	@chmod +x $(SERVE_APP_TEST_SCRIPT)
	@BIN=$(SERVE_APP_BIN) \
	LEFT_CAMERA=$(LEFT_CAMERA) \
	RIGHT_CAMERA=$(RIGHT_CAMERA) \
	MJPEG_HOST=$(MJPEG_HOST) \
	MJPEG_PORT=$(MJPEG_PORT) \
	OUT_DIR=$(OUT_DIR) \
	TIMEOUT_SEC=$(TIMEOUT_SEC) \
	ENABLE_WINDOW_TEST=$(ENABLE_WINDOW_TEST) \
	$(SERVE_APP_TEST_SCRIPT)
