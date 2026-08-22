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
