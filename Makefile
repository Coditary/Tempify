.PHONY: configure build start run test clean analyze lint security static-analysis format format-check coverage sanitize tsan msan fuzz fuzz-regression

BUILD_DIR ?= build
CLANG_TIDY_BUILD_DIR := build-cmake/tidy
COVERAGE_BUILD_DIR := build-cmake/coverage
COVERAGE_MIN_LINE ?= 85
TEMPIFY_FUZZ_MAX_TOTAL_TIME ?= 60

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
	chmod +x scripts/ci/bootstrap_vcpkg_deps.sh scripts/ci/generate_coverage_report.sh
	env -u VCPKG_ROOT -u VCPKG_TARGET_TRIPLET ./scripts/ci/bootstrap_vcpkg_deps.sh coverage
	set -a && . build-cmake/.vcpkg-coverage.env && set +a && \
	cmake --preset coverage \
		-DCMAKE_TOOLCHAIN_FILE="$$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
		-DVCPKG_TARGET_TRIPLET="$$VCPKG_TARGET_TRIPLET" && \
	cmake --build --preset coverage-tests --parallel && \
	ctest --preset coverage -j1 && \
	COVERAGE_MIN_LINE=$(COVERAGE_MIN_LINE) ./scripts/ci/generate_coverage_report.sh

sanitize:
	chmod +x scripts/ci/run_sanitize_tests.sh
	./scripts/ci/run_sanitize_tests.sh asan $(CMAKE_VCPKG_ARGS)

tsan:
	chmod +x scripts/ci/run_sanitize_tests.sh
	./scripts/ci/run_sanitize_tests.sh tsan $(CMAKE_VCPKG_ARGS)

msan:
	chmod +x scripts/ci/run_sanitize_tests.sh
	./scripts/ci/run_sanitize_tests.sh msan

fuzz:
	chmod +x scripts/ci/run_fuzzers.sh
	TEMPIFY_FUZZ_MAX_TOTAL_TIME=$(TEMPIFY_FUZZ_MAX_TOTAL_TIME) ./scripts/ci/run_fuzzers.sh $(CMAKE_VCPKG_ARGS)

fuzz-regression:
	chmod +x scripts/ci/run_fuzz_regression.sh
	./scripts/ci/run_fuzz_regression.sh

clean:
	rm -rf $(BUILD_DIR) build-cmake
