#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::create_required_only_template;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::write_text_file;

} // namespace

TEST_CASE(TempifyApp_test_runs_template_fixtures) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Test basic_cpp") != std::string::npos);
    REQUIRE(output.find("PASS default_no_ci (5 files)") != std::string::npos);
    REQUIRE(output.find("PASS ci_enabled (5 files)") != std::string::npos);
    REQUIRE(output.find("2/2 fixtures passed") != std::string::npos);
    REQUIRE(output.find("10 snapshot artifacts") != std::string::npos);
    REQUIRE(output.find("snapshot artifacts") != std::string::npos);
    REQUIRE(output.find(" ms)") != std::string::npos);
}

TEST_CASE(TempifyApp_test_fixture_flag_runs_single_named_fixture) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-filtered-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp", "--fixture", "ci_enabled"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("PASS ci_enabled (5 files)") != std::string::npos);
    REQUIRE(output.find("default_no_ci") == std::string::npos);
    REQUIRE(output.find("1/1 fixtures passed") != std::string::npos);
    REQUIRE(output.find("5 snapshot artifacts") != std::string::npos);
}

TEST_CASE(TempifyApp_test_list_fixtures_outputs_names_only) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-test-list-fixtures-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp", "--list-fixtures"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("default_no_ci") != std::string::npos);
    REQUIRE(output.find("ci_enabled") != std::string::npos);
    REQUIRE(output.find("PASS ") == std::string::npos);
    REQUIRE(output.find("fixtures passed") == std::string::npos);
}

TEST_CASE(TempifyApp_test_list_fixtures_respects_fixture_filter) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-list-filter-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp", "--list-fixtures", "--fixture", "ci_enabled"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("ci_enabled") != std::string::npos);
    REQUIRE(output.find("default_no_ci") == std::string::npos);
}

TEST_CASE(TempifyApp_test_list_fixtures_json_outputs_machine_readable_fixture_metadata) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-list-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp", "--list-fixtures", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"total\": 2") != std::string::npos);
    REQUIRE(output.find("\"name\": \"default_no_ci\"") != std::string::npos);
    REQUIRE(output.find("\"name\": \"ci_enabled\"") != std::string::npos);
    REQUIRE(output.find("\"has_answers_file\": true") != std::string::npos);
    REQUIRE(output.find("\"has_lockfile_snapshot\": false") != std::string::npos);
    REQUIRE(output.find("\"snapshot_root\": ") != std::string::npos);
    REQUIRE(output.find("\"answers_file\": ") != std::string::npos);
    REQUIRE(output.find("fixtures passed") == std::string::npos);
}

TEST_CASE(TempifyApp_test_json_outputs_machine_readable_report) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp", "--fixture", "ci_enabled", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"passed\": 1") != std::string::npos);
    REQUIRE(output.find("\"failed\": 0") != std::string::npos);
    REQUIRE(output.find("\"name\": \"ci_enabled\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"passed\"") != std::string::npos);
    REQUIRE(output.find("\"elapsed_ms\": ") != std::string::npos);
}

TEST_CASE(TempifyApp_test_json_reports_failures_machine_readably) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() /
                                         "tempify-app-test-json-fail-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-json-fail-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "broken_case" / "answers.json",
                    "{\n  \"required_name\": \"Mismatch App\"\n}\n");
    write_text_file(template_path / "tests" / "broken_case" / "snapshot" / "README.md", "# Wrong App\n");

    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", template_path.string(), "--json"}), 1);
    const std::string output = capture.str();
    REQUIRE(output.find("\"failed\": 1") != std::string::npos);
    REQUIRE(output.find("\"status\": \"failed\"") != std::string::npos);
    REQUIRE(output.find("\"name\": \"broken_case\"") != std::string::npos);
    REQUIRE(output.find("\"code\": \"SNAPSHOT_MISMATCH\"") != std::string::npos);
    REQUIRE(output.find("\"kind\": \"snapshot_mismatch\"") != std::string::npos);
    REQUIRE(output.find("\"message\": ") != std::string::npos);
    REQUIRE(output.find("snapshot mismatch") != std::string::npos);
}

TEST_CASE(TempifyApp_test_update_snapshots_rewrites_fixture_outputs) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-test-update-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-update-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "refresh_case" / "answers.json",
                    "{\n  \"required_name\": \"Updated App\"\n}\n");
    write_text_file(template_path / "tests" / "refresh_case" / "snapshot" / "README.md", "# Old App\n");

    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", template_path.string(), "--update-snapshots"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("PASS refresh_case (1 files)") != std::string::npos);
    REQUIRE(read_text_file(template_path / "tests" / "refresh_case" / "snapshot" / "README.md") ==
            std::string("# Updated App\n"));
}

TEST_CASE(TempifyApp_test_update_snapshots_rewrites_lock_snapshot_when_present) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() /
                                         "tempify-app-test-update-lock-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-update-lock-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "refresh_lock" / "answers.json",
                    "{\n  \"required_name\": \"Lock App\"\n}\n");
    write_text_file(template_path / "tests" / "refresh_lock" / "snapshot" / "README.md", "# stale\n");
    write_text_file(template_path / "tests" / "refresh_lock" / "lock.json", "{\n  \"generated_at\": \"stale\"\n}\n");

    tempify::TempifyApp app;

    REQUIRE_EQ(app.run({"test", template_path.string(), "--update-snapshots"}), 0);
    const std::string lock_text = read_text_file(template_path / "tests" / "refresh_lock" / "lock.json");
    REQUIRE(lock_text.find("<generated-at>") != std::string::npos);
    REQUIRE(lock_text.find("<build-root>") != std::string::npos);
}

TEST_CASE(TempifyApp_test_fixture_flag_rejects_unknown_fixture) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-test-filtered-missing-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "basic_cpp", "--fixture", "missing_fixture"}), 1);
    const std::string output = capture.str();
    REQUIRE(output.find("FAIL missing_fixture") != std::string::npos);
    REQUIRE(output.find("Fixture not found") != std::string::npos);
    REQUIRE(output.find("0/1 fixtures passed, 1 failed") != std::string::npos);
}

TEST_CASE(TempifyApp_test_runs_layered_template_fixture) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-layered-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "layered_cpp_product"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Test layered_cpp_product") != std::string::npos);
    REQUIRE(output.find("PASS ci_enabled_layered (3 files)") != std::string::npos);
    REQUIRE(output.find("1/1 fixtures passed") != std::string::npos);
    REQUIRE(output.find("3 snapshot artifacts") != std::string::npos);
}

TEST_CASE(TempifyApp_test_runs_hook_heavy_fixture_with_lock_snapshot) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-hooks-lock-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "advanced_hooks_layout"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Test advanced_hooks_layout") != std::string::npos);
    REQUIRE(output.find("PASS hooks_and_lock (7 files, lock)") != std::string::npos);
    REQUIRE(output.find("1/1 fixtures passed") != std::string::npos);
    REQUIRE(output.find("8 snapshot artifacts") != std::string::npos);
}

TEST_CASE(TempifyApp_test_reports_snapshot_mismatch_with_diff_hint) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-test-mismatch-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-mismatch-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "broken_case" / "answers.json",
                    "{\n  \"required_name\": \"Mismatch App\"\n}\n");
    write_text_file(template_path / "tests" / "broken_case" / "snapshot" / "README.md", "# Wrong App\n");
    write_text_file(template_path / "tests" / "passing_case" / "answers.json",
                    "{\n  \"required_name\": \"Passing App\"\n}\n");
    write_text_file(template_path / "tests" / "passing_case" / "snapshot" / "README.md", "# Passing App\n");

    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", template_path.string()}), 1);
    const std::string output = capture.str();
    REQUIRE(output.find("FAIL broken_case") != std::string::npos);
    REQUIRE(output.find("snapshot mismatch") != std::string::npos);
    REQUIRE(output.find("line 1") != std::string::npos);
    REQUIRE(output.find("PASS passing_case (1 files)") != std::string::npos);
    REQUIRE(output.find("1/2 fixtures passed, 1 failed") != std::string::npos);
}

TEST_CASE(TempifyApp_test_reports_missing_and_unexpected_snapshot_files) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-test-file-set-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-file-set-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "wrong_files" / "answers.json",
                    "{\n  \"required_name\": \"Wrong Files App\"\n}\n");
    write_text_file(template_path / "tests" / "wrong_files" / "snapshot" / "expected-only.txt", "only in snapshot\n");

    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", template_path.string()}), 1);
    const std::string output = capture.str();
    REQUIRE(output.find("FAIL wrong_files") != std::string::npos);
    REQUIRE(output.find("missing [expected-only.txt]") != std::string::npos);
    REQUIRE(output.find("unexpected [README.md") != std::string::npos);
    REQUIRE(output.find("0/1 fixtures passed, 1 failed") != std::string::npos);
}

TEST_CASE(TempifyApp_test_rejects_fixture_missing_snapshot_directory) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() /
                                         "tempify-app-test-missing-snapshot-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-test-missing-snapshot-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "missing_snapshot" / "answers.json",
                    "{\n  \"required_name\": \"App\"\n}\n");

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({"test", template_path.string()}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_test_rejects_fixture_bad_lock_snapshot_path) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() /
                                         "tempify-app-test-bad-lock-path-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-test-bad-lock-path-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "bad_lock" / "answers.json", "{\n  \"required_name\": \"App\"\n}\n");
    write_text_file(template_path / "tests" / "bad_lock" / "snapshot" / "README.md", "# App\n");
    std::filesystem::create_directories(template_path / "tests" / "bad_lock" / "lock.json");

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({"test", template_path.string()}), tempify::TempifyError);
}
