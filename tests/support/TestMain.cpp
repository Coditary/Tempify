#include "TempifyTestSupport.h"
#include "TestHarness.h"

#include <filesystem>
#include <iostream>
#include <string_view>

int main(int argc, char **argv) {
    tempify::test_support::ensure_test_workspace_layout();
    std::filesystem::current_path(tempify::test_support::test_workspace_root());
    std::filesystem::create_directories(tempify::test_support::test_workspace_root() / ".xdg-data");
    std::filesystem::create_directories(tempify::test_support::test_workspace_root() / ".xdg-config");
    prebyte::test::ScopedEnvironmentVariable default_data_home(
        "XDG_DATA_HOME", (tempify::test_support::test_workspace_root() / ".xdg-data").string());
    prebyte::test::ScopedEnvironmentVariable default_config_home(
        "XDG_CONFIG_HOME", (tempify::test_support::test_workspace_root() / ".xdg-config").string());

    if (argc == 2 && std::string_view(argv[1]) == "--list-tests") {
        for (const std::string &name : prebyte::test::test_names()) {
            std::cout << name << '\n';
        }
        return 0;
    }

    if (argc == 3 && std::string_view(argv[1]) == "--run-test") {
        return prebyte::test::run_test_by_name(argv[2]);
    }

    return prebyte::test::run_all_tests();
}
