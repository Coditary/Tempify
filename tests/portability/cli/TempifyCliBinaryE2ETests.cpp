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
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyConfigHome;
using tempify::test_support::ScopedTempifyDataHome;

} // namespace

TEST_CASE(TempifyCliBinary_help_and_version_exit_zero) {
    const ProcessResult help = run_cli({"--help"});
    REQUIRE_EQ(help.exit_code, 0);
    REQUIRE(help.stdout_text.find("Usage:") != std::string::npos);
    REQUIRE(help.stdout_text.find("Commands:") != std::string::npos);

    const ProcessResult version = run_cli({"--version"});
    REQUIRE_EQ(version.exit_code, 0);
    REQUIRE(version.stdout_text.find("v0.") != std::string::npos);
}

TEST_CASE(TempifyCliBinary_list_reports_fixture_templates) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-cli-binary-list-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-cli-binary-list-data-home");
    prepare_template_workspace(workspace.path());

    const ProcessResult result = run_cli({"list"}, workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("basic_cpp") != std::string::npos);
    REQUIRE(result.stdout_text.find("advanced_hooks_layout") != std::string::npos);
}

TEST_CASE(TempifyCliBinary_render_basic_cpp_creates_expected_files) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-cli-binary-render-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-cli-binary-render-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-cli-binary-render-data-home");
    prepare_template_workspace(workspace.path());

    const ProcessResult result = run_cli({
                                       "basic_cpp",
                                       target.path().string(),
                                       "--set",
                                       "project_name=Binary E2E",
                                       "--set",
                                       "name_slug=binary-e2e",
                                       "--set",
                                       "namespace=binary_e2e_ns",
                                       "--set",
                                       "include_ci=false",
                                       "--set",
                                       "author=Binary Tester",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(std::filesystem::is_regular_file(target.path() / "README.md"));
    REQUIRE(read_text_file(target.path() / "README.md").find("# Binary E2E") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(target.path() / ".tempify-lock.json"));
}

TEST_CASE(TempifyCliBinary_doctor_runs_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-cli-binary-doctor-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-cli-binary-doctor-data-home");
    prepare_template_workspace(workspace.path());

    const ProcessResult result = run_cli({"doctor"}, workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("Doctor") != std::string::npos || result.stdout_text.find("doctor") != std::string::npos);
}

TEST_CASE(TempifyCliBinary_render_unknown_template_fails_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-cli-binary-missing-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-cli-binary-missing-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-cli-binary-missing-data-home");
    prepare_template_workspace(workspace.path());

    const ProcessResult result = run_cli({"definitely_missing_template_12345", target.path().string()}, workspace.path(),
                                         isolated_cli_env(data_home.path()));
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Template not found") != std::string::npos
            || combined.find("not found") != std::string::npos);
}

TEST_CASE(TempifyCliBinary_render_existing_target_without_overwrite_fails_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-cli-binary-conflict-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-cli-binary-conflict-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-cli-binary-conflict-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    const std::vector<std::string> render_args = {
        "basic_cpp",
        target.path().string(),
        "--set",
        "project_name=Binary Conflict",
        "--set",
        "name_slug=binary-conflict",
        "--set",
        "namespace=binary_conflict_ns",
        "--set",
        "include_ci=false",
        "--set",
        "author=Binary Tester",
    };
    REQUIRE_EQ(run_cli(render_args, workspace.path(), env).exit_code, 0);

    const ProcessResult second_render = run_cli(render_args, workspace.path(), env);
    REQUIRE(second_render.exit_code != 0);
}
