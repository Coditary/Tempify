#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::test_template_path;

} // namespace

TEST_CASE(TempifyApp_render_with_cli_values_generates_expected_output) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-cli-values-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-cli-values-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "basic_cpp",
        target.path().string(),
        "--set",
        "project_name=CLI App",
        "--set",
        "name_slug=alias-slug",
        "--set",
        "namespace=cli_ns",
        "--set",
        "include_ci=false",
        "--set",
        "ci_provider=gitlab",
        "--set",
        "docs_url=https://docs.example",
        "--set",
        "author=Leodoras",
    });

    REQUIRE_EQ(result, 0);

    const std::string readme = read_text_file(target.path() / "README.md");
    const std::string cmake = read_text_file(target.path() / "CMakeLists.txt");
    const std::string summary = read_text_file(target.path() / ".tempify-summary.txt");

    REQUIRE(readme.find("# CLI App") != std::string::npos);
    REQUIRE(readme.find("- Namespace: cli_ns") != std::string::npos);
    REQUIRE(readme.find("- CI: disabled") != std::string::npos);
    REQUIRE(readme.find("- Docs:") == std::string::npos);
    REQUIRE(cmake.find("project(alias-slug") != std::string::npos);
    REQUIRE(summary.find("slug=alias-slug") != std::string::npos);
    REQUIRE(summary.find("provider=\n") != std::string::npos);
    REQUIRE(summary.find("docs=\n") != std::string::npos);
}

TEST_CASE(TempifyApp_render_inherited_template_merges_layers_and_drop_paths) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-layered-cpp-product-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-layered-cpp-product-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "layered_cpp_product",
        target.path().string(),
        "--set",
        "project_name=Layered Product",
        "--set",
        "project_slug=layered-product",
        "--set",
        "language_standard=c++23",
        "--set",
        "include_ci=true",
        "--set",
        "ci_provider=github",
    });

    REQUIRE_EQ(result, 0);
    REQUIRE(std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(std::filesystem::exists(target.path() / "src" / "main.cpp"));
    REQUIRE(std::filesystem::exists(target.path() / ".github" / "workflows" / "ci.yml"));
    REQUIRE(!std::filesystem::exists(target.path() / "base-only.txt"));

    const std::string readme = read_text_file(target.path() / "README.md");
    const std::string main_cpp = read_text_file(target.path() / "src" / "main.cpp");
    REQUIRE(readme.find("Layer: ci") != std::string::npos);
    REQUIRE(readme.find("CI: github") != std::string::npos);
    REQUIRE(main_cpp.find("product Layered Product c++23") != std::string::npos);
}

TEST_CASE(TempifyApp_render_with_direct_template_path_generates_expected_output) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-path-render-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-path-render-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        test_template_path("basic_cpp").string(),
        target.path().string(),
        "--set",
        "project_name=Path App",
        "--set",
        "name_slug=path-app",
        "--set",
        "namespace=path_ns",
        "--set",
        "include_ci=false",
        "--set",
        "author=Path Tester",
    });

    REQUIRE_EQ(result, 0);
    REQUIRE(read_text_file(target.path() / "README.md").find("# Path App") != std::string::npos);
    REQUIRE(read_text_file(target.path() / ".tempify-summary.txt").find("slug=path-app") != std::string::npos);
}

TEST_CASE(TempifyApp_overwrite_if_exists_replaces_conflicting_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-overwrite-if-exists-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-overwrite-if-exists-data-home");
    std::filesystem::create_directories(target.path());
    tempify::test_support::write_text_file(target.path() / "README.md", "old readme\n");
    tempify::test_support::write_text_file(target.path() / "keep.txt", "keep\n");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   target.path().string(),
                   "-f",
                   "--set",
                   "project_name=Overwrite App",
                   "--set",
                   "name_slug=overwrite-app",
                   "--set",
                   "namespace=overwrite_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Overwrite Tester",
               }),
               0);

    REQUIRE(read_text_file(target.path() / "README.md").find("# Overwrite App") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "keep.txt") == std::string("keep\n"));
}

TEST_CASE(TempifyApp_skip_if_file_exists_preserves_conflicting_files_and_generates_missing_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-skip-if-file-exists-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-skip-if-file-exists-data-home");
    std::filesystem::create_directories(target.path());
    tempify::test_support::write_text_file(target.path() / "README.md", "custom readme\n");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   target.path().string(),
                   "-s",
                   "--set",
                   "project_name=Skip App",
                   "--set",
                   "name_slug=skip-app",
                   "--set",
                   "namespace=skip_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Skip Tester",
               }),
               0);

    REQUIRE(read_text_file(target.path() / "README.md") == std::string("custom readme\n"));
    REQUIRE(std::filesystem::exists(target.path() / "src" / "main.cpp"));
}

TEST_CASE(TempifyApp_render_unknown_template_throws) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-render-missing-data-home");
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"definitely_missing_template_67890", "out-dir"}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_render_existing_target_without_overwrite_throws) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-existing-target-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-existing-target-data-home");
    std::filesystem::create_directories(target.path());
    tempify::test_support::write_text_file(target.path() / "keep.txt", "keep\n");

    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({
                          "basic_cpp",
                          target.path().string(),
                          "--set",
                          "project_name=Existing Target",
                          "--set",
                          "name_slug=existing-target",
                          "--set",
                          "namespace=existing_ns",
                          "--set",
                          "include_ci=false",
                          "--set",
                          "author=Keeper",
                      }),
                      tempify::TempifyError);
}
