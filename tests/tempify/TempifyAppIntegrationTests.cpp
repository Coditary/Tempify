#include "TestHarness.h"

#include "tempify/app/TempifyApp.h"
#include "tempify/cli/CliParser.h"
#include "tempify/domain/CliRequest.h"
#include "tempify/support/Errors.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

class ScopedDirectoryCleanup {
public:
    explicit ScopedDirectoryCleanup(std::filesystem::path path)
        : path_(std::move(path)) {
        std::filesystem::remove_all(path_);
    }

    ~ScopedDirectoryCleanup() {
        std::filesystem::remove_all(path_);
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

class ScopedStdoutCapture {
public:
    ScopedStdoutCapture()
        : previous_(std::cout.rdbuf(buffer_.rdbuf())) {}

    ~ScopedStdoutCapture() {
        std::cout.rdbuf(previous_);
    }

    std::string str() const {
        return buffer_.str();
    }

private:
    std::ostringstream buffer_;
    std::streambuf* previous_ = nullptr;
};

class ScopedStdinCapture {
public:
    explicit ScopedStdinCapture(std::string text)
        : buffer_(std::move(text)),
          previous_(std::cin.rdbuf(buffer_.rdbuf())) {}

    ~ScopedStdinCapture() {
        std::cin.rdbuf(previous_);
    }

private:
    std::istringstream buffer_;
    std::streambuf* previous_ = nullptr;
};

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& path)
        : previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(path);
    }

    ~ScopedCurrentPath() {
        std::filesystem::current_path(previous_);
    }

private:
    std::filesystem::path previous_;
};

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::filesystem::path tempify_binary_path() {
    if (const char* binary = std::getenv("TEMPIFY_TEST_BINARY"); binary != nullptr && binary[0] != '\0') {
        return binary;
    }

    std::filesystem::path fallback = std::filesystem::current_path() / "build" / "tempify";
#ifdef _WIN32
    fallback += ".exe";
#endif
    return fallback;
}

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::filesystem::path create_required_only_template_with_version(const std::filesystem::path& root,
                                                                 const std::string& version) {
    const std::filesystem::path template_root = root / "required_only";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"required_only\",\n"
                    "  name = \"Required Only\",\n"
                    "  version = \"" + version + "\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ required_name }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"required_name\", type = \"string\", prompt = \"Required name\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ required_name }}\n");
    return template_root;
}

std::filesystem::path create_required_only_template(const std::filesystem::path& root) {
    return create_required_only_template_with_version(root, "1.0.0");
}

std::filesystem::path create_duplicate_alias_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "duplicate_alias";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"duplicate_alias\",\n"
                    "  name = \"Duplicate Alias\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"out\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"project_name\", type = \"string\", alias = { \"slug\" } },\n"
                    "      { key = \"slug\", type = \"string\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# hi\n");
    return template_root;
}

std::filesystem::path create_bad_layout_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "bad_layout";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"bad_layout\",\n"
                    "  name = \"Bad Layout\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"out\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "layout.lua",
                    "return {\n"
                    "  { source = \"missing.txt\", target = \"renamed.txt\" },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# hi\n");
    return template_root;
}

std::filesystem::path create_sensitive_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "sensitive_demo";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"sensitive_demo\",\n"
                    "  name = \"Sensitive Demo\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ project_name }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"Secrets\" },\n"
                    "  groups = {\n"
                    "    Secrets = {\n"
                    "      { key = \"project_name\", type = \"string\", prompt = \"Project name\" },\n"
                    "      { key = \"api_token\", type = \"string\", prompt = \"API token\", sensitive = true },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n");
    return template_root;
}

std::filesystem::path create_slow_hook_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "slow_hook_demo";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"slow_hook_demo\",\n"
                    "  name = \"Slow Hook Demo\",\n"
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
    write_text_file(template_root / "hooks" / "post.lua", "while true do end\n");
    return template_root;
}

class ScopedTempifyDataHome {
public:
    explicit ScopedTempifyDataHome(std::filesystem::path path)
        : cleanup_(std::move(path)),
          env_("XDG_DATA_HOME", cleanup_.path().string()) {}

    const std::filesystem::path& path() const noexcept {
        return cleanup_.path();
    }

    std::filesystem::path shared_root() const {
        return cleanup_.path() / "tempify";
    }

private:
    ScopedDirectoryCleanup cleanup_;
    prebyte::test::ScopedEnvironmentVariable env_;
};

class ScopedTempifyConfigHome {
public:
    explicit ScopedTempifyConfigHome(std::filesystem::path path)
        : cleanup_(std::move(path)),
          env_("XDG_CONFIG_HOME", cleanup_.path().string()) {}

    const std::filesystem::path& path() const noexcept {
        return cleanup_.path();
    }

private:
    ScopedDirectoryCleanup cleanup_;
    prebyte::test::ScopedEnvironmentVariable env_;
};

}

TEST_CASE(TempifyApp_render_with_cli_values_generates_expected_output) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-cli-values-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-cli-values-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "basic_cpp",
        target.path().string(),
        "--set", "project_name=CLI App",
        "--set", "name_slug=alias-slug",
        "--set", "namespace=cli_ns",
        "--set", "include_ci=false",
        "--set", "ci_provider=gitlab",
        "--set", "docs_url=https://docs.example",
        "--set", "author=Leodoras",
    });

    REQUIRE_EQ(result, 0);

    const std::string readme = read_text_file(target.path() / "README.md");
    const std::string cmake = read_text_file(target.path() / "CMakeLists.txt");
    const std::string summary = read_text_file(target.path() / ".tempify-summary.txt");

    REQUIRE(readme.find("# CLI App") != std::string::npos);
    REQUIRE(readme.find("- Namespace: cli_ns") != std::string::npos);
    REQUIRE(readme.find("- CI: disabled") != std::string::npos);
    REQUIRE(readme.find("- Docs:") == std::string::npos);
    REQUIRE(cmake.find("project(alias-slug") != std::string::npos);
    REQUIRE(summary.find("slug=alias-slug") != std::string::npos);
    REQUIRE(summary.find("provider=\n") != std::string::npos);
    REQUIRE(summary.find("docs=\n") != std::string::npos);
}

TEST_CASE(TempifyApp_render_inherited_template_merges_layers_and_drop_paths) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-m3-product-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-m3-product-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "m3_product",
        target.path().string(),
        "--set", "project_name=M3 Product",
        "--set", "project_slug=m3-product",
        "--set", "language_standard=c++23",
        "--set", "include_ci=true",
        "--set", "ci_provider=github",
    });

    REQUIRE_EQ(result, 0);
    REQUIRE(std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(std::filesystem::exists(target.path() / "src" / "main.cpp"));
    REQUIRE(std::filesystem::exists(target.path() / ".github" / "workflows" / "ci.yml"));
    REQUIRE(!std::filesystem::exists(target.path() / "base-only.txt"));

    const std::string readme = read_text_file(target.path() / "README.md");
    const std::string main_cpp = read_text_file(target.path() / "src" / "main.cpp");
    REQUIRE(readme.find("Layer: ci") != std::string::npos);
    REQUIRE(readme.find("CI: github") != std::string::npos);
    REQUIRE(main_cpp.find("product M3 Product c++23") != std::string::npos);
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
                    "  \"tempify_version\": \"v0.1.0\",\n"
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
        "m3_product",
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
    REQUIRE(output.find("\"template_id\": \"m3_product\"") != std::string::npos);
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
        "m3_product",
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
        REQUIRE(std::string(error.what()).find("m3_product") != std::string::npos);
        REQUIRE(error.origin_mismatch().has_value());
        REQUIRE_EQ(error.origin_mismatch()->lockfile_path, std::string(".tempify-lock.json"));
        REQUIRE_EQ(error.origin_mismatch()->origin_template_id, std::string("m3_product"));
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
        "m3_product",
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
    REQUIRE(error_output.find("\"id\": \"m3_product\"") != std::string::npos);
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

TEST_CASE(TempifyApp_render_help_mentions_diff_mode) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-diff-help-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"basic_cpp", "out-dir", "-h"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("--diff") != std::string::npos);
    REQUIRE(output.find("--reapply") != std::string::npos);
    REQUIRE(output.find("tempify reapply <template-id> <target>") != std::string::npos);
    REQUIRE(output.find("--report") != std::string::npos);
    REQUIRE(output.find("does not write files and does not run hooks") != std::string::npos);
    REQUIRE(output.find("applies only ready create/update/delete actions") != std::string::npos);
    REQUIRE(output.find("blocks cross-template .tempify-lock.json reuse") != std::string::npos);
    REQUIRE(output.find("major upgrades, pre-1.0 minor upgrades, downgrades") != std::string::npos);
}

TEST_CASE(TempifyApp_cli_parser_enables_tui_mode_flag) {
    tempify::CliParser parser;
    const tempify::CliRequest request = parser.parse({"basic_cpp", "out-dir", "--tui", "--set", "project_name=App"});

    REQUIRE(request.use_tui);
    REQUIRE_EQ(request.template_ref, std::string("basic_cpp"));
    REQUIRE(request.target_dir.has_value());
}

TEST_CASE(TempifyApp_tui_review_screen_allows_back_before_generate) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-tui-review-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-tui-review-data-home");
    tempify::TempifyApp app;
    ScopedStdinCapture input(
        "First App\n"
        "\n"
        "core\n"
        "no\n"
        "n\n"
        ":back\n"
        "Second App\n"
        "\n"
        "core2\n"
        "no\n"
        "yes\n");
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"basic_cpp", target.path().string(), "--set", "author=Review Tester", "--tui"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Review values before generate:") != std::string::npos);
    REQUIRE(output.find("Generate with these values? [Y/n]: ") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "README.md").find("# Second App") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "README.md").find("Namespace: core2") != std::string::npos);
}

TEST_CASE(TempifyApp_questions_outputs_minimal_text_overview) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-q-text-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    const int result = app.run({"m6_advanced", "-q"});

    REQUIRE_EQ(result, 0);
    const std::string output = capture.str();
    REQUIRE(output.find("m6_advanced (M6 Advanced)") != std::string::npos);
    REQUIRE(output.find("Groups: 1") != std::string::npos);
    REQUIRE(output.find("Questions: 3") != std::string::npos);
    REQUIRE(output.find("[Project]") != std::string::npos);
    REQUIRE(output.find("- project_name [string]") != std::string::npos);
    REQUIRE(output.find("choices:") == std::string::npos);
}

TEST_CASE(TempifyApp_questions_long_flag_alias_works) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-questions-long-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    const int result = app.run({"m6_advanced", "--questions"});

    REQUIRE_EQ(result, 0);
    const std::string output = capture.str();
    REQUIRE(output.find("m6_advanced (M6 Advanced)") != std::string::npos);
    REQUIRE(output.find("[Project]") != std::string::npos);
}

TEST_CASE(TempifyApp_questions_json_and_full_modes_work) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-q-json-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"m6_advanced", "-q", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"template\"") != std::string::npos);
        REQUIRE(output.find("\"m6_advanced\"") != std::string::npos);
        REQUIRE(output.find("\"use_notes\"") != std::string::npos);
        REQUIRE(output.find("\"order\": [\"Project\"]") != std::string::npos);
        REQUIRE(output.find("\"Project\": [") != std::string::npos);
        REQUIRE(output.find("\"choices\"") == std::string::npos);
        REQUIRE(output.find("\"optional\": false") == std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"m6_advanced", "-q", "--json", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"choices\": []") != std::string::npos);
        REQUIRE(output.find("\"optional\": false") != std::string::npos);
        REQUIRE(output.find("\"help\": \"\"") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"m6_advanced", "-q", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("[Project]") != std::string::npos);
        REQUIRE(output.find("choices: []") != std::string::npos);
        REQUIRE(output.find("optional: no") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_questions_outputs_sensitive_metadata_for_template_path) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-sensitive-q-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-sensitive-q-data-home");
    const std::filesystem::path template_root = create_sensitive_template(workspace.path());
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({template_root.string(), "-q", "--json", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"api_token\"") != std::string::npos);
        REQUIRE(output.find("\"sensitive\": true") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({template_root.string(), "-q", "--full"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("sensitive: yes") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_render_with_direct_template_path_generates_expected_output) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-path-render-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-path-render-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        std::filesystem::absolute("templates/basic_cpp").string(),
        target.path().string(),
        "--set", "project_name=Path App",
        "--set", "name_slug=path-app",
        "--set", "namespace=path_ns",
        "--set", "include_ci=false",
        "--set", "author=Path Tester",
    });

    REQUIRE_EQ(result, 0);
    REQUIRE(read_text_file(target.path() / "README.md").find("# Path App") != std::string::npos);
    REQUIRE(read_text_file(target.path() / ".tempify-summary.txt").find("slug=path-app") != std::string::npos);
}

TEST_CASE(TempifyApp_overwrite_if_exists_replaces_conflicting_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-overwrite-if-exists-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-overwrite-if-exists-data-home");
    std::filesystem::create_directories(target.path());
    {
        std::ofstream output(target.path() / "README.md", std::ios::binary);
        output << "old readme\n";
    }
    {
        std::ofstream output(target.path() / "keep.txt", std::ios::binary);
        output << "keep\n";
    }

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "-f",
        "--set", "project_name=Overwrite App",
        "--set", "name_slug=overwrite-app",
        "--set", "namespace=overwrite_ns",
        "--set", "include_ci=false",
        "--set", "author=Overwrite Tester",
    }), 0);

    REQUIRE(read_text_file(target.path() / "README.md").find("# Overwrite App") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "keep.txt") == std::string("keep\n"));
}

TEST_CASE(TempifyApp_skip_if_file_exists_preserves_conflicting_files_and_generates_missing_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-skip-if-file-exists-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-skip-if-file-exists-data-home");
    std::filesystem::create_directories(target.path());
    {
        std::ofstream output(target.path() / "README.md", std::ios::binary);
        output << "custom readme\n";
    }

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "-s",
        "--set", "project_name=Skip App",
        "--set", "name_slug=skip-app",
        "--set", "namespace=skip_ns",
        "--set", "include_ci=false",
        "--set", "author=Skip Tester",
    }), 0);

    REQUIRE(read_text_file(target.path() / "README.md") == std::string("custom readme\n"));
    REQUIRE(std::filesystem::exists(target.path() / "src" / "main.cpp"));
}

TEST_CASE(TempifyApp_no_hooks_skips_hook_side_effects) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-no-hooks-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-no-hooks-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "m6_advanced",
        target.path().string(),
        "--accept-hooks", "no",
        "--set", "project_name=No Hooks",
        "--set", "project_slug=no-hooks",
        "--set", "use_notes=false",
    });

    REQUIRE_EQ(result, 0);
    REQUIRE(std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(std::filesystem::exists(target.path() / "static" / "output.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "pre.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "before-render.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "after-render.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "post.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "script-marker.txt"));
}

TEST_CASE(TempifyApp_accept_hooks_ask_without_tty_runs_hooks) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-accept-hooks-ask-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-accept-hooks-ask-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "m6_advanced",
        target.path().string(),
        "--accept-hooks", "ask",
        "--set", "project_name=Ask Hooks",
        "--set", "project_slug=ask-hooks",
        "--set", "use_notes=false",
    });

    REQUIRE_EQ(result, 0);
    REQUIRE(std::filesystem::exists(target.path() / "pre.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "before-render.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "after-render.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "post.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "script-marker.txt"));
}

TEST_CASE(TempifyApp_accept_hooks_ask_tty_shows_summary_and_persists_trust) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-hook-trust-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-hook-trust-data-home");
    prebyte::test::ScopedEnvironmentVariable force_prompt("TEMPIFY_FORCE_HOOK_PROMPT", "1");
    tempify::TempifyApp app;

    {
        ScopedStdinCapture input("yes\n");
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({
            "m6_advanced",
            target.path().string(),
            "--accept-hooks", "ask",
            "--set", "project_name=Trusted Hooks",
            "--set", "project_slug=trusted-hooks",
            "--set", "use_notes=false",
        }), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("Template defines hooks:") != std::string::npos);
        REQUIRE(output.find("- pre: ") != std::string::npos);
        REQUIRE(output.find("- before_render: ") != std::string::npos);
        REQUIRE(output.find("- after_render: ") != std::string::npos);
        REQUIRE(output.find("- post: ") != std::string::npos);
        REQUIRE(output.find("Run hooks and trust this template next time? [Y/n]: ") != std::string::npos);
    }

    const std::filesystem::path trust_store = data_home.shared_root() / "trust" / "hooks.json";
    REQUIRE(std::filesystem::is_regular_file(trust_store));
    REQUIRE(read_text_file(trust_store).find(std::filesystem::weakly_canonical(std::filesystem::path("templates/m6_advanced")).string()) != std::string::npos);

    {
        ScopedDirectoryCleanup second_target(std::filesystem::temp_directory_path() / "tempify-app-hook-trust-target-2");
        ScopedStdinCapture input("");
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({
            "m6_advanced",
            second_target.path().string(),
            "--accept-hooks", "ask",
            "--set", "project_name=Trusted Hooks Again",
            "--set", "project_slug=trusted-hooks-again",
            "--set", "use_notes=false",
            "--tui",
        }), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("Template hooks trusted from previous approval.") != std::string::npos);
        REQUIRE(output.find("Run hooks and trust this template next time?") == std::string::npos);
    }
}

TEST_CASE(TempifyApp_questions_unknown_template_throws) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-q-missing-data-home");
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(
        app.run({"definitely_missing_template_12345", "-q"}),
        tempify::TempifyError);
}

TEST_CASE(TempifyApp_render_unknown_template_throws) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-render-missing-data-home");
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(
        app.run({"definitely_missing_template_67890", "out-dir"}),
        tempify::TempifyError);
}

TEST_CASE(TempifyApp_render_existing_target_without_overwrite_throws) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-existing-target-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-existing-target-data-home");
    std::filesystem::create_directories(target.path());
    {
        std::ofstream output(target.path() / "keep.txt", std::ios::binary);
        output << "keep\n";
    }

    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(
        app.run({
            "basic_cpp",
            target.path().string(),
            "--set", "project_name=Existing Target",
            "--set", "name_slug=existing-target",
            "--set", "namespace=existing_ns",
            "--set", "include_ci=false",
            "--set", "author=Keeper",
        }),
        tempify::TempifyError);
}

TEST_CASE(TempifyApp_refresh_creates_shared_index_without_templates) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-refresh-empty-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"refresh"}), 0);
    REQUIRE(capture.str().find("Refreshed 0 shared templates") != std::string::npos);
    REQUIRE(std::filesystem::is_regular_file(data_home.shared_root() / "index" / "templates.json"));
}

TEST_CASE(TempifyApp_help_version_and_list_write_expected_stdout) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-help-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("Usage:") != std::string::npos);
        REQUIRE(output.find("Options:") != std::string::npos);
        REQUIRE(output.find("Commands:") != std::string::npos);
        REQUIRE(output.find("Examples:") != std::string::npos);
        REQUIRE(output.find("tempify <command> [args...] [options]") != std::string::npos);
        REQUIRE(output.find("tempify <template-id> [target] [options]") != std::string::npos);
        REQUIRE(output.find("tempify reapply <template-id> <target> [options]") != std::string::npos);
        REQUIRE(output.find("-q") == std::string::npos);
        REQUIRE(output.find("--questions") == std::string::npos);
        REQUIRE(output.find("process") != std::string::npos);
        REQUIRE(output.find("info") != std::string::npos);
        REQUIRE(output.find("doctor") != std::string::npos);
        REQUIRE(output.find("completion") != std::string::npos);
        REQUIRE(output.find("validate") != std::string::npos);
        REQUIRE(output.find("inspect") != std::string::npos);
        REQUIRE(output.find("lint") != std::string::npos);
        REQUIRE(output.find("test") != std::string::npos);
        REQUIRE(output.find("Passes remaining args to embedded Prebyte") != std::string::npos);
        REQUIRE(output.find("tempify reapply basic_cpp my-app --report --json") != std::string::npos);
        REQUIRE(output.find("-q       Shows question overview for template") == std::string::npos);
        REQUIRE(output.find("Run 'tempify <command> -h' for command-specific help.") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"--version"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("0.1.0") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("basic_cpp") != std::string::npos);
        REQUIRE(output.find("m6_advanced") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_help_and_version_aliases_work) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-help-alias-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"help"}), 0);
        REQUIRE(capture.str().find("Usage:") != std::string::npos);
    }
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"-h"}), 0);
        REQUIRE(capture.str().find("Usage:") != std::string::npos);
    }
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"version"}), 0);
        REQUIRE(capture.str().find("0.1.0") != std::string::npos);
    }
    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"-v"}), 0);
        REQUIRE(capture.str().find("0.1.0") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_command_and_render_help_pages_work) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-command-help-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify list - List Available Templates") != std::string::npos);
        REQUIRE(output.find("workspace templates") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"info", "basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify info - Show Template Details") != std::string::npos);
        REQUIRE(output.find("tempify info <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"doctor", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify doctor - Inspect Tempify Environment") != std::string::npos);
        REQUIRE(output.find("tempify doctor") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify completion - Generate Shell Completion Script") != std::string::npos);
        REQUIRE(output.find("tempify completion <shell>") != std::string::npos);
        REQUIRE(output.find("bash, zsh, fish") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"validate", "basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify validate - Validate Template Structure") != std::string::npos);
        REQUIRE(output.find("tempify validate <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"inspect", "m3_product", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify inspect - Inspect Merged Template Graph") != std::string::npos);
        REQUIRE(output.find("tempify inspect <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"lint", "m3_product", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify lint - Lint Template Quality") != std::string::npos);
        REQUIRE(output.find("tempify lint <template-id>") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"test", "basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify test - Run Template Fixtures") != std::string::npos);
        REQUIRE(output.find("tempify test <template-id>") != std::string::npos);
        REQUIRE(output.find("answers.json") != std::string::npos);
        REQUIRE(output.find("lock.json") != std::string::npos);
        REQUIRE(output.find("--list-fixtures") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
        REQUIRE(output.find("snapshot/") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"refresh", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify refresh - Rebuild Shared Template Index") != std::string::npos);
        REQUIRE(output.find("index/templates.json") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"process", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify process - Embedded Prebyte Passthrough") != std::string::npos);
        REQUIRE(output.find("tempify process [prebyte-args...]") != std::string::npos);
        REQUIRE(output.find("tempify -p [prebyte-args...]") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "-q", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify -q|--questions - Show Template Questions") != std::string::npos);
        REQUIRE(output.find("tempify <template-id> -q|--questions [options]") != std::string::npos);
        REQUIRE(output.find("--json") != std::string::npos);
        REQUIRE(output.find("--full") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "--questions", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify -q|--questions - Show Template Questions") != std::string::npos);
        REQUIRE(output.find("tempify <template-id> -q|--questions [options]") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify render - Generate Template Output") != std::string::npos);
        REQUIRE(output.find("Template:") != std::string::npos);
        REQUIRE(output.find("basic_cpp") != std::string::npos);
        REQUIRE(output.find("-q,--questions") != std::string::npos);
        REQUIRE(output.find("instead of rendering") != std::string::npos);
        REQUIRE(output.find("-f,--overwrite-if-exists") != std::string::npos);
        REQUIRE(output.find("-s,--skip-if-file-exists") != std::string::npos);
        REQUIRE(output.find("--accept-hooks <yes|ask|no>") != std::string::npos);
        REQUIRE(output.find("--no-hooks") != std::string::npos);
        REQUIRE(output.find("--set <key=value>") != std::string::npos);
        REQUIRE(output.find("--answers <path>") != std::string::npos);
        REQUIRE(output.find("--write-answers <path>") != std::string::npos);
        REQUIRE(output.find("--non-interactive") != std::string::npos);
        REQUIRE(output.find("--strict") != std::string::npos);
        REQUIRE(output.find("tempify basic_cpp existing-dir --reapply --json") != std::string::npos);
        REQUIRE(output.find("--dry-run") != std::string::npos);
        REQUIRE(output.find("--plan-json") != std::string::npos);
        REQUIRE(output.find("--tui") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"basic_cpp", "out-dir", "--help"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify <template-id> [target] [options]") != std::string::npos);
        REQUIRE(output.find("Output directory") != std::string::npos);
        REQUIRE(output.find("Generated basic_cpp") == std::string::npos);
        REQUIRE(output.find("tempify reapply basic_cpp existing-dir --json") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"reapply", "basic_cpp", "out-dir", "-h"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("tempify render - Generate Template Output") != std::string::npos);
        REQUIRE(output.find("Template:") != std::string::npos);
        REQUIRE(output.find("basic_cpp") != std::string::npos);
        REQUIRE(output.find("tempify reapply <template-id> <target>") != std::string::npos);
    }

    {
        REQUIRE_THROWS_AS(app.run({"basic_cpp", "out-dir", "-q"}), tempify::TempifyError);
    }

    {
        REQUIRE_THROWS_AS(app.run({"basic_cpp", "out-dir", "--questions"}), tempify::TempifyError);
    }
}

TEST_CASE(TempifyApp_info_outputs_template_details) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-info-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"info", "basic_cpp"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("basic_cpp (Basic C++ App)") != std::string::npos);
    REQUIRE(output.find("Description: Small C++ starter built through Tempify and Prebyte") != std::string::npos);
    REQUIRE(output.find("Includes: 1 (base_cpp_common)") != std::string::npos);
    REQUIRE(output.find("Questions: 6") != std::string::npos);
}

TEST_CASE(TempifyApp_list_json_outputs_machine_readable_catalog) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-list-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"list", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"total\": ") != std::string::npos);
    REQUIRE(output.find("\"templates\": [") != std::string::npos);
    REQUIRE(output.find("\"id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"id\": \"m6_advanced\"") != std::string::npos);
}

TEST_CASE(TempifyApp_info_json_outputs_machine_readable_details) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-info-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"info", "basic_cpp", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"name\": \"Basic C++ App\"") != std::string::npos);
    REQUIRE(output.find("\"include_ids\": [") != std::string::npos);
    REQUIRE(output.find("\"base_cpp_common\"") != std::string::npos);
    REQUIRE(output.find("\"question_count\": 6") != std::string::npos);
    REQUIRE(output.find("\"hooks\": {") != std::string::npos);
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
    REQUIRE(output.find((config_home.path() / "tempify" / "config.json").string()) != std::string::npos);
    REQUIRE(output.find("\"workspace_config\": ") != std::string::npos);
    REQUIRE(output.find((workspace.path() / ".tempify" / "config.json").string()) != std::string::npos);
    REQUIRE(output.find("\"shared_templates_root\": ") != std::string::npos);
    REQUIRE(output.find("\"shared_index_exists\": ") != std::string::npos);
    REQUIRE(output.find("\"shared_index_status\": \"ok\"") != std::string::npos);
    REQUIRE(output.find("\"catalog_status\": ") != std::string::npos);
}

TEST_CASE(TempifyApp_completion_outputs_shell_scripts) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-completion-data-home");
    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "bash"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("complete -F _tempify tempify") != std::string::npos);
        REQUIRE(output.find("tempify list 2>/dev/null | cut -f1") != std::string::npos);
        REQUIRE(output.find("reapply") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "zsh"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("#compdef tempify") != std::string::npos);
        REQUIRE(output.find("compdef _tempify tempify") != std::string::npos);
        REQUIRE(output.find("reapply") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"completion", "fish"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("complete -c tempify -f") != std::string::npos);
        REQUIRE(output.find("__fish_use_subcommand") != std::string::npos);
        REQUIRE(output.find("reapply") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_answers_file_round_trip_supports_non_interactive_render) {
    ScopedDirectoryCleanup answers_root(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-root");
    ScopedDirectoryCleanup first_target(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-first");
    ScopedDirectoryCleanup second_target(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-second");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-answers-roundtrip-data-home");
    const std::filesystem::path answers_file = answers_root.path() / "answers.json";

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        first_target.path().string(),
        "--set", "project_name=Round Trip App",
        "--set", "name_slug=round-trip-app",
        "--set", "namespace=round_trip_ns",
        "--set", "include_ci=false",
        "--set", "author=Round Trip Tester",
        "--write-answers", answers_file.string(),
    }), 0);

    const std::string answer_text = read_text_file(answers_file);
    REQUIRE(answer_text.find("project_name") != std::string::npos);
    REQUIRE(answer_text.find("author") == std::string::npos);
    REQUIRE(answer_text.find("name_slug") == std::string::npos);

    REQUIRE_EQ(app.run({
        "basic_cpp",
        second_target.path().string(),
        "--answers", answers_file.string(),
        "--set", "author=Round Trip Tester",
        "--non-interactive",
        "--strict",
    }), 0);

    REQUIRE(read_text_file(second_target.path() / "README.md").find("# Round Trip App") != std::string::npos);
    REQUIRE(read_text_file(second_target.path() / ".tempify-summary.txt").find("slug=round-trip-app") != std::string::npos);
}

TEST_CASE(TempifyApp_non_interactive_missing_required_answer_throws) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-non-interactive-required-template");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-non-interactive-missing");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-non-interactive-missing-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(
        app.run({
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
    REQUIRE_THROWS_AS(
        app.run({
            "basic_cpp",
            target.path().string(),
            "--answers", answers_file.string(),
            "--set", "author=Strict Tester",
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
    std::filesystem::create_directory_symlink(std::filesystem::absolute("templates"), workspace.path() / "templates");
    std::filesystem::create_directories(workspace.path() / ".tempify");
    write_text_file(config_home.path() / "tempify" / "config.json",
                    "{\n"
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
    write_text_file(workspace.path() / ".tempify" / "config.json",
                    "{\n"
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
    write_text_file(answers_file,
                    "{\n"
                    "  \"project_name\": \"Answer App\",\n"
                    "  \"namespace\": \"answer_ns\"\n"
                    "}\n");

    ScopedCurrentPath cwd(workspace.path());
    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
        "basic_cpp",
        target.path().string(),
        "--answers", answers_file.string(),
        "--set", "author=CLI Author",
        "--non-interactive",
        "--strict",
    }), 0);

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

TEST_CASE(TempifyApp_dry_run_outputs_plan_and_writes_no_files) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-dry-run-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-dry-run-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({
        "m6_advanced",
        target.path().string(),
        "--set", "project_name=Dry Run App",
        "--set", "project_slug=dry-run-app",
        "--set", "use_notes=false",
        "--dry-run",
    }), 0);

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
        "--set", "project_name=Plan App",
        "--set", "name_slug=plan-app",
        "--set", "namespace=plan_ns",
        "--set", "include_ci=false",
        "--set", "author=Planner",
        "--plan-json",
    }), 0);

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
        "--set", "project_name=Lock App",
        "--set", "name_slug=lock-app",
        "--set", "namespace=lock_ns",
        "--set", "include_ci=false",
        "--set", "author=Locker",
    }), 0);

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
        "--set", "project_name=Secure App",
        "--set", "api_token=super-secret-token",
    }), 0);

    const std::string lock = read_text_file(target.path() / ".tempify-lock.json");
    REQUIRE(lock.find("\"api_token\": \"<redacted>\"") != std::string::npos);
    REQUIRE(lock.find("super-secret-token") == std::string::npos);
    REQUIRE(lock.find("\"project_name\": \"Secure App\"") != std::string::npos);
}

TEST_CASE(TempifyApp_render_hook_timeout_flag_aborts_slow_hook_with_phase_diagnostics) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-hook-timeout-workspace");
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-hook-timeout-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-hook-timeout-data-home");
    const std::filesystem::path template_root = create_slow_hook_template(workspace.path());
    tempify::TempifyApp app;

    try {
        static_cast<void>(app.run({
            template_root.string(),
            target.path().string(),
            "--hook-timeout-ms", "25",
            "--set", "project_name=Slow App",
        }));
        REQUIRE(false);
    } catch (const tempify::TempifyError& error) {
        REQUIRE(std::string(error.what()).find("Hook phase 'post' failed") != std::string::npos);
        REQUIRE(std::string(error.what()).find("post.lua") != std::string::npos);
        REQUIRE(std::string(error.what()).find("timed out after 25 ms") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_validate_accepts_valid_template) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-valid-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"validate", "basic_cpp"}), 0);
    REQUIRE(capture.str().find("Validated basic_cpp -> OK") != std::string::npos);
}

TEST_CASE(TempifyApp_validate_json_outputs_machine_readable_result) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"validate", "basic_cpp", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"ok\"") != std::string::npos);
}

TEST_CASE(TempifyApp_validate_rejects_duplicate_alias_conflict) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-validate-alias-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-alias-data-home");
    const std::filesystem::path template_path = create_duplicate_alias_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"validate", template_path.string()}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_validate_rejects_missing_layout_source) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-validate-layout-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-validate-layout-data-home");
    const std::filesystem::path template_path = create_bad_layout_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"validate", template_path.string()}), tempify::TempifyError);
}

TEST_CASE(TempifyApp_inspect_outputs_merge_and_provenance_details) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-inspect-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"inspect", "m3_product"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("m3_product (M3 Product)") != std::string::npos);
    REQUIRE(output.find("Source Roots:") != std::string::npos);
    REQUIRE(output.find("m3_lang_base") != std::string::npos);
    REQUIRE(output.find("m3_ci_layer") != std::string::npos);
    REQUIRE(output.find("src/main.cpp.pbt <- m3_product") != std::string::npos);
    REQUIRE(output.find("README.md.pbt <- m3_ci_layer") != std::string::npos);
    REQUIRE(output.find("Questions:") != std::string::npos);
}

TEST_CASE(TempifyApp_inspect_json_outputs_machine_readable_report) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-inspect-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"inspect", "m3_product", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"m3_product\"") != std::string::npos);
    REQUIRE(output.find("\"source_roots\": [") != std::string::npos);
    REQUIRE(output.find("\"include_ids\": [") != std::string::npos);
    REQUIRE(output.find("\"files\": [") != std::string::npos);
    REQUIRE(output.find("\"questions\": [") != std::string::npos);
    REQUIRE(output.find("\"layout_rules\": [") != std::string::npos);
    REQUIRE(output.find("\"hooks\": {") != std::string::npos);
}

TEST_CASE(TempifyApp_inspect_json_includes_sensitive_question_metadata) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-app-sensitive-inspect-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-sensitive-inspect-data-home");
    const std::filesystem::path template_root = create_sensitive_template(workspace.path());
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"inspect", template_root.string(), "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"api_token\"") != std::string::npos);
    REQUIRE(output.find("\"sensitive\": true") != std::string::npos);
}

TEST_CASE(TempifyApp_lint_outputs_authoring_warnings) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lint-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"lint", "m3_product"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Lint m3_product") != std::string::npos);
    REQUIRE(output.find("Warnings:") != std::string::npos);
    REQUIRE(output.find("question missing prompt") != std::string::npos);
}

TEST_CASE(TempifyApp_lint_json_outputs_machine_readable_report) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lint-json-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"lint", "m3_product", "--json"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("\"template_id\": \"m3_product\"") != std::string::npos);
    REQUIRE(output.find("\"warning_count\": ") != std::string::npos);
    REQUIRE(output.find("\"warnings\": [") != std::string::npos);
    REQUIRE(output.find("question missing prompt") != std::string::npos);
}

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
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-list-fixtures-data-home");
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
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-test-json-fail-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-json-fail-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "broken_case" / "answers.json",
                    "{\n  \"required_name\": \"Mismatch App\"\n}\n");
    write_text_file(template_path / "tests" / "broken_case" / "snapshot" / "README.md",
                    "# Wrong App\n");

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
    write_text_file(template_path / "tests" / "refresh_case" / "snapshot" / "README.md",
                    "# Old App\n");

    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", template_path.string(), "--update-snapshots"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("PASS refresh_case (1 files)") != std::string::npos);
    REQUIRE(read_text_file(template_path / "tests" / "refresh_case" / "snapshot" / "README.md") == std::string("# Updated App\n"));
}

TEST_CASE(TempifyApp_test_update_snapshots_rewrites_lock_snapshot_when_present) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-test-update-lock-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-update-lock-data-home");
    const std::filesystem::path template_path = create_required_only_template(template_root.path());
    write_text_file(template_path / "tests" / "refresh_lock" / "answers.json",
                    "{\n  \"required_name\": \"Lock App\"\n}\n");
    write_text_file(template_path / "tests" / "refresh_lock" / "snapshot" / "README.md",
                    "# stale\n");
    write_text_file(template_path / "tests" / "refresh_lock" / "lock.json",
                    "{\n  \"generated_at\": \"stale\"\n}\n");

    tempify::TempifyApp app;

    REQUIRE_EQ(app.run({"test", template_path.string(), "--update-snapshots"}), 0);
    const std::string lock_text = read_text_file(template_path / "tests" / "refresh_lock" / "lock.json");
    REQUIRE(lock_text.find("<generated-at>") != std::string::npos);
    REQUIRE(lock_text.find("<build-root>") != std::string::npos);
}

TEST_CASE(TempifyApp_test_fixture_flag_rejects_unknown_fixture) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-filtered-missing-data-home");
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

    REQUIRE_EQ(app.run({"test", "m3_product"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Test m3_product") != std::string::npos);
    REQUIRE(output.find("PASS ci_enabled_layered (3 files)") != std::string::npos);
    REQUIRE(output.find("1/1 fixtures passed") != std::string::npos);
    REQUIRE(output.find("3 snapshot artifacts") != std::string::npos);
}

TEST_CASE(TempifyApp_test_runs_hook_heavy_fixture_with_lock_snapshot) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-test-hooks-lock-data-home");
    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", "m6_advanced"}), 0);
    const std::string output = capture.str();
    REQUIRE(output.find("Test m6_advanced") != std::string::npos);
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
    write_text_file(template_path / "tests" / "broken_case" / "snapshot" / "README.md",
                    "# Wrong App\n");
    write_text_file(template_path / "tests" / "passing_case" / "answers.json",
                    "{\n  \"required_name\": \"Passing App\"\n}\n");
    write_text_file(template_path / "tests" / "passing_case" / "snapshot" / "README.md",
                    "# Passing App\n");

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
    write_text_file(template_path / "tests" / "wrong_files" / "snapshot" / "expected-only.txt",
                    "only in snapshot\n");

    tempify::TempifyApp app;
    ScopedStdoutCapture capture;

    REQUIRE_EQ(app.run({"test", template_path.string()}), 1);
    const std::string output = capture.str();
    REQUIRE(output.find("FAIL wrong_files") != std::string::npos);
    REQUIRE(output.find("missing [expected-only.txt]") != std::string::npos);
    REQUIRE(output.find("unexpected [README.md") != std::string::npos);
    REQUIRE(output.find("0/1 fixtures passed, 1 failed") != std::string::npos);
}

TEST_CASE(TempifyApp_lint_still_fails_invalid_template_structure) {
    ScopedDirectoryCleanup template_root(std::filesystem::temp_directory_path() / "tempify-app-lint-invalid-template");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-lint-invalid-data-home");
    const std::filesystem::path template_path = create_duplicate_alias_template(template_root.path());
    tempify::TempifyApp app;

    REQUIRE_THROWS_AS(app.run({"lint", template_path.string()}), tempify::TempifyError);
}
