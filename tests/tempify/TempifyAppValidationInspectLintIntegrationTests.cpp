#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::create_bad_layout_template;
using tempify::test_support::create_duplicate_alias_template;
using tempify::test_support::create_sensitive_template;

}

TEST_CASE(TempifyApp_validate_accepts_valid_template) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-valid-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"validate", "basic_cpp"}), 0);
    REQUIRE(capture.str().find("Validated basic_cpp -> OK") != std::string::npos);
}

TEST_CASE(TempifyApp_validate_json_outputs_machine_readable_result) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"validate", "basic_cpp", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"ok\"") != std::string::npos);
}

TEST_CASE(TempifyApp_validate_rejects_duplicate_alias_conflict) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-validate-alias-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-alias-data-home");
    const std::filesystem::path template_path = create_duplicate_alias_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"validate", template_path.string()}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_validate_rejects_missing_layout_source) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-validate-layout-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-layout-data-home");
    const std::filesystem::path template_path = create_bad_layout_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"validate", template_path.string()}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_inspect_outputs_merge_and_provenance_details) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-inspect-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"inspect", "layered_cpp_product"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("layered_cpp_product (Layered C++ Product)") != std::string::npos);
    REQUIRE(output.find("Source Roots:") != std::string::npos);
    REQUIRE(output.find("layered_cpp_base") != std::string::npos);
    REQUIRE(output.find("layered_ci_overlay") != std::string::npos);
    REQUIRE(output.find("src/main.cpp.pbt <- layered_cpp_product") != std::string::npos);
    REQUIRE(output.find("README.md.pbt <- layered_ci_overlay") != std::string::npos);
    REQUIRE(output.find("Questions:") != std::string::npos);
}

TEST_CASE(TempifyApp_inspect_json_outputs_machine_readable_report) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-inspect-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"inspect", "layered_cpp_product", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"layered_cpp_product\"") != std::string::npos);
    REQUIRE(output.find("\"source_roots\": [") != std::string::npos);
    REQUIRE(output.find("\"include_ids\": [") != std::string::npos);
    REQUIRE(output.find("\"files\": [") != std::string::npos);
    REQUIRE(output.find("\"questions\": [") != std::string::npos);
    REQUIRE(output.find("\"layout_rules\": [") != std::string::npos);
    REQUIRE(output.find("\"hooks\": {") != std::string::npos);
}

TEST_CASE(TempifyApp_inspect_json_includes_sensitive_question_metadata) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-sensitive-inspect-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-sensitive-inspect-data-home");
    const std::filesystem::path template_root = create_sensitive_template(workspace.path());
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"inspect", template_root.string(), "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"api_token\"") != std::string::npos);
    REQUIRE(output.find("\"sensitive\": true") != std::string::npos);
}

TEST_CASE(TempifyApp_lint_outputs_authoring_warnings) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lint-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"lint", "layered_cpp_product"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Lint layered_cpp_product") != std::string::npos);
    REQUIRE(output.find("Warnings:") != std::string::npos);
    REQUIRE(output.find("question missing prompt") != std::string::npos);
}

TEST_CASE(TempifyApp_lint_json_outputs_machine_readable_report) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lint-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"lint", "layered_cpp_product", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"layered_cpp_product\"") != std::string::npos);
    REQUIRE(output.find("\"warning_count\": ") != std::string::npos);
    REQUIRE(output.find("\"warnings\": [") != std::string::npos);
    REQUIRE(output.find("question missing prompt") != std::string::npos);
}

TEST_CASE(TempifyApp_lint_still_fails_invalid_template_structure) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-lint-invalid-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lint-invalid-data-home");
    const std::filesystem::path template_path = create_duplicate_alias_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"lint", template_path.string()}), tempify::TempifyError);
}
