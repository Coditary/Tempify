BUILD_DIR ?= build

start:
	cmake -S . -B $(BUILD_DIR) -G Ninja
	cmake --build $(BUILD_DIR)

run: start
	./$(BUILD_DIR)/tempify

test: start
	ctest --test-dir $(BUILD_DIR) --output-on-failure
