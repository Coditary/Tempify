#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include <array>
#include <future>
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

std::vector<std::string> parallel_render_args(const std::filesystem::path &target, const std::string &slug) {
    return {
        "basic_cpp",
        target.string(),
        "--set",
        "project_name=Parallel " + slug,
        "--set",
        "name_slug=" + slug,
        "--set",
        "namespace=" + slug_to_namespace(slug),
        "--set",
        "include_ci=false",
        "--set",
        "author=Parallel Tester",
    };
}

} // namespace

TEST_CASE(TempifyConcurrencyE2E_parallel_renders_succeed_in_subprocesses) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-concurrency-e2e-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-concurrency-e2e-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());

    constexpr int parallel_count = 4;
    std::array<std::filesystem::path, parallel_count> target_paths;
    std::array<ScopedDirectoryCleanup, parallel_count> targets{{
        ScopedDirectoryCleanup(std::filesystem::temp_directory_path() / "tempify-concurrency-e2e-target-0"),
        ScopedDirectoryCleanup(std::filesystem::temp_directory_path() / "tempify-concurrency-e2e-target-1"),
        ScopedDirectoryCleanup(std::filesystem::temp_directory_path() / "tempify-concurrency-e2e-target-2"),
        ScopedDirectoryCleanup(std::filesystem::temp_directory_path() / "tempify-concurrency-e2e-target-3"),
    }};
    for (int index = 0; index < parallel_count; ++index) {
        target_paths[static_cast<std::size_t>(index)] = targets[static_cast<std::size_t>(index)].path();
    }

    std::vector<std::future<ProcessResult>> futures;
    futures.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        const std::string slug = "parallel-" + std::to_string(index);
        futures.push_back(std::async(std::launch::async, [workspace_path = workspace.path(), env, args = parallel_render_args(target_paths[static_cast<std::size_t>(index)], slug)]() {
            return run_cli(args, workspace_path, env);
        }));
    }

    for (int index = 0; index < parallel_count; ++index) {
        const ProcessResult result = futures[static_cast<std::size_t>(index)].get();
        REQUIRE_EQ(result.exit_code, 0);
        const std::filesystem::path target_path = target_paths[static_cast<std::size_t>(index)];
        REQUIRE(std::filesystem::is_regular_file(target_path / "README.md"));
        REQUIRE(read_text_file(target_path / "README.md").find("# Parallel parallel-") != std::string::npos);
    }
}

TEST_CASE(TempifyConcurrencyE2E_parallel_list_and_inspect_do_not_interfere) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-concurrency-catalog-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-concurrency-catalog-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();

    auto list_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"list"}, workspace_path, env);
    });
    auto inspect_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"inspect", "basic_cpp", "--json"}, workspace_path, env);
    });
    auto doctor_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"doctor"}, workspace_path, env);
    });

    const ProcessResult list = list_future.get();
    const ProcessResult inspect = inspect_future.get();
    const ProcessResult doctor = doctor_future.get();

    REQUIRE_EQ(list.exit_code, 0);
    REQUIRE(list.stdout_text.find("basic_cpp") != std::string::npos);
    REQUIRE_EQ(inspect.exit_code, 0);
    REQUIRE(inspect.stdout_text.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE_EQ(doctor.exit_code, 0);
}

TEST_CASE(TempifyConcurrencyE2E_parallel_renders_to_same_target_without_overwrite_report_failures) {
    ScopedDirectoryCleanup workspace(
        std::filesystem::temp_directory_path() / "tempify-concurrency-same-target-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-concurrency-same-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-concurrency-same-target-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();
    const std::filesystem::path &target_path = target.path();

    constexpr int parallel_count = 4;
    std::vector<std::future<ProcessResult>> futures;
    futures.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        const std::string slug = "same-target-" + std::to_string(index);
        futures.push_back(std::async(std::launch::async, [workspace_path, env, target_path, slug]() {
            return run_cli(parallel_render_args(target_path, slug), workspace_path, env);
        }));
    }

    std::size_t success_count = 0;
    std::size_t failure_count = 0;
    for (int index = 0; index < parallel_count; ++index) {
        const ProcessResult result = futures[static_cast<std::size_t>(index)].get();
        if (result.exit_code == 0) {
            ++success_count;
        } else {
            ++failure_count;
        }
    }

    REQUIRE_EQ(success_count, static_cast<std::size_t>(1));
    REQUIRE_EQ(failure_count, static_cast<std::size_t>(3));
    REQUIRE(std::filesystem::is_regular_file(target_path / "README.md"));
}
