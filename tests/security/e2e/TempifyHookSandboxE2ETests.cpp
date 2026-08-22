#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include "TempifyTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test::ProcessResult;
using tempify::test::run_cli;
using tempify::test_support::e2e::isolated_cli_env;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::write_text_file;

void create_minimal_hook_template(const std::filesystem::path &template_root, const std::string &template_id,
                                  const std::string &post_hook_body) {
    std::filesystem::create_directories(template_root / "files");
    std::filesystem::create_directories(template_root / "hooks");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"" +
                        template_id +
                        "\",\n"
                        "  name = \"Hook Sandbox Demo\",\n"
                        "  version = \"1.0.0\",\n"
                        "  source_dir = \"files\",\n"
                        "  output = { path = \"{{ project_name }}\", overwrite = false },\n"
                        "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"project_name\", type = \"string\", prompt = \"Project name\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n");
    write_text_file(template_root / "hooks" / "post.lua", post_hook_body);
}

} // namespace

TEST_CASE(TempifyHookSandboxE2E_render_aborts_when_post_hook_writes_outside_build_root) {
    ScopedDirectoryCleanup sandbox_root(std::filesystem::temp_directory_path() / "tempify-hook-sandbox-e2e-sandbox");
    ScopedDirectoryCleanup workspace(sandbox_root.path() / "workspace");
    ScopedDirectoryCleanup target(sandbox_root.path() / "target");
    ScopedDirectoryCleanup outside(sandbox_root.path() / "outside");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-hook-sandbox-e2e-data-home");
    const std::filesystem::path escape_file = outside.path() / "escaped.txt";
    create_minimal_hook_template(workspace.path() / "escape_hook_demo",
                                 "escape_hook_demo",
                                 "write_file('" + escape_file.string() + "', 'escaped')\n");

    const ProcessResult result = run_cli({
                                       "escape_hook_demo",
                                       target.path().string(),
                                       "--set",
                                       "project_name=Escape App",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Hook path escapes build root") != std::string::npos
            || combined.find("Hook phase 'post' failed") != std::string::npos);
    REQUIRE(!std::filesystem::exists(escape_file));
}

TEST_CASE(TempifyHookSandboxE2E_render_aborts_when_post_hook_uses_relative_traversal) {
    ScopedDirectoryCleanup sandbox_root(
        std::filesystem::temp_directory_path() / "tempify-hook-sandbox-traversal-sandbox");
    ScopedDirectoryCleanup workspace(sandbox_root.path() / "workspace");
    ScopedDirectoryCleanup target(sandbox_root.path() / "target");
    ScopedDirectoryCleanup outside(sandbox_root.path() / "outside");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-hook-sandbox-traversal-data-home");
    const std::filesystem::path escape_file = outside.path() / "traversal-escaped.txt";
    create_minimal_hook_template(workspace.path() / "traversal_hook_demo",
                                 "traversal_hook_demo",
                                 "write_file('../outside/traversal-escaped.txt', 'escaped')\n");

    const ProcessResult result = run_cli({
                                       "traversal_hook_demo",
                                       target.path().string(),
                                       "--set",
                                       "project_name=Traversal App",
                                   },
                                   workspace.path(), isolated_cli_env(data_home.path()));
    REQUIRE(result.exit_code != 0);
    const std::string combined = result.stdout_text + result.stderr_text;
    REQUIRE(combined.find("Hook path escapes") != std::string::npos
            || combined.find("Hook phase 'post' failed") != std::string::npos);
    REQUIRE(!std::filesystem::exists(escape_file));
}
