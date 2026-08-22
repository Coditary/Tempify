#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test::ProcessResult;
using tempify::test::run_cli;
using tempify::test_support::create_sensitive_template;
using tempify::test_support::create_slow_hook_template;
using tempify::test_support::e2e::isolated_cli_env;
using tempify::test_support::e2e::prepare_template_workspace;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::test_template_path;

} // namespace

TEST_CASE(TempifySecurityE2E_sensitive_lock_values_are_redacted_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-security-sensitive-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-security-sensitive-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-security-sensitive-data-home");
    create_sensitive_template(workspace.path());

    const ProcessResult result = run_cli({
                                       "sensitive_demo",
                                       target.path().string(),
                                       "--set",
                                       "project_name=Secret App",
                                       "--set",
                                       "api_token=super-secret-token",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE_EQ(result.exit_code, 0);

    const std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    REQUIRE(lock_text.find("super-secret-token") == std::string::npos);
    REQUIRE(lock_text.find("\"api_token\"") != std::string::npos);
}

TEST_CASE(TempifySecurityE2E_no_hooks_skips_hook_side_effects_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-security-no-hooks-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-security-no-hooks-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-security-no-hooks-data-home");
    prepare_template_workspace(workspace.path());

    const ProcessResult result = run_cli({
                                       "advanced_hooks_layout",
                                       target.path().string(),
                                       "--no-hooks",
                                       "--set",
                                       "project_name=No Hooks",
                                       "--set",
                                       "project_slug=no-hooks",
                                       "--set",
                                       "use_notes=false",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(!std::filesystem::exists(target.path() / "script-marker.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "pre.txt"));
}

TEST_CASE(TempifySecurityE2E_hook_timeout_aborts_slow_hook_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-security-hook-timeout-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-security-hook-timeout-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-security-hook-timeout-data-home");
    create_slow_hook_template(workspace.path());

    const ProcessResult result = run_cli({
                                       "slow_hook_demo",
                                       target.path().string(),
                                       "--set",
                                       "project_name=Timeout App",
                                       "--hook-timeout-ms",
                                       "50",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Hook phase 'post' failed") != std::string::npos
            || combined.find("hook") != std::string::npos);
}

TEST_CASE(TempifySecurityE2E_invalid_namespace_rejected_in_subprocess) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-security-namespace-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-security-namespace-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-security-namespace-data-home");
    prepare_template_workspace(workspace.path());

    const ProcessResult result = run_cli({
                                       "basic_cpp",
                                       target.path().string(),
                                       "--set",
                                       "project_name=Invalid Namespace",
                                       "--set",
                                       "name_slug=invalid-ns",
                                       "--set",
                                       "namespace=invalid-ns",
                                       "--set",
                                       "include_ci=false",
                                       "--set",
                                       "author=Security Tester",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Namespace must be valid C++ identifier") != std::string::npos);
}
