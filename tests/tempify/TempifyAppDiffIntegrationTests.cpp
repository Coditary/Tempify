#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::create_required_only_template_with_version;
using tempify::test_support::read_text_file;
using tempify::test_support::write_text_file;

}

TEST_CASE(TempifyApp_diff_reports_managed_changes_without_mutation) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Diff App",
        "--set", "name_slug=diff-app",
        "--set", "namespace=diff_ns",
        "--set", "include_ci=false",
        "--set", "author=Diff Tester",
    }), 0);

    const std::string original_main = read_text_file(target.path() / "src" / "main.cpp");
    write_text_file(target.path() / "README.md", "# user edited\n");
    std::filesystem::remove(target.path() / "CMakeLists.txt");

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Diff App",
        "--set", "name_slug=diff-app",
        "--set", "namespace=diff_ns",
        "--set", "include_ci=false",
        "--set", "author=Diff Tester",
        "--diff",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("Diff ") != std::string::npos);
    REQUIRE(output.find("Origin lock: ") != std::string::npos);
    REQUIRE(output.find("Change: 1") != std::string::npos);
    REQUIRE(output.find("Create: 1") != std::string::npos);
    REQUIRE(output.find("Unchanged:") != std::string::npos);
    REQUIRE(output.find("Update policy: ") != std::string::npos);
    REQUIRE(output.find("Update reason: ") != std::string::npos);
    REQUIRE(output.find("Update recommendation: ") != std::string::npos);
    REQUIRE(output.find("Reapply status: conflict") != std::string::npos);
    REQUIRE(output.find("Reapply keep: 1") != std::string::npos);
    REQUIRE(output.find("Reapply conflict: 1") != std::string::npos);
    REQUIRE(output.find("change  README.md [local_edit] -> keep") != std::string::npos);
    REQUIRE(output.find("create  CMakeLists.txt [local_edit] -> conflict") != std::string::npos);
    REQUIRE(output.find("unchanged  src/main.cpp") != std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "README.md"), std::string("# user edited\n"));
    REQUIRE(!std::filesystem::exists(target.path() / "CMakeLists.txt"));
    REQUIRE_EQ(read_text_file(target.path() / "src" / "main.cpp"), original_main);
}

TEST_CASE(TempifyApp_diff_json_outputs_machine_readable_report) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-json-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-json-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Diff Json App",
        "--set", "name_slug=diff-json-app",
        "--set", "namespace=diff_json_ns",
        "--set", "include_ci=false",
        "--set", "author=Diff Json Tester",
    }), 0);

    write_text_file(target.path() / "README.md", "# changed\n");
    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Diff Json App",
        "--set", "name_slug=diff-json-app",
        "--set", "namespace=diff_json_ns",
        "--set", "include_ci=false",
        "--set", "author=Diff Json Tester",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"build_root\": ") != std::string::npos);
    REQUIRE(output.find("\"origin\": {") != std::string::npos);
    REQUIRE(output.find("\"detected\": true") != std::string::npos);
    REQUIRE(output.find("\"reapply\": {") != std::string::npos);
    REQUIRE(output.find("\"status\": \"ready\"") != std::string::npos);
    REQUIRE(output.find("\"keep\": 1") != std::string::npos);
    REQUIRE(output.find("\"counts\": {") != std::string::npos);
    REQUIRE(output.find("\"change\": 1") != std::string::npos);
    REQUIRE(output.find("\"entries\": [") != std::string::npos);
    REQUIRE(output.find("\"path\": \"README.md\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"change\"") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"local_edit\"") != std::string::npos);
    REQUIRE(output.find("\"reapply_action\": \"keep\"") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_classifies_template_updates_from_generation_baseline) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-template-update-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-template-update-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Template Update App",
        "--set", "name_slug=template-update-app",
        "--set", "namespace=template_update_ns",
        "--set", "include_ci=false",
        "--set", "author=Old Author",
    }), 0);

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Template Update App",
        "--set", "name_slug=template-update-app",
        "--set", "namespace=template_update_ns",
        "--set", "include_ci=false",
        "--set", "author=New Author",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"template\": {") != std::string::npos);
    REQUIRE(output.find("\"id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"ready\"") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"template_update\"") != std::string::npos);
    REQUIRE(output.find("\"reapply_action\": \"update\"") != std::string::npos);
    REQUIRE(output.find("\"path\": \"README.md\"") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_classifies_conflicts_when_local_and_desired_both_diverge_from_baseline) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-conflict-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-conflict-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Conflict App",
        "--set", "name_slug=conflict-app",
        "--set", "namespace=conflict_ns",
        "--set", "include_ci=false",
        "--set", "author=Old Author",
    }), 0);

    write_text_file(target.path() / "README.md", "# local change\n");
    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Conflict App",
        "--set", "name_slug=conflict-app",
        "--set", "namespace=conflict_ns",
        "--set", "include_ci=false",
        "--set", "author=New Author",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"reapply\": {") != std::string::npos);
    REQUIRE(output.find("\"status\": \"conflict\"") != std::string::npos);
    REQUIRE(output.find("\"conflict\": 1") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"conflict\"") != std::string::npos);
    REQUIRE(output.find("\"reapply_action\": \"conflict\"") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_detects_delete_candidates_from_lockfile) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-delete-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-delete-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Diff Delete App",
        "--set", "name_slug=diff-delete-app",
        "--set", "namespace=diff_delete_ns",
        "--set", "include_ci=false",
        "--set", "author=Diff Delete Tester",
    }), 0);

    write_text_file(target.path() / "old-managed.txt", "stale\n");
    write_text_file(target.path() / ".tempify-lock.json",
                    "{\n"
                    "  \"tempify_version\": \"v0.1.2\",\n"
                    "  \"template\": {\n"
                    "    \"id\": \"basic_cpp\",\n"
                    "    \"name\": \"Basic C++ App\",\n"
                    "    \"version\": \"0.1.0\",\n"
                    "    \"root\": \"/tmp/template\"\n"
                    "  },\n"
                    "  \"build_root\": \"/tmp/output\",\n"
                    "  \"generated_at\": \"2026-05-17T00:00:00Z\",\n"
                    "  \"existing_path_behavior\": \"error\",\n"
                    "  \"hook_acceptance\": \"yes\",\n"
                    "  \"hooks_disabled\": false,\n"
                    "  \"managed_files\": [\n"
                    "    \"CMakeLists.txt\",\n"
                    "    \"README.md\",\n"
                    "    \"old-managed.txt\",\n"
                    "    \"src/main.cpp\"\n"
                    "  ],\n"
                    "  \"values\": {\n"
                    "    \"project_name\": \"Diff Delete App\"\n"
                    "  }\n"
                    "}\n");

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Diff Delete App",
        "--set", "name_slug=diff-delete-app",
        "--set", "namespace=diff_delete_ns",
        "--set", "include_ci=false",
        "--set", "author=Diff Delete Tester",
        "--diff",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("Delete: 1") != std::string::npos);
    REQUIRE(output.find("delete  old-managed.txt") != std::string::npos);
    REQUIRE_EQ(read_text_file(target.path() / "old-managed.txt"), std::string("stale\n"));
}

TEST_CASE(TempifyApp_diff_surfaces_origin_template_mismatch_for_reapply) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-origin-mismatch-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-origin-mismatch-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "layered_cpp_product",
        target.path().string(),
        "--set", "project_name=Origin Mismatch App",
        "--set", "project_slug=origin-mismatch-app",
        "--set", "language_standard=c++23",
        "--set", "include_ci=true",
        "--set", "ci_provider=github",
    }), 0);

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Origin Mismatch App",
        "--set", "name_slug=origin-mismatch-app",
        "--set", "namespace=origin_mismatch_ns",
        "--set", "include_ci=false",
        "--set", "author=Origin Tester",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"origin\": {") != std::string::npos);
    REQUIRE(output.find("\"template_id\": \"layered_cpp_product\"") != std::string::npos);
    REQUIRE(output.find("\"matches_requested_template\": false") != std::string::npos);
    REQUIRE(output.find("\"matches_requested_version\": false") != std::string::npos);
    REQUIRE(output.find("\"update\": {") != std::string::npos);
    REQUIRE(output.find("\"kind\": \"template_mismatch\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"review\"") != std::string::npos);
    REQUIRE(output.find("\"review\": 1") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_surfaces_pre_1_0_minor_upgrade_policy_for_update_workflow) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-version-change-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-version-change-data-home");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Version Change App",
        "--set", "name_slug=version-change-app",
        "--set", "namespace=version_change_ns",
        "--set", "include_ci=false",
        "--set", "author=Version Tester",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string old_version = "\"version\": \"0.1.0\"";
    const auto version_pos = lock_text.find(old_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, old_version.size(), "\"version\": \"0.0.9\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Version Change App",
        "--set", "name_slug=version-change-app",
        "--set", "namespace=version_change_ns",
        "--set", "include_ci=false",
        "--set", "author=Version Tester",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"matches_requested_template\": true") != std::string::npos);
    REQUIRE(output.find("\"matches_requested_version\": false") != std::string::npos);
    REQUIRE(output.find("\"update\": {") != std::string::npos);
    REQUIRE(output.find("\"kind\": \"upgrade\"") != std::string::npos);
    REQUIRE(output.find("\"from_version\": \"0.0.9\"") != std::string::npos);
    REQUIRE(output.find("\"to_version\": \"0.1.0\"") != std::string::npos);
    REQUIRE(output.find("\"policy\": {") != std::string::npos);
    REQUIRE(output.find("\"action\": \"review\"") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"pre_1_0_minor_upgrade\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"review\"") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_treats_semver_prerelease_release_transition_as_upgrade) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-semver-prerelease-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-semver-prerelease-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Semver Prerelease App",
        "--set", "name_slug=semver-prerelease-app",
        "--set", "namespace=semver_prerelease_ns",
        "--set", "include_ci=false",
        "--set", "author=Semver Tester",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string old_version = "\"version\": \"0.1.0\"";
    const auto version_pos = lock_text.find(old_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, old_version.size(), "\"version\": \"0.1.0-rc.1\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Semver Prerelease App",
        "--set", "name_slug=semver-prerelease-app",
        "--set", "namespace=semver_prerelease_ns",
        "--set", "include_ci=false",
        "--set", "author=Semver Tester",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"kind\": \"upgrade\"") != std::string::npos);
    REQUIRE(output.find("\"from_version\": \"0.1.0-rc.1\"") != std::string::npos);
    REQUIRE(output.find("\"action\": \"allow\"") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"forward_version_change\"") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_treats_semver_build_metadata_change_as_equivalent_version) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-diff-semver-build-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-semver-build-data-home");
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Semver Build App",
        "--set", "name_slug=semver-build-app",
        "--set", "namespace=semver_build_ns",
        "--set", "include_ci=false",
        "--set", "author=Semver Tester",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string old_version = "\"version\": \"0.1.0\"";
    const auto version_pos = lock_text.find(old_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, old_version.size(), "\"version\": \"0.1.0+7\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=Semver Build App",
        "--set", "name_slug=semver-build-app",
        "--set", "namespace=semver_build_ns",
        "--set", "include_ci=false",
        "--set", "author=Semver Tester",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"matches_requested_version\": false") != std::string::npos);
    REQUIRE(output.find("\"kind\": \"same_template\"") != std::string::npos);
    REQUIRE(output.find("\"from_version\": \"0.1.0+7\"") != std::string::npos);
    REQUIRE(output.find("\"action\": \"allow\"") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"equivalent_version_change\"") != std::string::npos);
}

TEST_CASE(TempifyApp_diff_surfaces_major_upgrade_policy_review) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-major-upgrade-template");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-major-upgrade-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-major-upgrade-data-home");
    const std::filesystem::path template_path = create_required_only_template_with_version(template_root.path(), "1.0.0");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        template_path.string(),
        target.path().string(),
        "--set", "required_name=Major Upgrade App",
    }), 0);

    std::string lock_text = read_text_file(target.path() / ".tempify-lock.json");
    const std::string current_version = "\"version\": \"1.0.0\"";
    const auto version_pos = lock_text.find(current_version);
    REQUIRE(version_pos != std::string::npos);
    lock_text.replace(version_pos, current_version.size(), "\"version\": \"0.9.0\"");
    write_text_file(target.path() / ".tempify-lock.json", lock_text);

    ScopedStdoutCapture capture;
    REQUIRE_EQ(app.run({
        template_path.string(),
        target.path().string(),
        "--set", "required_name=Major Upgrade App",
        "--diff",
        "--json",
    }), 0);

    const std::string output = capture.str();
    REQUIRE(output.find("\"kind\": \"upgrade\"") != std::string::npos);
    REQUIRE(output.find("\"from_version\": \"0.9.0\"") != std::string::npos);
    REQUIRE(output.find("\"to_version\": \"1.0.0\"") != std::string::npos);
    REQUIRE(output.find("\"action\": \"review\"") != std::string::npos);
    REQUIRE(output.find("\"reason\": \"major_version_upgrade\"") != std::string::npos);
}
