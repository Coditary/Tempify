#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include <filesystem>
#include <string>

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

std::vector<std::string> basic_cpp_render_args(const std::filesystem::path &target, const std::string &slug) {
    return {
        "basic_cpp",
        target.string(),
        "--set",
        "project_name=Workflow " + slug,
        "--set",
        "name_slug=" + slug,
        "--set",
        "namespace=" + slug_to_namespace(slug),
        "--set",
        "include_ci=false",
        "--set",
        "author=Workflow Tester",
    };
}

} // namespace

TEST_CASE(TempifyWorkflowE2E_render_validate_and_inspect_round_trip) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-workflow-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    const ProcessResult render = run_cli(basic_cpp_render_args(target.path(), "workflow-e2e"), workspace.path(), env);
    REQUIRE_EQ(render.exit_code, 0);
    REQUIRE(std::filesystem::is_regular_file(target.path() / ".tempify-lock.json"));

    const ProcessResult validate = run_cli({"validate", "basic_cpp"}, workspace.path(), env);
    REQUIRE_EQ(validate.exit_code, 0);

    const ProcessResult inspect = run_cli({"inspect", "basic_cpp", "--json"}, workspace.path(), env);
    REQUIRE_EQ(inspect.exit_code, 0);
    REQUIRE(inspect.stdout_text.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
}

TEST_CASE(TempifyWorkflowE2E_answers_file_enables_second_render) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-answers-workspace");
    ScopedDirectoryCleanup answers_root(std::filesystem::temp_directory_path() / "tempify-workflow-answers-root");
    ScopedDirectoryCleanup first_target(std::filesystem::temp_directory_path() / "tempify-workflow-answers-first");
    ScopedDirectoryCleanup second_target(std::filesystem::temp_directory_path() / "tempify-workflow-answers-second");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-answers-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path answers_file = answers_root.path() / "answers.json";

    std::vector<std::string> first_args = basic_cpp_render_args(first_target.path(), "answers-first");
    first_args.push_back("--write-answers");
    first_args.push_back(answers_file.string());
    REQUIRE_EQ(run_cli(first_args, workspace.path(), env).exit_code, 0);
    REQUIRE(std::filesystem::is_regular_file(answers_file));

    std::vector<std::string> second_args = {
        "basic_cpp",
        second_target.path().string(),
        "--answers",
        answers_file.string(),
        "--set",
        "author=Workflow Tester",
        "--non-interactive",
        "--strict",
    };
    REQUIRE_EQ(run_cli(second_args, workspace.path(), env).exit_code, 0);
    REQUIRE(read_text_file(second_target.path() / "README.md").find("# Workflow answers-first") != std::string::npos);
}

TEST_CASE(TempifyWorkflowE2E_reapply_updates_safe_files_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-reapply-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-workflow-reapply-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-reapply-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "reapply-e2e"), workspace.path(), env).exit_code, 0);

    write_text_file(target.path() / "README.md", "# Local edit\n");
    const std::filesystem::path summary_path = target.path() / ".tempify-summary.txt";
    REQUIRE(std::filesystem::is_regular_file(summary_path));

    std::vector<std::string> reapply_args = basic_cpp_render_args(target.path(), "reapply-e2e");
    reapply_args.push_back("--reapply");
    const ProcessResult reapply = run_cli(reapply_args, workspace.path(), env);
    REQUIRE_EQ(reapply.exit_code, 0);
    REQUIRE(read_text_file(target.path() / "README.md").find("# Local edit") != std::string::npos);
    REQUIRE(reapply.stdout_text.find("Hooks: skipped") != std::string::npos
            || reapply.stdout_text.find("reapply") != std::string::npos);
}

TEST_CASE(TempifyWorkflowE2E_diff_reports_changes_after_manual_edit) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-diff-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-workflow-diff-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-diff-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "diff-e2e"), workspace.path(), env).exit_code, 0);
    write_text_file(target.path() / "README.md", "# Changed locally\n");

    std::vector<std::string> diff_args = basic_cpp_render_args(target.path(), "diff-e2e");
    diff_args.push_back("--diff");
    const ProcessResult diff = run_cli(diff_args, workspace.path(), env);
    REQUIRE_EQ(diff.exit_code, 0);
    REQUIRE(diff.stdout_text.find("README.md") != std::string::npos || diff.stdout_text.find("Diff ") != std::string::npos);
}

TEST_CASE(TempifyWorkflowE2E_reapply_without_lock_fails_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-reapply-fail-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-workflow-reapply-fail-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-reapply-fail-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    std::filesystem::create_directories(target.path());

    std::vector<std::string> args = basic_cpp_render_args(target.path(), "reapply-fail");
    args.push_back("--reapply");
    const ProcessResult result = run_cli(args, workspace.path(), env);
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find(".tempify-lock.json") != std::string::npos);
}

TEST_CASE(TempifyWorkflowE2E_strict_rejects_unknown_answers_keys_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-strict-fail-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-workflow-strict-fail-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-strict-fail-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path answers_file = workspace.path() / "bad-answers.json";
    write_text_file(answers_file, "{\n  \"project_name\": \"Strict Fail\",\n  \"mystery\": \"value\"\n}\n");

    const ProcessResult result = run_cli({
                                             "basic_cpp",
                                             target.path().string(),
                                             "--answers",
                                             answers_file.string(),
                                             "--set",
                                             "author=Workflow Tester",
                                             "--non-interactive",
                                             "--strict",
                                         },
                                         workspace.path(), env);
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("mystery") != std::string::npos || combined.find("Unknown key") != std::string::npos);
}

TEST_CASE(TempifyWorkflowE2E_render_creates_nested_target_directories) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-workflow-nested-target-workspace");
    ScopedDirectoryCleanup target_root(std::filesystem::temp_directory_path() / "tempify-workflow-nested-target-root");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-workflow-nested-target-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    const std::filesystem::path target = target_root.path() / "deep" / "nested" / "cli-target";
    REQUIRE(!std::filesystem::exists(target.parent_path()));

    const ProcessResult render = run_cli(basic_cpp_render_args(target, "workflow-nested-target"), workspace.path(), env);
    REQUIRE_EQ(render.exit_code, 0);

    REQUIRE(std::filesystem::is_directory(target));
    REQUIRE(std::filesystem::is_directory(target / "src"));
    REQUIRE(std::filesystem::is_regular_file(target / "src" / "main.cpp"));
    REQUIRE(std::filesystem::is_regular_file(target / ".tempify-lock.json"));
}
