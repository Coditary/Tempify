#include "TempifyAppTestSupport.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::read_text_file;
using tempify::test_support::tempify_binary_path;
using tempify::test_support::write_text_file;

}

TEST_CASE(TempifyApp_reapply_blocks_version_downgrade_with_structured_details) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-downgrade-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-downgrade-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Downgrade App",
        "--set", "name_slug=downgrade-app",
        "--set", "namespace=downgrade_ns",
        "--set", "include_ci=false",
        "--set", "author=Downgrade Tester",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string current_version = "\"version\": \"0.1.0\"";
    const auto version_pos = lock_text.find(current_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, current_version.size(), "\"version\": \"0.2.0\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    try {
        static_cast<void>(app.run({
            "basic_cpp",
            target.path().string(),
            "--set", "project_name=Downgrade App",
            "--set", "name_slug=downgrade-app",
            "--set", "namespace=downgrade_ns",
            "--set", "include_ci=false",
            "--set", "author=Downgrade Tester",
            "--reapply",
        }));
        REQUIRE(false);
    } catch (const tempify::ReapplyBlockedError& error) {
        REQUIRE(std::string(error.what()).find("version downgrade") != std::string::npos);
        REQUIRE(error.version_transition().has_value());
        REQUIRE_EQ(error.version_transition()->kind, std::string("downgrade"));
        REQUIRE_EQ(error.version_transition()->reason, std::string("backward_version_change"));
        REQUIRE_EQ(error.version_transition()->from_version, std::string("0.2.0"));
        REQUIRE_EQ(error.version_transition()->to_version, std::string("0.1.0"));
        REQUIRE(std::find(error.review_paths().begin(), error.review_paths().end(), ".tempify-lock.json") != error.review_paths().end());
    }
}

TEST_CASE(TempifyApp_reapply_blocks_pre_1_0_minor_upgrade_with_structured_details) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-pre-1-0-upgrade-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-pre-1-0-upgrade-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Pre1 Upgrade App",
        "--set", "name_slug=pre1-upgrade-app",
        "--set", "namespace=pre1_upgrade_ns",
        "--set", "include_ci=false",
        "--set", "author=Upgrade Tester",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string current_version = "\"version\": \"0.1.0\"";
    const auto version_pos = lock_text.find(current_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, current_version.size(), "\"version\": \"0.0.9\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    try {
        static_cast<void>(app.run({
            "basic_cpp",
            target.path().string(),
            "--set", "project_name=Pre1 Upgrade App",
            "--set", "name_slug=pre1-upgrade-app",
            "--set", "namespace=pre1_upgrade_ns",
            "--set", "include_ci=false",
            "--set", "author=Upgrade Tester",
            "--reapply",
        }));
        REQUIRE(false);
    } catch (const tempify::ReapplyBlockedError& error) {
        REQUIRE(std::string(error.what()).find("pre-1.0 minor upgrade") != std::string::npos);
        REQUIRE(error.version_transition().has_value());
        REQUIRE_EQ(error.version_transition()->kind, std::string("upgrade"));
        REQUIRE_EQ(error.version_transition()->reason, std::string("pre_1_0_minor_upgrade"));
        REQUIRE_EQ(error.version_transition()->from_version, std::string("0.0.9"));
        REQUIRE_EQ(error.version_transition()->to_version, std::string("0.1.0"));
        REQUIRE(std::find(error.review_paths().begin(), error.review_paths().end(), ".tempify-lock.json") != error.review_paths().end());
    }
}

TEST_CASE(TempifyApp_reapply_updates_safe_files_keeps_local_edits_and_skips_hooks) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply App",
        "--set", "name_slug=old-slug",
        "--set", "namespace=reapply_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Tester",
    }), 0);

    write_text_file(target.path() / "README.md", "# local readme\n");
    const std::string original_summary = read_text_file(target.path() / ".tempify-summary.txt");

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply App",
        "--set", "name_slug=new-slug",
        "--set", "namespace=reapply_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Tester",
        "--reapply",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("Reapplied basic_cpp") != std::string::npos);
    REQUIRE(output.find("Update policy: allow") != std::string::npos);
    REQUIRE(output.find("Next step: ") != std::string::npos);
    REQUIRE(output.find("Updated: 1") != std::string::npos);
    REQUIRE(output.find("Kept local edits: 1") != std::string::npos);
    REQUIRE(output.find("Hooks: skipped") != std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "README.md"), std::string("# local readme\n"));
    REQUIRE(read_text_file(target.path() / "CMakeLists.txt").find("project(new-slug") != std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / ".tempify-summary.txt"), original_summary);

    ScopedStdoutCapture diff_capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply App",
        "--set", "name_slug=new-slug",
        "--set", "namespace=reapply_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Tester",
        "--diff",
        "--json",
    }), 0);
    const std::string diff_output = diff_capture.str();
    REQUIRE(diff_output.find("\"path\": \"README.md\"") != std::string::npos);
    REQUIRE(diff_output.find("\"reason\": \"local_edit\"") != std::string::npos);
    REQUIRE(diff_output.find("\"reapply_action\": \"keep\"") != std::string::npos);
    REQUIRE(diff_output.find("\"path\": \"CMakeLists.txt\"") != std::string::npos);
    REQUIRE(diff_output.find("\"path\": \"CMakeLists.txt\", \"status\": \"unchanged\"") != std::string::npos);
}

TEST_CASE(TempifyApp_reapply_json_outputs_machine_readable_success_result) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-json-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-json-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Json App",
        "--set", "name_slug=old-json-slug",
        "--set", "namespace=reapply_json_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Json Tester",
    }), 0);

    write_text_file(target.path() / "README.md", "# local readme\n");

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Json App",
        "--set", "name_slug=new-json-slug",
        "--set", "namespace=reapply_json_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Json Tester",
        "--reapply",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"status\": \"ok\"") != std::string::npos);
    REQUIRE(output.find("\"mode\": \"reapply\"") != std::string::npos);
    REQUIRE(output.find("\"update\": {") != std::string::npos);
    REQUIRE(output.find("\"kind\": \"same_template\"") != std::string::npos);
    REQUIRE(output.find("\"policy\": {") != std::string::npos);
    REQUIRE(output.find("\"action\": \"allow\"") != std::string::npos);
    REQUIRE(output.find("\"reapply\": {") != std::string::npos);
    REQUIRE(output.find("\"status\": \"ready\"") != std::string::npos);
    REQUIRE(output.find("\"applied\": {") != std::string::npos);
    REQUIRE(output.find("\"update\": [") != std::string::npos);
    REQUIRE(output.find("\"CMakeLists.txt\"") != std::string::npos);
    REQUIRE(output.find("\"kept\": [") != std::string::npos);
    REQUIRE(output.find("\"README.md\"") != std::string::npos);
    REQUIRE(output.find("\"blocked\": {") != std::string::npos);
}

TEST_CASE(TempifyApp_reapply_subcommand_json_outputs_machine_readable_success_result) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-json-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-json-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Subcommand App",
        "--set", "name_slug=old-subcommand-slug",
        "--set", "namespace=reapply_subcommand_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Json Tester",
    }), 0);

    write_text_file(target.path() / "README.md", "# local readme\n");

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "reapply",
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Subcommand App",
        "--set", "name_slug=new-subcommand-slug",
        "--set", "namespace=reapply_subcommand_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Json Tester",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"status\": \"ok\"") != std::string::npos);
    REQUIRE(output.find("\"mode\": \"reapply\"") != std::string::npos);
    REQUIRE(output.find("\"update\": {") != std::string::npos);
    REQUIRE(output.find("\"action\": \"allow\"") != std::string::npos);
    REQUIRE(output.find("\"reapply\": {") != std::string::npos);
    REQUIRE(output.find("\"status\": \"ready\"") != std::string::npos);
}

TEST_CASE(TempifyApp_reapply_report_outputs_report_only_json_without_mutation) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-report-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-report-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Report App",
        "--set", "name_slug=old-report-slug",
        "--set", "namespace=reapply_report_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Report Tester",
    }), 0);

    const std::string original_cmake = read_text_file(target.path() / "CMakeLists.txt");
    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Report App",
        "--set", "name_slug=new-report-slug",
        "--set", "namespace=reapply_report_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Report Tester",
        "--reapply",
        "--report",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"reapply\": {") != std::string::npos);
    REQUIRE(output.find("\"reapply_action\": \"update\"") != std::string::npos);
    REQUIRE(output.find("\"path\": \"CMakeLists.txt\"") != std::string::npos);
    REQUIRE(output.find("\"mode\": \"reapply\"") == std::string::npos);
    REQUIRE(output.find("\"applied\": {") == std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "CMakeLists.txt"), original_cmake);
}

TEST_CASE(TempifyApp_reapply_subcommand_report_outputs_report_only_json_without_mutation) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-report-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-report-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Subcommand Report App",
        "--set", "name_slug=old-subcommand-report-slug",
        "--set", "namespace=reapply_subcommand_report_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Report Tester",
    }), 0);

    const std::string original_cmake = read_text_file(target.path() / "CMakeLists.txt");
    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "reapply",
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Reapply Subcommand Report App",
        "--set", "name_slug=new-subcommand-report-slug",
        "--set", "namespace=reapply_subcommand_report_ns",
        "--set", "include_ci=false",
        "--set", "author=Reapply Report Tester",
        "--report",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"reapply\": {") != std::string::npos);
    REQUIRE(output.find("\"reapply_action\": \"update\"") != std::string::npos);
    REQUIRE(output.find("\"mode\": \"reapply\"") == std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "CMakeLists.txt"), original_cmake);
}

TEST_CASE(TempifyApp_reapply_blocks_conflicts_without_mutation) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-conflict-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-conflict-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Conflict Reapply App",
        "--set", "name_slug=conflict-reapply-app",
        "--set", "namespace=conflict_reapply_ns",
        "--set", "include_ci=false",
        "--set", "author=Old Author",
    }), 0);

    write_text_file(target.path() / "README.md", "# local conflict\n");
    const std::string original_readme = read_text_file(target.path() / "README.md");
    const std::string original_lock = read_text_file(target.path() / ".tempify-lock.json");

    try {
        static_cast<void>(app.run({
            "basic_cpp",
            target.path().string(),
            "--set", "project_name=Conflict Reapply App",
            "--set", "name_slug=conflict-reapply-app",
            "--set", "namespace=conflict_reapply_ns",
            "--set", "include_ci=false",
            "--set", "author=New Author",
            "--reapply",
        }));
        REQUIRE(false);
    } catch (const tempify::TempifyError& error) {
        REQUIRE(std::string(error.what()).find("Reapply blocked") != std::string::npos);
        REQUIRE(std::string(error.what()).find("conflict item") != std::string::npos);
        REQUIRE(std::string(error.what()).find("--diff") != std::string::npos);
    }

    REQUIRE_EQ(read_text_file(target.path() / "README.md"), original_readme);
    REQUIRE_EQ(read_text_file(target.path() / ".tempify-lock.json"), original_lock);
}

TEST_CASE(Tempify_json_error_wraps_reapply_block_and_includes_blocked_paths) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-json-error-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-json-error-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Json Conflict App",
        "--set", "name_slug=json-conflict-app",
        "--set", "namespace=json_conflict_ns",
        "--set", "include_ci=false",
        "--set", "author=Old Author",
    }), 0);

    write_text_file(target.path() / "README.md", "# local conflict\n");

    const std::filesystem::path binary = tempify_binary_path();
    const std::filesystem::path stderr_file = std::filesystem::temp_directory_path() / "tempify-app-reapply-json-error-stderr.txt";
    std::filesystem::remove(stderr_file);
    const int exit_code = std::system((std::string{"\""} + binary.string()
                                       + "\" basic_cpp \"" + target.path().string()
                                       + "\" --set project_name=JsonConflictApp"
                                       + " --set name_slug=json-conflict-app"
                                       + " --set namespace=json_conflict_ns"
                                       + " --set include_ci=false"
                                       + " --set author=NewAuthor"
                                       + " --reapply --json > /dev/null 2> \"" + stderr_file.string() + "\"").c_str());

    REQUIRE(exit_code != 0);
    const std::string error_output = read_text_file(stderr_file);
    REQUIRE(error_output.find("\"status\": \"error\"") != std::string::npos);
    REQUIRE(error_output.find("\"code\": \"REAPPLY_BLOCKED\"") != std::string::npos);
    REQUIRE(error_output.find("Reapply blocked") != std::string::npos);
    REQUIRE(error_output.find("\"blocked\": {") != std::string::npos);
    REQUIRE(error_output.find("\"conflict\": [") != std::string::npos);
    REQUIRE(error_output.find("\"review\": [") != std::string::npos);
    REQUIRE(error_output.find("\"origin_mismatch\": null") != std::string::npos);
    REQUIRE(error_output.find("\"version_transition\": null") != std::string::npos);
    REQUIRE(error_output.find("README.md") != std::string::npos);
    std::filesystem::remove(stderr_file);
}

TEST_CASE(TempifyApp_reapply_requires_existing_generation_lock) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-no-lock-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-no-lock-data-home");
    tempify::TempifyApp app;

    std::filesystem::create_directories(target.path());
    try {
        static_cast<void>(app.run({
            "basic_cpp",
            target.path().string(),
            "--set", "project_name=No Lock App",
            "--set", "name_slug=no-lock-app",
            "--set", "namespace=no_lock_ns",
            "--set", "include_ci=false",
            "--set", "author=No Lock Tester",
            "--reapply",
        }));
        REQUIRE(false);
    } catch (const tempify::TempifyError& error) {
        REQUIRE(std::string(error.what()).find("requires existing .tempify-lock.json") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_reapply_blocks_origin_template_mismatch) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-origin-mismatch-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-origin-mismatch-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "layered_cpp_product",
        target.path().string(),
        "--set", "project_name=Origin Mismatch Reapply App",
        "--set", "project_slug=origin-mismatch-reapply-app",
        "--set", "language_standard=c++23",
        "--set", "include_ci=true",
        "--set", "ci_provider=github",
    }), 0);

    const std::string original_lock = read_text_file(target.path() / ".tempify-lock.json");

    try {
        static_cast<void>(app.run({
            "basic_cpp",
            target.path().string(),
            "--set", "project_name=Origin Mismatch Reapply App",
            "--set", "name_slug=origin-mismatch-reapply-app",
            "--set", "namespace=origin_mismatch_reapply_ns",
            "--set", "include_ci=false",
            "--set", "author=Origin Tester",
            "--reapply",
        }));
        REQUIRE(false);
    } catch (const tempify::ReapplyBlockedError& error) {
        REQUIRE(std::string(error.what()).find("origin template mismatch") != std::string::npos);
        REQUIRE(std::string(error.what()).find("layered_cpp_product") != std::string::npos);
        REQUIRE(error.origin_mismatch().has_value());
        REQUIRE_EQ(error.origin_mismatch()->lockfile_path, std::string(".tempify-lock.json"));
        REQUIRE_EQ(error.origin_mismatch()->origin_template_id, std::string("layered_cpp_product"));
        REQUIRE_EQ(error.origin_mismatch()->requested_template_id, std::string("basic_cpp"));
        REQUIRE(!error.version_transition().has_value());
        REQUIRE(std::find(error.review_paths().begin(), error.review_paths().end(), ".tempify-lock.json") != error.review_paths().end());
    }

    REQUIRE_EQ(read_text_file(target.path() / ".tempify-lock.json"), original_lock);
}

TEST_CASE(Tempify_json_error_wraps_origin_template_mismatch_with_structured_blocked_details) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-origin-mismatch-json-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-origin-mismatch-json-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "layered_cpp_product",
        target.path().string(),
        "--set", "project_name=Origin Json App",
        "--set", "project_slug=origin-json-app",
        "--set", "language_standard=c++23",
        "--set", "include_ci=true",
        "--set", "ci_provider=github",
    }), 0);

    const std::filesystem::path binary = tempify_binary_path();
    const std::filesystem::path stderr_file = std::filesystem::temp_directory_path() / "tempify-app-reapply-origin-mismatch-json-stderr.txt";
    std::filesystem::remove(stderr_file);
    const int exit_code = std::system((std::string{"\""} + binary.string()
                                       + "\" basic_cpp \"" + target.path().string()
                                       + "\" --set project_name=OriginJsonApp"
                                       + " --set name_slug=origin-json-app"
                                       + " --set namespace=origin_json_ns"
                                       + " --set include_ci=false"
                                       + " --set author=OriginJsonTester"
                                       + " --reapply --json > /dev/null 2> \"" + stderr_file.string() + "\"").c_str());

    REQUIRE(exit_code != 0);
    const std::string error_output = read_text_file(stderr_file);
    REQUIRE(error_output.find("\"code\": \"REAPPLY_BLOCKED\"") != std::string::npos);
    REQUIRE(error_output.find("\"origin_mismatch\": {") != std::string::npos);
    REQUIRE(error_output.find("\"version_transition\": null") != std::string::npos);
    REQUIRE(error_output.find("\"lockfile\": \".tempify-lock.json\"") != std::string::npos);
    REQUIRE(error_output.find("\"origin_template\": {") != std::string::npos);
    REQUIRE(error_output.find("\"requested_template\": {") != std::string::npos);
    REQUIRE(error_output.find("\"id\": \"layered_cpp_product\"") != std::string::npos);
    REQUIRE(error_output.find("\"id\": \"basic_cpp\"") != std::string::npos);
    std::filesystem::remove(stderr_file);
}

TEST_CASE(Tempify_json_error_wraps_pre_1_0_minor_upgrade_with_structured_blocked_details) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-pre-1-0-json-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-pre-1-0-json-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Pre1 Json App",
        "--set", "name_slug=pre1-json-app",
        "--set", "namespace=pre1_json_ns",
        "--set", "include_ci=false",
        "--set", "author=Upgrade Tester",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string current_version = "\"version\": \"0.1.0\"";
    const auto version_pos = lock_text.find(current_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, current_version.size(), "\"version\": \"0.0.9\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    const std::filesystem::path binary = tempify_binary_path();
    const std::filesystem::path stderr_file = std::filesystem::temp_directory_path() / "tempify-app-reapply-pre-1-0-json-stderr.txt";
    std::filesystem::remove(stderr_file);
    const int exit_code = std::system((std::string{"\""} + binary.string()
                                       + "\" basic_cpp \"" + target.path().string()
                                       + "\" --set project_name=Pre1JsonApp"
                                       + " --set name_slug=pre1-json-app"
                                       + " --set namespace=pre1_json_ns"
                                       + " --set include_ci=false"
                                       + " --set author=UpgradeTester"
                                       + " --reapply --json > /dev/null 2> \"" + stderr_file.string() + "\"").c_str());

    REQUIRE(exit_code != 0);
    const std::string error_output = read_text_file(stderr_file);
    REQUIRE(error_output.find("\"code\": \"REAPPLY_BLOCKED\"") != std::string::npos);
    REQUIRE(error_output.find("\"version_transition\": {") != std::string::npos);
    REQUIRE(error_output.find("\"kind\": \"upgrade\"") != std::string::npos);
    REQUIRE(error_output.find("\"reason\": \"pre_1_0_minor_upgrade\"") != std::string::npos);
    REQUIRE(error_output.find("\"from_version\": \"0.0.9\"") != std::string::npos);
    REQUIRE(error_output.find("\"to_version\": \"0.1.0\"") != std::string::npos);
    std::filesystem::remove(stderr_file);
}

TEST_CASE(Tempify_json_error_wraps_reapply_subcommand_block_with_structured_details) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-json-error-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-json-error-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Json Subcommand Conflict App",
        "--set", "name_slug=json-subcommand-conflict-app",
        "--set", "namespace=json_subcommand_conflict_ns",
        "--set", "include_ci=false",
        "--set", "author=Old Author",
    }), 0);

    write_text_file(target.path() / "README.md", "# local conflict\n");

    const std::filesystem::path binary = tempify_binary_path();
    const std::filesystem::path stderr_file = std::filesystem::temp_directory_path() / "tempify-app-reapply-subcommand-json-error-stderr.txt";
    std::filesystem::remove(stderr_file);
    const int exit_code = std::system((std::string{"\""} + binary.string()
                                       + "\" reapply basic_cpp \"" + target.path().string()
                                       + "\" --set project_name=JsonSubcommandConflictApp"
                                       + " --set name_slug=json-subcommand-conflict-app"
                                       + " --set namespace=json_subcommand_conflict_ns"
                                       + " --set include_ci=false"
                                       + " --set author=NewAuthor"
                                       + " --json > /dev/null 2> \"" + stderr_file.string() + "\"").c_str());

    REQUIRE(exit_code != 0);
    const std::string error_output = read_text_file(stderr_file);
    REQUIRE(error_output.find("\"status\": \"error\"") != std::string::npos);
    REQUIRE(error_output.find("\"code\": \"REAPPLY_BLOCKED\"") != std::string::npos);
    REQUIRE(error_output.find("\"blocked\": {") != std::string::npos);
    REQUIRE(error_output.find("\"conflict\": [") != std::string::npos);
    REQUIRE(error_output.find("README.md") != std::string::npos);
    std::filesystem::remove(stderr_file);
}
