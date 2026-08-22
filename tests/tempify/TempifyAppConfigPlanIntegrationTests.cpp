#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::create_basic_template_at;
using tempify::test_support::create_required_only_template;
using tempify::test_support::create_sensitive_template;
using tempify::test_support::json_escaped_path;
using tempify::test_support::link_test_templates_into_workspace;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedCurrentPath;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyConfigHome;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::write_text_file;

} // namespace

TEST_CASE(TempifyApp_answers_file_round_trip_supports_non_interactive_render) {
    ScopedDirectoryCleanup answers_root(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-root");
    ScopedDirectoryCleanup first_target(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-first");
    ScopedDirectoryCleanup second_target(std::filesystem::temp_directory_path() /
                                         "tempify-app-answers-roundtrip-second");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-data-home");
    const std::filesystem::path answers_file = answers_root.path() / "answers.json";

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   first_target.path().string(),
                   "--set",
                   "project_name=Round Trip App",
                   "--set",
                   "name_slug=round-trip-app",
                   "--set",
                   "namespace=round_trip_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Round Trip Tester",
                   "--write-answers",
                   answers_file.string(),
               }),
               0);

    const std::string answer_text = read_text_file(answers_file);
    REQUIRE(answer_text.find("project_name") != std::string::npos);
    REQUIRE(answer_text.find("author") == std::string::npos);
    REQUIRE(answer_text.find("name_slug") == std::string::npos);

    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   second_target.path().string(),
                   "--answers",
                   answers_file.string(),
                   "--set",
                   "author=Round Trip Tester",
                   "--non-interactive",
                   "--strict",
               }),
               0);

    REQUIRE(read_text_file(second_target.path() / "README.md").find("# Round Trip App") != std::string::npos);
    REQUIRE(read_text_file(second_target.path() / ".tempify-summary.txt").find("slug=round-trip-app") !=
            std::string::npos);
}

TEST_CASE(TempifyApp_non_interactive_missing_required_answer_throws) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() /
                                         "tempify-app-non-interactive-required-template");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-non-interactive-missing");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-non-interactive-missing-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({
                          template_path.string(),
                          target.path().string(),
                          "--non-interactive",
                          "--strict",
                      }),
                      tempify::TempifyError);
}

TEST_CASE(TempifyApp_strict_rejects_unknown_answers_file_keys) {
    ScopedDirectoryCleanup answers_root(std::filesystem::temp_directory_path() / "tempify-app-answers-strict-root");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-answers-strict-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-answers-strict-data-home");
    const std::filesystem::path answers_file = answers_root.path() / "bad-answers.json";
    write_text_file(answers_file, "{\n  \"project_name\": \"Strict App\",\n  \"mystery\": \"value\"\n}\n");

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({
                          "basic_cpp",
                          target.path().string(),
                          "--answers",
                          answers_file.string(),
                          "--set",
                          "author=Strict Tester",
                          "--non-interactive",
                          "--strict",
                      }),
                      tempify::TempifyError);
}

TEST_CASE(TempifyApp_config_hierarchy_uses_global_and_workspace_defaults_but_answers_and_cli_override_them) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-config-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-config-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-config-data-home");
    ScopedTempifyConfigHome config_home(std::filesystem::temp_directory_path() / "tempify-app-config-config-home");
    const std::filesystem::path answers_file = workspace.path() / "answers.json";

    std::filesystem::create_directories(workspace.path());
    link_test_templates_into_workspace(workspace.path());
    std::filesystem::create_directories(workspace.path() / ".tempify");
    write_text_file(config_home.path() / "tempify" / "config.json", "{\n"
                                                                    "  \"defaults\": {\n"
                                                                    "    \"project_name\": \"Global App\",\n"
                                                                    "    \"namespace\": \"global_ns\",\n"
                                                                    "    \"include_ci\": false,\n"
                                                                    "    \"author\": \"Global Author\"\n"
                                                                    "  },\n"
                                                                    "  \"render\": {\n"
                                                                    "    \"accept_hooks\": \"no\",\n"
                                                                    "    \"hook_timeout_ms\": 1234\n"
                                                                    "  }\n"
                                                                    "}\n");
    write_text_file(workspace.path() / ".tempify" / "config.json", "{\n"
                                                                   "  \"defaults\": {\n"
                                                                   "    \"project_name\": \"Workspace App\",\n"
                                                                   "    \"namespace\": \"workspace_ns\",\n"
                                                                   "    \"author\": \"Workspace Author\"\n"
                                                                   "  },\n"
                                                                   "  \"render\": {\n"
                                                                   "    \"accept_hooks\": \"yes\",\n"
                                                                   "    \"existing_path_behavior\": \"skip\"\n"
                                                                   "  }\n"
                                                                   "}\n");
    write_text_file(answers_file, "{\n"
                                  "  \"project_name\": \"Answer App\",\n"
                                  "  \"namespace\": \"answer_ns\"\n"
                                  "}\n");

    ScopedCurrentPath cwd(workspace.path());
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   target.path().string(),
                   "--answers",
                   answers_file.string(),
                   "--set",
                   "author=CLI Author",
                   "--non-interactive",
                   "--strict",
               }),
               0);

    const std::string readme = read_text_file(target.path() / "README.md");
    const std::string summary = read_text_file(target.path() / ".tempify-summary.txt");
    const std::string lock = read_text_file(target.path() / ".tempify-lock.json");
    REQUIRE(readme.find("# Answer App") != std::string::npos);
    REQUIRE(readme.find("Namespace: answer_ns") != std::string::npos);
    REQUIRE(summary.find("author=CLI Author") != std::string::npos);
    REQUIRE(summary.find("slug=answer-app") != std::string::npos);
    REQUIRE(summary.find("ci=false") != std::string::npos);
    REQUIRE(lock.find("\"existing_path_behavior\": \"skip\"") != std::string::npos);
    REQUIRE(lock.find("\"hook_acceptance\": \"yes\"") != std::string::npos);
    REQUIRE(lock.find("\"hooks_disabled\": false") != std::string::npos);
}

TEST_CASE(TempifyApp_nested_workspace_cwd_uses_nearest_templates_and_config) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-nested-workspace-cwd");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-nested-workspace-cwd-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-nested-workspace-cwd-data-home");
    ScopedTempifyConfigHome config_home(std::filesystem::temp_directory_path() /
                                        "tempify-app-nested-workspace-cwd-config-home");
    const std::filesystem::path outer = workspace.path() / "outer";
    const std::filesystem::path inner = outer / "apps" / "inner";
    const std::filesystem::path nested = inner / "deep" / "src";

    std::filesystem::create_directories(nested);
    static_cast<void>(create_basic_template_at(outer / "templates" / "nested_app", "nested_app", "Outer Template",
                                               "1.0.0", "Outer desc", "OUTER TEMPLATE\n"));
    static_cast<void>(create_basic_template_at(inner / "templates" / "nested_app", "nested_app", "Inner Template",
                                               "1.0.0", "Inner desc", "INNER TEMPLATE\n"));
    write_text_file(outer / ".tempify" / "config.json", "{\n"
                                                        "  \"defaults\": {\n"
                                                        "    \"project_name\": \"Outer Config App\"\n"
                                                        "  }\n"
                                                        "}\n");
    write_text_file(inner / ".tempify" / "config.json", "{\n"
                                                        "  \"defaults\": {\n"
                                                        "    \"project_name\": \"Inner Config App\"\n"
                                                        "  }\n"
                                                        "}\n");

    ScopedCurrentPath cwd(nested);
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
                   "nested_app",
                   target.path().string(),
                   "--non-interactive",
                   "--strict",
               }),
               0);

    const std::string readme = read_text_file(target.path() / "README.md");
    REQUIRE(readme.find("# Inner Config App") != std::string::npos);
    REQUIRE(readme.find("INNER TEMPLATE") != std::string::npos);
    REQUIRE(readme.find("OUTER TEMPLATE") == std::string::npos);
}

TEST_CASE(TempifyApp_dry_run_outputs_plan_and_writes_no_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-dry-run-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-dry-run-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({
                   "advanced_hooks_layout",
                   target.path().string(),
                   "--set",
                   "project_name=Dry Run App",
                   "--set",
                   "project_slug=dry-run-app",
                   "--set",
                   "use_notes=false",
                   "--dry-run",
               }),
               0);

    const std::string output = capture.str();
    REQUIRE(output.find("Build root:") != std::string::npos);
    REQUIRE(output.find("Files:") != std::string::npos);
    REQUIRE(output.find("Hooks:") != std::string::npos);
    REQUIRE(!std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(!std::filesystem::exists(target.path() / ".tempify-lock.json"));
    REQUIRE(!std::filesystem::exists(target.path() / "pre.txt"));
}

TEST_CASE(TempifyApp_plan_json_outputs_json_and_writes_no_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-plan-json-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-plan-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   target.path().string(),
                   "--set",
                   "project_name=Plan App",
                   "--set",
                   "name_slug=plan-app",
                   "--set",
                   "namespace=plan_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Planner",
                   "--plan-json",
               }),
               0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"build_root\"") != std::string::npos);
    REQUIRE(output.find("\"files\"") != std::string::npos);
    REQUIRE(output.find("\"hooks\"") != std::string::npos);
    REQUIRE(!std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(!std::filesystem::exists(target.path() / ".tempify-lock.json"));
}

TEST_CASE(TempifyApp_render_writes_generation_lock_file) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-lock-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lock-data-home");
    tempify::TempifyApp app;

    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   target.path().string(),
                   "--set",
                   "project_name=Lock App",
                   "--set",
                   "name_slug=lock-app",
                   "--set",
                   "namespace=lock_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Locker",
               }),
               0);

    const std::string lock = read_text_file(target.path() / ".tempify-lock.json");
    REQUIRE(lock.find("\"tempify_version\"") != std::string::npos);
    REQUIRE(lock.find("\"basic_cpp\"") != std::string::npos);
    REQUIRE(lock.find("\"lock-app\"") != std::string::npos);
    REQUIRE(lock.find("\"hooks_disabled\"") != std::string::npos);
}

TEST_CASE(TempifyApp_render_redacts_sensitive_values_in_generation_lock) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-sensitive-lock-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-sensitive-lock-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-sensitive-lock-data-home");
    const std::filesystem::path template_root = create_sensitive_template(workspace.path());
    tempify::TempifyApp app;

    REQUIRE_EQ(app.run({
                   template_root.string(),
                   target.path().string(),
                   "--set",
                   "project_name=Secure App",
                   "--set",
                   "api_token=super-secret-token",
               }),
               0);

    const std::string lock = read_text_file(target.path() / ".tempify-lock.json");
    REQUIRE(lock.find("\"api_token\": \"<redacted>\"") != std::string::npos);
    REQUIRE(lock.find("super-secret-token") == std::string::npos);
    REQUIRE(lock.find("\"project_name\": \"Secure App\"") != std::string::npos);
}

TEST_CASE(TempifyApp_doctor_outputs_environment_summary) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-doctor-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-doctor-data-home");
    ScopedTempifyConfigHome config_home(std::filesystem::temp_directory_path() / "tempify-app-doctor-config-home");
    std::filesystem::create_directories(workspace.path() / ".tempify");
    write_text_file(workspace.path() / ".tempify" / "config.json", "{}\n");
    ScopedCurrentPath cwd(workspace.path());
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"doctor"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Tempify Doctor") != std::string::npos);
    REQUIRE(output.find("Data root:") != std::string::npos);
    REQUIRE(output.find("Global config:") != std::string::npos);
    REQUIRE(output.find((config_home.path() / "tempify" / "config.json").string()) != std::string::npos);
    REQUIRE(output.find("Shared index status: ok") != std::string::npos);
    REQUIRE(output.find("Workspace config:") != std::string::npos);
    REQUIRE(output.find((workspace.path() / ".tempify" / "config.json").string()) != std::string::npos);
    REQUIRE(output.find("Catalog status:") != std::string::npos);
}

TEST_CASE(TempifyApp_doctor_json_outputs_machine_readable_summary) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-doctor-json-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-doctor-json-data-home");
    ScopedTempifyConfigHome config_home(std::filesystem::temp_directory_path() / "tempify-app-doctor-json-config-home");
    std::filesystem::create_directories(workspace.path() / ".tempify");
    write_text_file(workspace.path() / ".tempify" / "config.json", "{}\n");
    ScopedCurrentPath cwd(workspace.path());
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"doctor", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"data_root\": ") != std::string::npos);
    REQUIRE(output.find("\"global_config\": ") != std::string::npos);
    REQUIRE(output.find(json_escaped_path(config_home.path() / "tempify" / "config.json")) != std::string::npos);
    REQUIRE(output.find("\"workspace_config\": ") != std::string::npos);
    REQUIRE(output.find(json_escaped_path(workspace.path() / ".tempify" / "config.json")) != std::string::npos);
    REQUIRE(output.find("\"shared_templates_root\": ") != std::string::npos);
    REQUIRE(output.find("\"shared_index_exists\": ") != std::string::npos);
    REQUIRE(output.find("\"shared_index_status\": \"ok\"") != std::string::npos);
    REQUIRE(output.find("\"catalog_status\": ") != std::string::npos);
}
