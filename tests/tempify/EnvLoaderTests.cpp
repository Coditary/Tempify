#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/support/EnvLoader.h"

#include <string>

namespace {

using tempify::test_support::write_text_file;

} // namespace

TEST_CASE(EnvLoader_ignores_comments_and_blank_lines) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-env-loader-comments.env";
    write_text_file(path,
                    "# comment\n"
                    "\n"
                    "PROJECT_NAME=Demo App\n"
                    "AUTHOR='Quoted Author'\n");

    const std::map<std::string, std::string> values = tempify::load_env_file(path);
    REQUIRE_EQ(values.at("PROJECT_NAME"), std::string("Demo App"));
    REQUIRE_EQ(values.at("AUTHOR"), std::string("Quoted Author"));
}

TEST_CASE(EnvLoader_returns_empty_map_for_missing_file) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-env-loader-missing.env";
    std::filesystem::remove(path);
    REQUIRE(tempify::load_env_file(path).empty());
}

TEST_CASE(EnvLoader_skips_lines_without_separator) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-env-loader-invalid.env";
    write_text_file(path,
                    "VALID=yes\n"
                    "INVALID_LINE\n"
                    "OTHER=value\n");

    const std::map<std::string, std::string> values = tempify::load_env_file(path);
    REQUIRE_EQ(values.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(values.at("VALID"), std::string("yes"));
    REQUIRE_EQ(values.at("OTHER"), std::string("value"));
}
