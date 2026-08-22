#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include <map>
#include <string>
#include <vector>

namespace {

using tempify::test::ProcessResult;
using tempify::test::run_cli;
using tempify::test_support::e2e::isolated_cli_env;
using tempify::test_support::e2e::prepare_template_workspace;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;

void require_removed_command_subprocess(const std::vector<std::string> &args, const std::string &command_name,
                                        const std::string &migration_hint, const std::filesystem::path &workspace,
                                        const std::map<std::string, std::string> &env) {
    const ProcessResult result = run_cli(args, workspace, env);
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Command `" + command_name + "` removed") != std::string::npos);
    REQUIRE(combined.find(migration_hint) != std::string::npos);
}

} // namespace

TEST_CASE(TempifyRemovedCommandsE2E_subprocess_reports_migration_errors) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-removed-commands-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-removed-commands-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path workspace_path = workspace.path();

    require_removed_command_subprocess({"template", "list"}, "template", "tempify list", workspace_path, env);
    require_removed_command_subprocess({"prebyte", "render"}, "prebyte", "tempify -p", workspace_path, env);
    require_removed_command_subprocess({"schema", "basic_cpp"}, "schema", "-q|--questions", workspace_path, env);
    require_removed_command_subprocess({"questions", "basic_cpp"}, "questions", "-q|--questions", workspace_path, env);
    require_removed_command_subprocess({"registry", "refresh"}, "registry", "tempify refresh", workspace_path, env);
}
