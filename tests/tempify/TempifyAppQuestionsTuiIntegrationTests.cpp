#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::create_sensitive_template;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdinCapture;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;

} // namespace

TEST_CASE(TempifyApp_render_help_mentions_diff_mode) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-help-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"basic_cpp", "out-dir", "-h"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("--diff") != std::string::npos);
    REQUIRE(output.find("--reapply") != std::string::npos);
    REQUIRE(output.find("tempify reapply <template-id> <target>") != std::string::npos);
    REQUIRE(output.find("--report") != std::string::npos);
    REQUIRE(output.find("does not write files and does not run hooks") != std::string::npos);
    REQUIRE(output.find("applies only ready create/update/delete actions") != std::string::npos);
    REQUIRE(output.find("blocks cross-template .tempify-lock.json reuse") != std::string::npos);
    REQUIRE(output.find("major upgrades, pre-1.0 minor upgrades, downgrades") != std::string::npos);
}

TEST_CASE(TempifyApp_cli_parser_enables_tui_mode_flag) {
    tempify::CliParser parser;
    const tempify::CliRequest request = parser.parse({"basic_cpp", "out-dir", "--tui", "--set", "project_name=App"});

    REQUIRE(request.use_tui);
    REQUIRE_EQ(request.template_ref, std::string("basic_cpp"));
    REQUIRE(request.target_dir.has_value());
}

TEST_CASE(TempifyApp_tui_review_screen_allows_back_before_generate) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-tui-review-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-tui-review-data-home");
    tempify::TempifyApp app;
    ScopedStdinCapture input("First App\n"
                             "\n"
                             "core\n"
                             "no\n"
                             "n\n"
                             ":back\n"
                             "Second App\n"
                             "\n"
                             "core2\n"
                             "no\n"
                             "yes\n");
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"basic_cpp", target.path().string(), "--set", "author=Review Tester", "--tui"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Review values before generate:") != std::string::npos);
    REQUIRE(output.find("Generate with these values? [Y/n]: ") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "README.md").find("# Second App") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "README.md").find("Namespace: core2") != std::string::npos);
}

TEST_CASE(TempifyApp_questions_outputs_minimal_text_overview) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-q-text-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    const int result = app.run({"advanced_hooks_layout", "-q"});

    REQUIRE_EQ(result, 0);
    const std::string output = capture.str();
    REQUIRE(output.find("advanced_hooks_layout (Advanced Hooks Layout)") != std::string::npos);
    REQUIRE(output.find("Groups: 1") != std::string::npos);
    REQUIRE(output.find("Questions: 3") != std::string::npos);
    REQUIRE(output.find("[Project]") != std::string::npos);
    REQUIRE(output.find("- project_name [string]") != std::string::npos);
    REQUIRE(output.find("choices:") == std::string::npos);
}

TEST_CASE(TempifyApp_questions_long_flag_alias_works) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-questions-long-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    const int result = app.run({"advanced_hooks_layout", "--questions"});

    REQUIRE_EQ(result, 0);
    const std::string output = capture.str();
    REQUIRE(output.find("advanced_hooks_layout (Advanced Hooks Layout)") != std::string::npos);
    REQUIRE(output.find("[Project]") != std::string::npos);
}

TEST_CASE(TempifyApp_questions_json_and_full_modes_work) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-q-json-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"advanced_hooks_layout", "-q", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"template\"") != std::string::npos);
        REQUIRE(output.find("\"advanced_hooks_layout\"") != std::string::npos);
        REQUIRE(output.find("\"use_notes\"") != std::string::npos);
        REQUIRE(output.find("\"order\": [\"Project\"]") != std::string::npos);
        REQUIRE(output.find("\"Project\": [") != std::string::npos);
        REQUIRE(output.find("\"choices\"") == std::string::npos);
        REQUIRE(output.find("\"optional\": false") == std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"advanced_hooks_layout", "-q", "--json", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"choices\": []") != std::string::npos);
        REQUIRE(output.find("\"optional\": false") != std::string::npos);
        REQUIRE(output.find("\"help\": \"\"") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"advanced_hooks_layout", "-q", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("[Project]") != std::string::npos);
        REQUIRE(output.find("choices: []") != std::string::npos);
        REQUIRE(output.find("optional: no") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_questions_outputs_sensitive_metadata_for_template_path) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-sensitive-q-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-sensitive-q-data-home");
    const std::filesystem::path template_root = create_sensitive_template(workspace.path());
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({template_root.string(), "-q", "--json", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"api_token\"") != std::string::npos);
        REQUIRE(output.find("\"sensitive\": true") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({template_root.string(), "-q", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("sensitive: yes") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_questions_unknown_template_throws) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-q-missing-data-home");
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"definitely_missing_template_12345", "-q"}), tempify::TempifyError);
}
