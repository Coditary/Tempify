#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include "TempifyTestSupport.h"

#include <filesystem>
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

std::vector<std::string> basic_cpp_render_args(const std::filesystem::path &target, const std::string &slug) {
    return {
        "basic_cpp",
        target.string(),
        "--set",
        "project_name=Corrupt " + slug,
        "--set",
        "name_slug=" + slug,
        "--set",
        "namespace=" + slug_to_namespace(slug),
        "--set",
        "include_ci=false",
        "--set",
        "author=Corrupt Input Tester",
    };
}

} // namespace

TEST_CASE(TempifyCorruptInputE2E_subprocess_rejects_corrupt_lock_on_diff) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-diff-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-diff-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-diff-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "corrupt-diff"), workspace.path(), env).exit_code, 0);
    write_text_file(target.path() / ".tempify-lock.json", "{ broken lock file\n");

    std::vector<std::string> diff_args = basic_cpp_render_args(target.path(), "corrupt-diff");
    diff_args.push_back("--diff");
    const ProcessResult diff = run_cli(diff_args, workspace.path(), env);
    REQUIRE(diff.exit_code != 0);
    const std::string combined = diff.stdout_text + diff.stderr_text;
    REQUIRE(combined.find("Could not parse lock file") != std::string::npos);
}

TEST_CASE(TempifyCorruptInputE2E_subprocess_rejects_corrupt_lock_on_reapply) {
    ScopedDirectoryCleanup workspace(
        std::filesystem::temp_directory_path() / "tempify-corrupt-lock-reapply-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-reapply-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-reapply-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "corrupt-reapply"), workspace.path(), env).exit_code, 0);
    write_text_file(target.path() / ".tempify-lock.json", "{ broken lock file\n");

    std::vector<std::string> reapply_args = basic_cpp_render_args(target.path(), "corrupt-reapply");
    reapply_args.push_back("--reapply");
    const ProcessResult reapply = run_cli(reapply_args, workspace.path(), env);
    REQUIRE(reapply.exit_code != 0);
    const std::string combined = reapply.stdout_text + reapply.stderr_text;
    REQUIRE(combined.find("Could not parse lock file") != std::string::npos);
}

TEST_CASE(TempifyCorruptInputE2E_subprocess_rejects_corrupt_answers_file) {
    ScopedDirectoryCleanup workspace(
        std::filesystem::temp_directory_path() / "tempify-corrupt-answers-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-e2e-target");
    ScopedDirectoryCleanup answers_root(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-e2e-root");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path answers_file = answers_root.path() / "broken-answers.json";
    write_text_file(answers_file, "{ broken answers\n");

    std::vector<std::string> render_args = basic_cpp_render_args(target.path(), "corrupt-answers");
    render_args.push_back("--answers");
    render_args.push_back(answers_file.string());
    render_args.push_back("--set");
    render_args.push_back("author=Corrupt Input Tester");
    render_args.push_back("--non-interactive");

    const ProcessResult render = run_cli(render_args, workspace.path(), env);
    REQUIRE(render.exit_code != 0);
    const std::string combined = render.stdout_text + render.stderr_text;
    REQUIRE(combined.find("Could not parse answer file") != std::string::npos);
}

TEST_CASE(TempifyCorruptInputE2E_subprocess_rejects_missing_answers_file) {
    ScopedDirectoryCleanup workspace(
        std::filesystem::temp_directory_path() / "tempify-corrupt-answers-missing-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-missing-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-missing-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path answers_file =
        std::filesystem::temp_directory_path() / "tempify-corrupt-answers-missing-e2e.json";
    std::filesystem::remove(answers_file);

    std::vector<std::string> render_args = basic_cpp_render_args(target.path(), "missing-answers");
    render_args.push_back("--answers");
    render_args.push_back(answers_file.string());
    render_args.push_back("--non-interactive");

    const ProcessResult render = run_cli(render_args, workspace.path(), env);
    REQUIRE(render.exit_code != 0);
    const std::string combined = render.stdout_text + render.stderr_text;
    REQUIRE(combined.find("Answer file not found") != std::string::npos);
}

TEST_CASE(TempifyCorruptInputE2E_subprocess_rejects_lock_with_invalid_managed_files_type) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-type-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-type-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-type-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "corrupt-lock-type"), workspace.path(), env).exit_code, 0);
    write_text_file(target.path() / ".tempify-lock.json", "{\n  \"managed_files\": \"not-an-array\"\n}\n");

    std::vector<std::string> diff_args = basic_cpp_render_args(target.path(), "corrupt-lock-type");
    diff_args.push_back("--diff");
    const ProcessResult diff = run_cli(diff_args, workspace.path(), env);
    REQUIRE(diff.exit_code != 0);
    const std::string combined = diff.stdout_text + diff.stderr_text;
    REQUIRE(combined.find("managed_files") != std::string::npos);
    REQUIRE(combined.find("must be array") != std::string::npos);
}
