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
using tempify::test_support::inject_stale_managed_file;
using tempify::test_support::inject_stale_managed_file_without_hash;
using tempify::test_support::read_text_file;
using tempify::test_support::write_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;

std::vector<std::string> basic_cpp_render_args(const std::filesystem::path &target, const std::string &slug) {
    return {
        "basic_cpp",
        target.string(),
        "--set",
        "project_name=Reapply Delete " + slug,
        "--set",
        "name_slug=" + slug,
        "--set",
        "namespace=" + slug_to_namespace(slug),
        "--set",
        "include_ci=false",
        "--set",
        "author=Reapply Delete Tester",
    };
}

} // namespace

TEST_CASE(TempifyReapplyDeleteE2E_subprocess_deletes_removed_managed_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");
    REQUIRE(std::filesystem::is_regular_file(target.path() / "old-managed.txt"));

    std::vector<std::string> reapply_args = basic_cpp_render_args(target.path(), "delete-e2e");
    reapply_args.push_back("--reapply");
    const ProcessResult reapply = run_cli(reapply_args, workspace.path(), env);
    REQUIRE_EQ(reapply.exit_code, 0);
    REQUIRE(reapply.stdout_text.find("Deleted: 1") != std::string::npos
            || reapply.stdout_text.find("\"delete\": 1") != std::string::npos);
    REQUIRE(!std::filesystem::exists(target.path() / "old-managed.txt"));
    REQUIRE(read_text_file(target.path() / ".tempify-lock.json").find("old-managed.txt") == std::string::npos);
}

TEST_CASE(TempifyReapplyDeleteE2E_reapply_subcommand_deletes_removed_managed_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-subcommand-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-subcommand-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-subcommand-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-subcommand"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");

    const ProcessResult reapply = run_cli({
                                            "reapply",
                                            "basic_cpp",
                                            target.path().string(),
                                            "--set",
                                            "project_name=Reapply Delete delete-subcommand",
                                            "--set",
                                            "name_slug=delete-subcommand",
                                            "--set",
                                            "namespace=delete_subcommand_ns",
                                            "--set",
                                            "include_ci=false",
                                            "--set",
                                            "author=Reapply Delete Tester",
                                        },
                                        workspace.path(), env);
    REQUIRE_EQ(reapply.exit_code, 0);
    REQUIRE(!std::filesystem::exists(target.path() / "old-managed.txt"));
    REQUIRE(reapply.stdout_text.find("Deleted: 1") != std::string::npos
            || reapply.stdout_text.find("Reapplied basic_cpp") != std::string::npos);
}

TEST_CASE(TempifyReapplyDeleteE2E_subprocess_blocks_delete_when_stale_file_was_locally_modified) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-conflict-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-conflict-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-conflict-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-conflict-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");
    write_text_file(target.path() / "old-managed.txt", "user-edited-stale-content\n");

    std::vector<std::string> reapply_args = basic_cpp_render_args(target.path(), "delete-conflict-e2e");
    reapply_args.push_back("--reapply");
    const ProcessResult reapply = run_cli(reapply_args, workspace.path(), env);
    REQUIRE(reapply.exit_code != 0);
    const std::string combined = reapply.stdout_text + reapply.stderr_text;
    REQUIRE(combined.find("conflict item") != std::string::npos || combined.find("REAPPLY_BLOCKED") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(target.path() / "old-managed.txt"));
    REQUIRE(read_text_file(target.path() / "old-managed.txt") == "user-edited-stale-content\n");
}

TEST_CASE(TempifyReapplyDeleteE2E_subprocess_report_shows_delete_without_removing_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-report-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-report-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-report-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-report-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");
    const std::string stale_content = read_text_file(target.path() / "old-managed.txt");

    std::vector<std::string> report_args = basic_cpp_render_args(target.path(), "delete-report-e2e");
    report_args.push_back("--reapply");
    report_args.push_back("--report");
    const ProcessResult report = run_cli(report_args, workspace.path(), env);
    REQUIRE_EQ(report.exit_code, 0);
    REQUIRE(report.stdout_text.find("delete  old-managed.txt") != std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "old-managed.txt"), stale_content);
}

TEST_CASE(TempifyReapplyDeleteE2E_diff_only_leaves_stale_file_on_disk) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-diff-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-diff-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-diff-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-diff-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");
    const std::string stale_content = read_text_file(target.path() / "old-managed.txt");

    std::vector<std::string> diff_args = basic_cpp_render_args(target.path(), "delete-diff-e2e");
    diff_args.push_back("--diff");
    const ProcessResult diff = run_cli(diff_args, workspace.path(), env);
    REQUIRE_EQ(diff.exit_code, 0);
    REQUIRE(diff.stdout_text.find("delete  old-managed.txt") != std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "old-managed.txt"), stale_content);
}

TEST_CASE(TempifyReapplyDeleteE2E_subprocess_blocks_delete_when_lock_has_no_baseline_hash) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-review-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-review-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-review-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-review-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file_without_hash(target.path(), "old-managed.txt", "removed-by-template-update\n");

    std::vector<std::string> reapply_args = basic_cpp_render_args(target.path(), "delete-review-e2e");
    reapply_args.push_back("--reapply");
    const ProcessResult reapply = run_cli(reapply_args, workspace.path(), env);
    REQUIRE(reapply.exit_code != 0);
    const std::string combined = reapply.stdout_text + reapply.stderr_text;
    REQUIRE(combined.find("review item") != std::string::npos || combined.find("REAPPLY_BLOCKED") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(target.path() / "old-managed.txt"));
}

TEST_CASE(TempifyReapplyDeleteE2E_json_blocks_delete_conflict_without_removing_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-json-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-json-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-json-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-json-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");
    write_text_file(target.path() / "old-managed.txt", "user-edited-stale-content\n");

    std::vector<std::string> reapply_args = basic_cpp_render_args(target.path(), "delete-json-e2e");
    reapply_args.push_back("--reapply");
    reapply_args.push_back("--json");
    const ProcessResult reapply = run_cli(reapply_args, workspace.path(), env);
    REQUIRE(reapply.exit_code != 0);
    const std::string combined = reapply.stdout_text + reapply.stderr_text;
    REQUIRE(combined.find("REAPPLY_BLOCKED") != std::string::npos);
    REQUIRE(combined.find("old-managed.txt") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "old-managed.txt") == "user-edited-stale-content\n");
}

TEST_CASE(TempifyReapplyDeleteE2E_reapply_subcommand_blocks_delete_conflict) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-reapply-delete-sub-conflict-e2e-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-reapply-delete-sub-conflict-e2e-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-reapply-delete-sub-conflict-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target.path(), "delete-sub-conflict-e2e"), workspace.path(), env).exit_code, 0);
    inject_stale_managed_file(target.path(), "old-managed.txt", "removed-by-template-update\n");
    write_text_file(target.path() / "old-managed.txt", "user-edited-stale-content\n");

    const ProcessResult reapply = run_cli({
                                            "reapply",
                                            "basic_cpp",
                                            target.path().string(),
                                            "--set",
                                            "project_name=Reapply Delete delete-sub-conflict-e2e",
                                            "--set",
                                            "name_slug=delete-sub-conflict-e2e",
                                            "--set",
                                            "namespace=delete_sub_conflict_e2e_ns",
                                            "--set",
                                            "include_ci=false",
                                            "--set",
                                            "author=Reapply Delete Tester",
                                            "--json",
                                        },
                                        workspace.path(), env);
    REQUIRE(reapply.exit_code != 0);
    const std::string combined = reapply.stdout_text + reapply.stderr_text;
    REQUIRE(combined.find("old-managed.txt") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(target.path() / "old-managed.txt"));
}
