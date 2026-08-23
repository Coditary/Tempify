#include "CliProcess.h"
#include "E2ETestSupport.h"
#include "TestHarness.h"

#include "parser/JsonParser.h"
#include "TempifyTestSupport.h"

#include <future>
#include <string>
#include <vector>

namespace {

using tempify::test::ProcessResult;
using tempify::test::run_cli;
using tempify::test_support::create_shared_template;
using tempify::test_support::e2e::isolated_cli_env;
using tempify::test_support::e2e::prepare_template_workspace;
using tempify::test_support::e2e::slug_to_namespace;
using tempify::test_support::inject_stale_managed_file;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;

std::size_t count_successes(const std::vector<ProcessResult> &results) {
    std::size_t count = 0;
    for (const ProcessResult &result : results) {
        if (result.exit_code == 0) {
            ++count;
        }
    }
    return count;
}

void require_parseable_json_file(const std::filesystem::path &path) {
    prebyte::JsonParser parser;
    const prebyte::Data data = parser.parse(path);
    REQUIRE(data.is_map());
}

std::size_t count_indexed_templates(const std::filesystem::path &index_file) {
    prebyte::JsonParser parser;
    const prebyte::Data data = parser.parse(index_file);
    REQUIRE(data.is_map());
    const auto &root = data.as_map();
    const auto it = root.find("templates");
    REQUIRE(it != root.end());
    REQUIRE(it->second.is_array());
    return it->second.as_array().size();
}

void require_no_stale_index_temp_files(const std::filesystem::path &index_dir) {
    if (!std::filesystem::is_directory(index_dir)) {
        return;
    }
    for (const auto &entry : std::filesystem::directory_iterator(index_dir)) {
        REQUIRE(entry.path().extension() != ".tmp");
    }
}

std::vector<std::string> basic_cpp_render_args(const std::filesystem::path &target, const std::string &slug,
                                               const bool overwrite = false) {
    std::vector<std::string> args = {
        "basic_cpp",
        target.string(),
        "--set",
        "project_name=Shared State " + slug,
        "--set",
        "name_slug=" + slug,
        "--set",
        "namespace=" + slug_to_namespace(slug),
        "--set",
        "include_ci=false",
        "--set",
        "author=Shared State Tester",
    };
    if (overwrite) {
        args.push_back("--overwrite-if-exists");
    }
    return args;
}

void seed_shared_templates(const std::filesystem::path &shared_root) {
    create_shared_template(shared_root, "shared_alpha", "Shared Alpha", "1.0.0", "Alpha template");
    create_shared_template(shared_root, "shared_beta", "Shared Beta", "1.0.0", "Beta template");
    create_shared_template(shared_root, "shared_gamma", "Shared Gamma", "1.0.0", "Gamma template");
}

} // namespace

TEST_CASE(TempifySharedStateConcurrencyE2E_parallel_refresh_keeps_shared_index_consistent) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-shared-state-refresh-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-state-refresh-data-home");
    seed_shared_templates(data_home.shared_root());
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();

    constexpr int parallel_count = 6;
    std::vector<std::future<ProcessResult>> futures;
    futures.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        futures.push_back(std::async(std::launch::async, [workspace_path, env, use_json = index % 2 == 0]() {
            if (use_json) {
                return run_cli({"refresh", "--json"}, workspace_path, env);
            }
            return run_cli({"refresh"}, workspace_path, env);
        }));
    }

    std::vector<ProcessResult> results;
    results.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        results.push_back(futures[static_cast<std::size_t>(index)].get());
    }

    REQUIRE(count_successes(results) >= 1U);

    const ProcessResult stabilize = run_cli({"refresh"}, workspace_path, env);
    REQUIRE_EQ(stabilize.exit_code, 0);

    const std::filesystem::path index_file = data_home.shared_root() / "index" / "templates.json";
    REQUIRE(std::filesystem::is_regular_file(index_file));
    require_parseable_json_file(index_file);
    REQUIRE_EQ(count_indexed_templates(index_file), 3U);
    require_no_stale_index_temp_files(index_file.parent_path());

    const ProcessResult list = run_cli({"list"}, workspace_path, env);
    REQUIRE_EQ(list.exit_code, 0);
    REQUIRE(list.stdout_text.find("shared_alpha") != std::string::npos);
    REQUIRE(list.stdout_text.find("shared_beta") != std::string::npos);
    REQUIRE(list.stdout_text.find("shared_gamma") != std::string::npos);
    static_cast<void>(results);
}

TEST_CASE(TempifySharedStateConcurrencyE2E_refresh_list_and_doctor_share_data_home_without_corruption) {
    ScopedDirectoryCleanup workspace(
        std::filesystem::temp_directory_path() / "tempify-shared-state-catalog-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-state-catalog-data-home");
    seed_shared_templates(data_home.shared_root());
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();

    REQUIRE_EQ(run_cli({"refresh"}, workspace_path, env).exit_code, 0);

    auto refresh_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"refresh", "--json"}, workspace_path, env);
    });
    auto doctor_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"doctor"}, workspace_path, env);
    });
    auto info_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"info", "shared_beta"}, workspace_path, env);
    });
    auto list_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"list", "--json"}, workspace_path, env);
    });

    const ProcessResult refresh = refresh_future.get();
    const ProcessResult doctor = doctor_future.get();
    const ProcessResult info = info_future.get();
    const ProcessResult list = list_future.get();

    REQUIRE_EQ(refresh.exit_code, 0);
    REQUIRE_EQ(doctor.exit_code, 0);
    REQUIRE_EQ(info.exit_code, 0);
    REQUIRE_EQ(list.exit_code, 0);
    REQUIRE(info.stdout_text.find("shared_beta") != std::string::npos);

    const ProcessResult stabilized_list = run_cli({"list", "--json"}, workspace_path, env);
    REQUIRE_EQ(stabilized_list.exit_code, 0);
    REQUIRE(stabilized_list.stdout_text.find("\"shared_alpha\"") != std::string::npos);
    REQUIRE(stabilized_list.stdout_text.find("\"shared_beta\"") != std::string::npos);

    const std::filesystem::path index_file = data_home.shared_root() / "index" / "templates.json";
    require_parseable_json_file(index_file);
    require_no_stale_index_temp_files(index_file.parent_path());
}

TEST_CASE(TempifySharedStateConcurrencyE2E_concurrent_renders_to_same_target_leave_valid_lock) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-shared-state-render-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-shared-state-render-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-state-render-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();
    const std::filesystem::path &target_path = target.path();

    constexpr int parallel_count = 4;
    std::vector<std::future<ProcessResult>> futures;
    futures.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        futures.push_back(std::async(std::launch::async, [workspace_path, env, target_path]() {
            return run_cli(basic_cpp_render_args(target_path, "same-target", true), workspace_path, env);
        }));
    }

    std::vector<ProcessResult> results;
    results.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        results.push_back(futures[static_cast<std::size_t>(index)].get());
    }

    REQUIRE(count_successes(results) >= 1U);
    REQUIRE(std::filesystem::is_regular_file(target_path / "README.md"));
    REQUIRE(std::filesystem::is_regular_file(target_path / ".tempify-lock.json"));

    const std::string lock_text = read_text_file(target_path / ".tempify-lock.json");
    require_parseable_json_file(target_path / ".tempify-lock.json");
    REQUIRE(lock_text.find("\"basic_cpp\"") != std::string::npos);
    REQUIRE(read_text_file(target_path / "README.md").find("# Shared State same-target") != std::string::npos);
}

TEST_CASE(TempifySharedStateConcurrencyE2E_concurrent_reapply_on_same_target_deletes_stale_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-shared-state-reapply-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-shared-state-reapply-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-state-reapply-data-home");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();
    const std::filesystem::path &target_path = target.path();

    REQUIRE_EQ(run_cli(basic_cpp_render_args(target_path, "reapply-target"), workspace_path, env).exit_code, 0);
    inject_stale_managed_file(target_path, "old-managed.txt", "removed-by-template-update\n");
    REQUIRE(std::filesystem::is_regular_file(target_path / "old-managed.txt"));

    constexpr int parallel_count = 4;
    std::vector<std::future<ProcessResult>> futures;
    futures.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        futures.push_back(std::async(std::launch::async, [workspace_path, env, target_path]() {
            std::vector<std::string> args = basic_cpp_render_args(target_path, "reapply-target");
            args.push_back("--reapply");
            return run_cli(args, workspace_path, env);
        }));
    }

    std::vector<ProcessResult> results;
    results.reserve(parallel_count);
    for (int index = 0; index < parallel_count; ++index) {
        results.push_back(futures[static_cast<std::size_t>(index)].get());
    }

    REQUIRE(count_successes(results) >= 1U);
    REQUIRE(!std::filesystem::exists(target_path / "old-managed.txt"));
    require_parseable_json_file(target_path / ".tempify-lock.json");
    REQUIRE(read_text_file(target_path / ".tempify-lock.json").find("old-managed.txt") == std::string::npos);
}

TEST_CASE(TempifySharedStateConcurrencyE2E_render_from_shared_store_while_refresh_runs) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-shared-state-mixed-workspace");
    ScopedDirectoryCleanup output(std::filesystem::temp_directory_path() / "tempify-shared-state-mixed-output");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-state-mixed-data-home");
    create_shared_template(data_home.shared_root(), "shared_render", "Shared Render", "1.0.0", "Shared render template",
                           "Rendered from shared store.");
    prepare_template_workspace(workspace.path());
    const auto env = isolated_cli_env(data_home.path());
    const std::filesystem::path &workspace_path = workspace.path();
    const std::filesystem::path &output_path = output.path();

    REQUIRE_EQ(run_cli({"refresh"}, workspace_path, env).exit_code, 0);

    auto refresh_future = std::async(std::launch::async, [workspace_path, env]() {
        return run_cli({"refresh"}, workspace_path, env);
    });
    auto render_future = std::async(std::launch::async, [workspace_path, env, output_path]() {
        return run_cli({
                           "shared_render",
                           output_path.string(),
                           "--set",
                           "project_name=Concurrent Shared",
                           "--set",
                           "project_slug=concurrent-shared",
                       },
                       workspace_path, env);
    });

    const ProcessResult refresh = refresh_future.get();
    const ProcessResult render = render_future.get();

    REQUIRE_EQ(refresh.exit_code, 0);
    REQUIRE_EQ(render.exit_code, 0);
    const std::filesystem::path readme_path = output_path / "README.md";
    REQUIRE(std::filesystem::is_regular_file(readme_path));
    REQUIRE(read_text_file(readme_path).find("Rendered from shared store") != std::string::npos);

    const ProcessResult list = run_cli({"list", "--json"}, workspace_path, env);
    REQUIRE_EQ(list.exit_code, 0);
    REQUIRE(list.stdout_text.find("\"shared_render\"") != std::string::npos);
    require_parseable_json_file(data_home.shared_root() / "index" / "templates.json");
}
