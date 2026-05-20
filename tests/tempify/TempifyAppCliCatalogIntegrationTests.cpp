#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::ScopedCurrentPath;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::create_basic_template_at;
using tempify::test_support::write_text_file;

}

TEST_CASE(TempifyApp_refresh_creates_shared_index_without_templates) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-refresh-empty-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"refresh"}), 0);
    REQUIRE(capture.str().find("Refreshed 0 shared templates") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(data_home.shared_root() / "index" / "templates.json"));
}

TEST_CASE(TempifyApp_help_version_and_list_write_expected_stdout) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-help-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("Usage:") != std::string::npos);
        REQUIRE(output.find("Options:") != std::string::npos);
        REQUIRE(output.find("Commands:") != std::string::npos);
        REQUIRE(output.find("Examples:") != std::string::npos);
        REQUIRE(output.find("tempify <command> [args...] [options]") != std::string::npos);
        REQUIRE(output.find("tempify <template-id> [target] [options]") != std::string::npos);
        REQUIRE(output.find("tempify reapply <template-id> <target> [options]") != std::string::npos);
        REQUIRE(output.find("-q") == std::string::npos);
        REQUIRE(output.find("--questions") == std::string::npos);
        REQUIRE(output.find("process") != std::string::npos);
        REQUIRE(output.find("info") != std::string::npos);
        REQUIRE(output.find("doctor") != std::string::npos);
        REQUIRE(output.find("completion") != std::string::npos);
        REQUIRE(output.find("validate") != std::string::npos);
        REQUIRE(output.find("inspect") != std::string::npos);
        REQUIRE(output.find("lint") != std::string::npos);
        REQUIRE(output.find("test") != std::string::npos);
        REQUIRE(output.find("Passes remaining args to embedded Prebyte") != std::string::npos);
        REQUIRE(output.find("tempify reapply basic_cpp my-app --report --json") != std::string::npos);
        REQUIRE(output.find("-q       Shows question overview for template") == std::string::npos);
        REQUIRE(output.find("Run 'tempify <command> -h' for command-specific help.") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"--version"}), 0);
        REQUIRE(capture.str().find("0.1.0") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("basic_cpp") != std::string::npos);
        REQUIRE(output.find("advanced_hooks_layout") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_help_and_version_aliases_work) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-help-alias-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"help"}), 0);
        REQUIRE(capture.str().find("Usage:") != std::string::npos);
    }
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"-h"}), 0);
        REQUIRE(capture.str().find("Usage:") != std::string::npos);
    }
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"version"}), 0);
        REQUIRE(capture.str().find("0.1.0") != std::string::npos);
    }
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"-v"}), 0);
        REQUIRE(capture.str().find("0.1.0") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_command_and_render_help_pages_work) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-command-help-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify list - List Available Templates") != std::string::npos);
        REQUIRE(output.find("workspace templates") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"info", "basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify info - Show Template Details") != std::string::npos);
        REQUIRE(output.find("tempify info <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"doctor", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify doctor - Inspect Tempify Environment") != std::string::npos);
        REQUIRE(output.find("tempify doctor") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify completion - Generate Shell Completion Script") != std::string::npos);
        REQUIRE(output.find("tempify completion <shell>") != std::string::npos);
        REQUIRE(output.find("bash, zsh, fish") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"validate", "basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify validate - Validate Template Structure") != std::string::npos);
        REQUIRE(output.find("tempify validate <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"inspect", "layered_cpp_product", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify inspect - Inspect Merged Template Graph") != std::string::npos);
        REQUIRE(output.find("tempify inspect <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"lint", "layered_cpp_product", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify lint - Lint Template Quality") != std::string::npos);
        REQUIRE(output.find("tempify lint <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"test", "basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify test - Run Template Fixtures") != std::string::npos);
        REQUIRE(output.find("tempify test <template-id>") != std::string::npos);
        REQUIRE(output.find("answers.json") != std::string::npos);
        REQUIRE(output.find("lock.json") != std::string::npos);
        REQUIRE(output.find("--list-fixtures") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
        REQUIRE(output.find("snapshot/") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"refresh", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify refresh - Rebuild Shared Template Index") != std::string::npos);
        REQUIRE(output.find("index/templates.json") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"process", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify process - Embedded Prebyte Passthrough") != std::string::npos);
        REQUIRE(output.find("tempify process [prebyte-args...]") != std::string::npos);
        REQUIRE(output.find("tempify -p [prebyte-args...]") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "-q", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify -q|--questions - Show Template Questions") != std::string::npos);
        REQUIRE(output.find("tempify <template-id> -q|--questions [options]") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
        REQUIRE(output.find("--full") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "--questions", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify -q|--questions - Show Template Questions") != std::string::npos);
        REQUIRE(output.find("tempify <template-id> -q|--questions [options]") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify render - Generate Template Output") != std::string::npos);
        REQUIRE(output.find("Template:") != std::string::npos);
        REQUIRE(output.find("basic_cpp") != std::string::npos);
        REQUIRE(output.find("-q,--questions") != std::string::npos);
        REQUIRE(output.find("instead of rendering") != std::string::npos);
        REQUIRE(output.find("-f,--overwrite-if-exists") != std::string::npos);
        REQUIRE(output.find("-s,--skip-if-file-exists") != std::string::npos);
        REQUIRE(output.find("--accept-hooks <yes|ask|no>") != std::string::npos);
        REQUIRE(output.find("--no-hooks") != std::string::npos);
        REQUIRE(output.find("--set <key=value>") != std::string::npos);
        REQUIRE(output.find("--answers <path>") != std::string::npos);
        REQUIRE(output.find("--write-answers <path>") != std::string::npos);
        REQUIRE(output.find("--non-interactive") != std::string::npos);
        REQUIRE(output.find("--strict") != std::string::npos);
        REQUIRE(output.find("tempify basic_cpp existing-dir --reapply --json") != std::string::npos);
        REQUIRE(output.find("--dry-run") != std::string::npos);
        REQUIRE(output.find("--plan-json") != std::string::npos);
        REQUIRE(output.find("--tui") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "out-dir", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify <template-id> [target] [options]") != std::string::npos);
        REQUIRE(output.find("Output directory") != std::string::npos);
        REQUIRE(output.find("Generated basic_cpp") == std::string::npos);
        REQUIRE(output.find("tempify reapply basic_cpp existing-dir --json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"reapply", "basic_cpp", "out-dir", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify render - Generate Template Output") != std::string::npos);
        REQUIRE(output.find("Template:") != std::string::npos);
        REQUIRE(output.find("basic_cpp") != std::string::npos);
        REQUIRE(output.find("tempify reapply <template-id> <target>") != std::string::npos);
    }

    REQUIRE_THROWS_AS(app.run({"basic_cpp", "out-dir", "-q"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(app.run({"basic_cpp", "out-dir", "--questions"}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_info_outputs_template_details) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-info-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"info", "basic_cpp"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("basic_cpp (Basic C++ App)") != std::string::npos);
    REQUIRE(output.find("Description: Small C++ starter built through Tempify and Prebyte") != std::string::npos);
    REQUIRE(output.find("Includes: 1 (base_cpp_common)") != std::string::npos);
    REQUIRE(output.find("Questions: 6") != std::string::npos);
}

TEST_CASE(TempifyApp_list_json_outputs_machine_readable_catalog) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-list-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"list", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"total\": ") != std::string::npos);
    REQUIRE(output.find("\"templates\": [") != std::string::npos);
    REQUIRE(output.find("\"id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"id\": \"advanced_hooks_layout\"") != std::string::npos);
}

TEST_CASE(TempifyApp_info_json_outputs_machine_readable_details) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-info-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"info", "basic_cpp", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"name\": \"Basic C++ App\"") != std::string::npos);
    REQUIRE(output.find("\"include_ids\": [") != std::string::npos);
    REQUIRE(output.find("\"base_cpp_common\"") != std::string::npos);
    REQUIRE(output.find("\"question_count\": 6") != std::string::npos);
    REQUIRE(output.find("\"hooks\": {") != std::string::npos);
}

TEST_CASE(TempifyApp_completion_outputs_shell_scripts) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-completion-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "bash"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("complete -F _tempify tempify") != std::string::npos);
        REQUIRE(output.find("tempify list 2>/dev/null | cut -f1") != std::string::npos);
        REQUIRE(output.find("reapply") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "zsh"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("#compdef tempify") != std::string::npos);
        REQUIRE(output.find("compdef _tempify tempify") != std::string::npos);
        REQUIRE(output.find("reapply") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "fish"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("complete -c tempify -f") != std::string::npos);
        REQUIRE(output.find("__fish_use_subcommand") != std::string::npos);
        REQUIRE(output.find("reapply") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_broken_workspace_template_gets_skipped_while_valid_template_remains_visible) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-broken-workspace-skip");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-broken-workspace-skip-data-home");
    std::filesystem::create_directories(workspace.path() / "templates");
    static_cast<void>(create_basic_template_at(workspace.path() / "templates" / "visible_workspace",
                                               "visible_workspace",
                                               "Visible Workspace",
                                               "1.0.0",
                                               "Visible desc",
                                               "VISIBLE WORKSPACE\n"));
    std::filesystem::create_directories(workspace.path() / "templates" / "broken_workspace");
    write_text_file(workspace.path() / "templates" / "broken_workspace" / "questions.lua", "return {}\n");
    ScopedCurrentPath cwd(workspace.path());

    tempify::TempifyApp app;
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"id\": \"visible_workspace\"") != std::string::npos);
        REQUIRE(output.find("broken_workspace") == std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"info", "visible_workspace"}), 0);
        REQUIRE(capture.str().find("Visible Workspace") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_duplicate_workspace_template_ids_are_rejected) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-duplicate-workspace-id");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-duplicate-workspace-id-data-home");
    std::filesystem::create_directories(workspace.path() / "templates");
    static_cast<void>(create_basic_template_at(workspace.path() / "templates" / "alpha",
                                               "dup_workspace",
                                               "Alpha",
                                               "1.0.0",
                                               "Alpha desc"));
    static_cast<void>(create_basic_template_at(workspace.path() / "templates" / "beta",
                                               "dup_workspace",
                                               "Beta",
                                               "1.0.0",
                                               "Beta desc"));
    ScopedCurrentPath cwd(workspace.path());

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({"list"}), tempify::TempifyError);
}
