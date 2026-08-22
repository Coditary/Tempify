.PHONY: configure build start run test clean analyze lint security static-analysis format format-check

BUILD_DIR ?= build
CLANG_TIDY_BUILD_DIR := build-cmake/tidy

ifdef VCPKG_ROOT
CMAKE_VCPKG_ARGS := -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake
ifdef VCPKG_TARGET_TRIPLET
CMAKE_VCPKG_ARGS += -DVCPKG_TARGET_TRIPLET=$(VCPKG_TARGET_TRIPLET)
endif
endif

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja

build: configure
	cmake --build $(BUILD_DIR)

start: build

run: build
	./$(BUILD_DIR)/tempify

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

analyze:
	chmod +x scripts/ci/run_clang_tidy.sh
	./scripts/ci/run_clang_tidy.sh analyze $(CMAKE_VCPKG_ARGS)

lint:
	chmod +x scripts/ci/run_clang_tidy.sh
	./scripts/ci/run_clang_tidy.sh lint $(CMAKE_VCPKG_ARGS)

security:
	chmod +x scripts/ci/run_clang_tidy.sh
	./scripts/ci/run_clang_tidy.sh security $(CMAKE_VCPKG_ARGS)

static-analysis:
	chmod +x scripts/ci/run_clang_tidy.sh
	./scripts/ci/run_clang_tidy.sh all $(CMAKE_VCPKG_ARGS)

format:
	chmod +x scripts/ci/run_clang_format.sh
	./scripts/ci/run_clang_format.sh format

format-check:
	chmod +x scripts/ci/run_clang_format.sh
	./scripts/ci/run_clang_format.sh check

clean:
	rm -rf $(BUILD_DIR) build-cmake
