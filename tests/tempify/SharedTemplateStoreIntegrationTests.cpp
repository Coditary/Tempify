#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {
using tempify::test_support::create_basic_template_at;
using tempify::test_support::create_shared_template;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;

} // namespace

TEST_CASE(TempifyApp_refresh_indexes_shared_templates_and_renders_them) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-store-render-data-home");
    ScopedDirectoryCleanup output_root(std::filesystem::temp_directory_path() / "tempify-shared-store-render-output");
    create_shared_template(data_home.shared_root(), "shared_cpp", "Shared Template", "1.0.0", "From shared store",
                           "Installed from shared store.");

    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"refresh"}), 0);
        REQUIRE(capture.str().find("Refreshed 1 shared templates") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list"}), 0);
        REQUIRE(capture.str().find("shared_cpp") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"shared_cpp", "-q", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"shared_cpp\"") != std::string::npos);
        REQUIRE(output.find("\"Project\": [") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"shared_cpp", "--questions", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"shared_cpp\"") != std::string::npos);
        REQUIRE(output.find("\"Project\": [") != std::string::npos);
    }

    const std::filesystem::path generated = output_root.path() / "from-shared";
    REQUIRE_EQ(app.run({
                   "shared_cpp",
                   generated.string(),
                   "--set",
                   "project_name=Shared App",
                   "--set",
                   "project_slug=shared-app",
               }),
               0);
    REQUIRE(read_text_file(generated / "README.md").find("Installed from shared store.") != std::string::npos);
}

TEST_CASE(TempifyApp_workspace_template_overrides_shared_template_with_same_id) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-store-override-data-home");
    ScopedDirectoryCleanup output_root(std::filesystem::temp_directory_path() / "tempify-shared-store-override-output");
    create_shared_template(data_home.shared_root(), "basic_cpp", "Shared Basic", "1.0.0", "From shared store",
                           "FROM SHARED STORE");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({"refresh"}), 0);

    const std::filesystem::path generated = output_root.path() / "workspace-wins";
    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   generated.string(),
                   "--set",
                   "project_name=Workspace App",
                   "--set",
                   "name_slug=workspace-app",
                   "--set",
                   "namespace=workspace_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Tester",
               }),
               0);

    const std::string readme = read_text_file(generated / "README.md");
    REQUIRE(readme.find("FROM SHARED STORE") == std::string::npos);
    REQUIRE(readme.find("# Workspace App") != std::string::npos);
}

TEST_CASE(TempifyApp_list_info_and_catalog_prefer_workspace_template_over_shared_template) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-shared-store-catalog-override-data-home");
    create_shared_template(data_home.shared_root(), "basic_cpp", "Shared Basic", "9.9.9", "Shared desc",
                           "FROM SHARED STORE");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({"refresh"}), 0);

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"id\": \"basic_cpp\"") != std::string::npos);
        REQUIRE(output.find("\"name\": \"Basic C++ App\"") != std::string::npos);
        REQUIRE(output.find("\"name\": \"Shared Basic\"") == std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"info", "basic_cpp", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"name\": \"Basic C++ App\"") != std::string::npos);
        REQUIRE(output.find("Shared desc") == std::string::npos);
    }
}

TEST_CASE(TempifyApp_stale_shared_index_entry_missing_on_disk_is_not_listed_and_info_errors_cleanly) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-shared-store-stale-index-data-home");
    create_shared_template(data_home.shared_root(), "ghost_tpl", "Ghost Template", "1.0.0", "Ghost desc");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({"refresh"}), 0);
    std::filesystem::remove_all(data_home.shared_root() / "templates" / "ghost_tpl");

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list", "--json"}), 0);
        REQUIRE(capture.str().find("ghost_tpl") == std::string::npos);
    }

    REQUIRE_THROWS_AS(app.run({"info", "ghost_tpl"}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_refresh_rejects_duplicate_shared_template_ids) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-store-duplicate-data-home");
    static_cast<void>(create_basic_template_at(data_home.shared_root() / "templates" / "dup_tpl_alpha", "dup_tpl",
                                               "Duplicate Alpha", "1.0.0", "Alpha copy"));
    static_cast<void>(create_basic_template_at(data_home.shared_root() / "templates" / "dup_tpl_beta", "dup_tpl",
                                               "Duplicate Beta", "1.0.0", "Beta copy"));

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({"refresh"}), tempify::TempifyError);
}
