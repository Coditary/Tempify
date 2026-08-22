#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include "TempifyTestSupport.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {

using tempify::test::ProcessResult;
using tempify::test::run_cli;
using tempify::test_support::e2e::isolated_cli_env;
using tempify::test_support::e2e::prepare_template_workspace;
using tempify::test_support::e2e::slug_to_namespace;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::write_text_file;

struct CommandWorkspace {
    ScopedDirectoryCleanup workspace;
    ScopedTempifyDataHome data_home;
    std::map<std::string, std::string> env;

    explicit CommandWorkspace(const char *label)
        : workspace(std::filesystem::temp_directory_path() / ("tempify-cli-commands-" + std::string(label) + "-workspace")),
          data_home(std::filesystem::temp_directory_path() / ("tempify-cli-commands-" + std::string(label) + "-data-home")) {
        prepare_template_workspace(workspace.path());
        env = isolated_cli_env(data_home.path());
    }

    const std::filesystem::path &cwd() const {
        return workspace.path();
    }
};

std::vector<std::string> basic_cpp_render_args(const std::filesystem::path &target, const std::string &slug) {
    return {
        "basic_cpp",
        target.string(),
        "--set",
        "project_name=CLI " + slug,
        "--set",
        "name_slug=" + slug,
        "--set",
        "namespace=" + slug_to_namespace(slug),
        "--set",
        "include_ci=false",
        "--set",
        "author=CLI Commands Tester",
    };
}

std::vector<std::string> hooks_render_args(const std::filesystem::path &target, const std::string &slug) {
    return {
        "advanced_hooks_layout",
        target.string(),
        "--accept-hooks",
        "no",
        "--set",
        "project_name=CLI " + slug,
        "--set",
        "project_slug=" + slug,
        "--set",
        "use_notes=false",
    };
}

} // namespace

TEST_CASE(TempifyCliCommandsE2E_info_and_list_json_in_subprocess) {
    CommandWorkspace fixture("catalog");
    const ProcessResult info = run_cli({"info", "basic_cpp"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(info.exit_code, 0);
    REQUIRE(info.stdout_text.find("basic_cpp (Basic C++ App)") != std::string::npos);

    const ProcessResult info_json = run_cli({"info", "basic_cpp", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(info_json.exit_code, 0);
    REQUIRE(info_json.stdout_text.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(info_json.stdout_text.find("\"question_count\": 6") != std::string::npos);

    const ProcessResult list_json = run_cli({"list", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(list_json.exit_code, 0);
    REQUIRE(list_json.stdout_text.find("\"templates\": [") != std::string::npos);
    REQUIRE(list_json.stdout_text.find("\"id\": \"basic_cpp\"") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_validate_inspect_and_lint_in_subprocess) {
    CommandWorkspace fixture("validate-pipeline");
    const ProcessResult validate = run_cli({"validate", "basic_cpp"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(validate.exit_code, 0);
    REQUIRE(validate.stdout_text.find("Validated basic_cpp -> OK") != std::string::npos);

    const ProcessResult validate_json = run_cli({"validate", "basic_cpp", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(validate_json.exit_code, 0);
    REQUIRE(validate_json.stdout_text.find("\"status\": \"ok\"") != std::string::npos);

    const ProcessResult inspect_json = run_cli({"inspect", "layered_cpp_product", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(inspect_json.exit_code, 0);
    REQUIRE(inspect_json.stdout_text.find("\"template_id\": \"layered_cpp_product\"") != std::string::npos);
    REQUIRE(inspect_json.stdout_text.find("\"source_roots\": [") != std::string::npos);

    const ProcessResult lint = run_cli({"lint", "layered_cpp_product"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(lint.exit_code, 0);
    REQUIRE(lint.stdout_text.find("Lint layered_cpp_product") != std::string::npos);

    const ProcessResult lint_json = run_cli({"lint", "layered_cpp_product", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(lint_json.exit_code, 0);
    REQUIRE(lint_json.stdout_text.find("\"warning_count\": ") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_test_command_runs_fixtures_in_subprocess) {
    CommandWorkspace fixture("test-command");
    const ProcessResult test_run = run_cli({"test", "basic_cpp"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(test_run.exit_code, 0);
    REQUIRE(test_run.stdout_text.find("Test basic_cpp") != std::string::npos);
    REQUIRE(test_run.stdout_text.find("2/2 fixtures passed") != std::string::npos);

    const ProcessResult test_json = run_cli({"test", "basic_cpp", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(test_json.exit_code, 0);
    REQUIRE(test_json.stdout_text.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(test_json.stdout_text.find("\"fixtures\": [") != std::string::npos);

    const ProcessResult list_fixtures = run_cli({"test", "basic_cpp", "--list-fixtures"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(list_fixtures.exit_code, 0);
    REQUIRE(list_fixtures.stdout_text.find("default_no_ci") != std::string::npos);
    REQUIRE(list_fixtures.stdout_text.find("ci_enabled") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_completion_scripts_for_all_shells_in_subprocess) {
    CommandWorkspace fixture("completion");
    const ProcessResult bash = run_cli({"completion", "bash"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(bash.exit_code, 0);
    REQUIRE(bash.stdout_text.find("complete -F _tempify tempify") != std::string::npos);
    REQUIRE(bash.stdout_text.find("reapply") != std::string::npos);

    const ProcessResult zsh = run_cli({"completion", "zsh"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(zsh.exit_code, 0);
    REQUIRE(zsh.stdout_text.find("#compdef tempify") != std::string::npos);
    REQUIRE(zsh.stdout_text.find("compdef _tempify tempify") != std::string::npos);

    const ProcessResult fish = run_cli({"completion", "fish"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(fish.exit_code, 0);
    REQUIRE(fish.stdout_text.find("complete -c tempify -f") != std::string::npos);
    REQUIRE(fish.stdout_text.find("__fish_use_subcommand") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_refresh_in_subprocess) {
    CommandWorkspace fixture("refresh");
    const ProcessResult refresh = run_cli({"refresh"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(refresh.exit_code, 0);
    REQUIRE(refresh.stdout_text.find("Refreshed ") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(fixture.data_home.shared_root() / "index" / "templates.json"));

    const ProcessResult refresh_json = run_cli({"refresh", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(refresh_json.exit_code, 0);
    REQUIRE(refresh_json.stdout_text.find("\"refreshed\": ") != std::string::npos);
    REQUIRE(refresh_json.stdout_text.find("\"index_file\": ") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_questions_overview_in_subprocess) {
    CommandWorkspace fixture("questions");
    const ProcessResult questions = run_cli({"advanced_hooks_layout", "-q"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(questions.exit_code, 0);
    REQUIRE(questions.stdout_text.find("advanced_hooks_layout (Advanced Hooks Layout)") != std::string::npos);
    REQUIRE(questions.stdout_text.find("[Project]") != std::string::npos);

    const ProcessResult questions_json = run_cli({"advanced_hooks_layout", "--questions", "--json"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(questions_json.exit_code, 0);
    REQUIRE(questions_json.stdout_text.find("\"template\"") != std::string::npos);
    REQUIRE(questions_json.stdout_text.find("\"advanced_hooks_layout\"") != std::string::npos);
    REQUIRE(questions_json.stdout_text.find("\"Project\": [") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_dry_run_and_plan_json_write_no_files_in_subprocess) {
    CommandWorkspace fixture("plan");
    ScopedDirectoryCleanup dry_target(fixture.cwd().parent_path() / "tempify-cli-commands-dry-run-target");
    ScopedDirectoryCleanup plan_target(fixture.cwd().parent_path() / "tempify-cli-commands-plan-json-target");

    std::vector<std::string> dry_args = hooks_render_args(dry_target.path(), "dry-run");
    dry_args.push_back("--dry-run");
    const ProcessResult dry_run = run_cli(dry_args, fixture.cwd(), fixture.env);
    REQUIRE_EQ(dry_run.exit_code, 0);
    REQUIRE(dry_run.stdout_text.find("Build root:") != std::string::npos);
    REQUIRE(!std::filesystem::exists(dry_target.path() / "README.md"));

    std::vector<std::string> plan_args = basic_cpp_render_args(plan_target.path(), "plan-json");
    plan_args.push_back("--plan-json");
    const ProcessResult plan_json = run_cli(plan_args, fixture.cwd(), fixture.env);
    REQUIRE_EQ(plan_json.exit_code, 0);
    REQUIRE(plan_json.stdout_text.find("\"build_root\"") != std::string::npos);
    REQUIRE(plan_json.stdout_text.find("\"files\"") != std::string::npos);
    REQUIRE(!std::filesystem::exists(plan_target.path() / "README.md"));
}

TEST_CASE(TempifyCliCommandsE2E_diff_json_reports_changes_in_subprocess) {
    CommandWorkspace fixture("diff-json");
    ScopedDirectoryCleanup target(fixture.cwd().parent_path() / "tempify-cli-commands-diff-json-target");

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "diff-json"), fixture.cwd(), fixture.env).exit_code, 0);
    write_text_file(target.path() / "README.md", "# changed in subprocess\n");

    std::vector<std::string> diff_args = basic_cpp_render_args(target.path(), "diff-json");
    diff_args.push_back("--diff");
    diff_args.push_back("--json");
    const ProcessResult diff = run_cli(diff_args, fixture.cwd(), fixture.env);
    REQUIRE_EQ(diff.exit_code, 0);
    REQUIRE(diff.stdout_text.find("\"origin\": {") != std::string::npos);
    REQUIRE(diff.stdout_text.find("\"reapply\": {") != std::string::npos);
    REQUIRE(diff.stdout_text.find("README.md") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_reapply_subcommand_succeeds_in_subprocess) {
    CommandWorkspace fixture("reapply");
    ScopedDirectoryCleanup target(fixture.cwd().parent_path() / "tempify-cli-commands-reapply-target");

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "reapply-cmd"), fixture.cwd(), fixture.env).exit_code, 0);
    write_text_file(target.path() / "README.md", "# local readme\n");

    const ProcessResult reapply = run_cli({
                                            "reapply",
                                            "basic_cpp",
                                            target.path().string(),
                                            "--set",
                                            "project_name=CLI reapply-cmd",
                                            "--set",
                                            "name_slug=reapply-cmd",
                                            "--set",
                                            "namespace=reapply_cmd_ns",
                                            "--set",
                                            "include_ci=false",
                                            "--set",
                                            "author=CLI Commands Tester",
                                        },
                                        fixture.cwd(), fixture.env);
    REQUIRE_EQ(reapply.exit_code, 0);
    REQUIRE(reapply.stdout_text.find("Reapplied basic_cpp") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "README.md").find("# local readme") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_prebyte_passthrough_help_in_subprocess) {
    CommandWorkspace fixture("prebyte");
    const ProcessResult prebyte_short = run_cli({"-p", "-h"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(prebyte_short.exit_code, 0);

    const ProcessResult prebyte_process = run_cli({"process", "--help"}, fixture.cwd(), fixture.env);
    REQUIRE_EQ(prebyte_process.exit_code, 0);
    const std::string combined = prebyte_short.stdout_text + prebyte_short.stderr_text + prebyte_process.stdout_text
                                 + prebyte_process.stderr_text;
    REQUIRE(combined.find("Usage:") != std::string::npos || combined.find("usage") != std::string::npos);
}

TEST_CASE(TempifyCliCommandsE2E_validate_missing_template_fails_in_subprocess) {
    CommandWorkspace fixture("validate-missing");
    const ProcessResult result = run_cli({"validate", "definitely_missing_template_12345"}, fixture.cwd(), fixture.env);
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Template not found") != std::string::npos
            || combined.find("not found") != std::string::npos);
}
