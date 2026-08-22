#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include <string>

namespace {

using tempify::test::ProcessResult;
using tempify::test::run_cli;
using tempify::test_support::e2e::isolated_cli_env;
using tempify::test_support::e2e::prepare_template_workspace;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;

} // namespace

TEST_CASE(TempifyPrebytePassthroughE2E_short_and_long_aliases_run_embedded_prebyte) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-prebyte-passthrough-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-prebyte-passthrough-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path workspace_path = workspace.path();

    const ProcessResult short_version = run_cli({"-p", "--version"}, workspace_path, env);
    REQUIRE_EQ(short_version.exit_code, 0);
    REQUIRE(short_version.stdout_text.find("v1.") != std::string::npos);

    const ProcessResult long_version = run_cli({"--prebyte", "--version"}, workspace_path, env);
    REQUIRE_EQ(long_version.exit_code, 0);
    REQUIRE(long_version.stdout_text.find("v1.") != std::string::npos);

    const ProcessResult process_version = run_cli({"process", "--version"}, workspace_path, env);
    REQUIRE_EQ(process_version.exit_code, 0);
    REQUIRE(process_version.stdout_text.find("v1.") != std::string::npos);
}

TEST_CASE(TempifyPrebytePassthroughE2E_process_help_matches_tempify_wrapper) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-prebyte-help-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-prebyte-help-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path workspace_path = workspace.path();

    const ProcessResult short_help = run_cli({"-p", "-h"}, workspace_path, env);
    REQUIRE_EQ(short_help.exit_code, 0);
    REQUIRE(short_help.stdout_text.find("Embedded Prebyte Passthrough") != std::string::npos);

    const ProcessResult process_help = run_cli({"process", "--help"}, workspace_path, env);
    REQUIRE_EQ(process_help.exit_code, 0);
    REQUIRE(process_help.stdout_text.find("Embedded Prebyte Passthrough") != std::string::npos);
}
