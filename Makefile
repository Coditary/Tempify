.PHONY: configure build start run test clean analyze lint security static-analysis format format-check coverage

BUILD_DIR ?= build
CLANG_TIDY_BUILD_DIR := build-cmake/tidy
COVERAGE_BUILD_DIR := build-cmake/coverage
COVERAGE_MIN_LINE ?= 85

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

coverage:
	cmake --preset coverage $(CMAKE_VCPKG_ARGS)
	cmake --build --preset coverage-tests --parallel
	ctest --preset coverage
	chmod +x scripts/ci/generate_coverage_report.sh
	COVERAGE_MIN_LINE=$(COVERAGE_MIN_LINE) ./scripts/ci/generate_coverage_report.sh

clean:
	rm -rf $(BUILD_DIR) build-cmake
