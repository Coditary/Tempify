#include "TestHarness.h"

#include "tempify/build/BuildPlanner.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/support/Errors.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>

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

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

tempify::TemplateManifest make_manifest(std::string id) {
    tempify::TemplateManifest manifest;
    manifest.info.id = std::move(id);
    manifest.output_path_template = "generated";
    return manifest;
}

}

TEST_CASE(BuildPlanner_rejects_rendered_output_path_that_escapes_build_root) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-planner-escape-test");
    const std::filesystem::path source_file = workspace.path() / "source.txt";
    write_text_file(source_file, "content\n");

    tempify::TemplateManifest manifest = make_manifest("escape_case");
    manifest.files.push_back({
        .relative_path = "{{ output_path }}",
        .source_path = source_file,
        .render_with_prebyte = false,
        .source_template_id = "escape_case",
    });

    const tempify::PrebyteRenderer renderer;
    const tempify::BuildPlanner planner(renderer);

    REQUIRE_THROWS_AS(
        planner.plan(
            manifest,
            {{"output_path", "../escape.txt"}},
            workspace.path() / "out"),
        tempify::TempifyError);
}

TEST_CASE(BuildPlanner_rejects_empty_rendered_build_root) {
    tempify::TemplateManifest manifest = make_manifest("empty_root");
    manifest.output_path_template = "{{ build_name }}";

    const tempify::PrebyteRenderer renderer;
    const tempify::BuildPlanner planner(renderer);

    REQUIRE_THROWS_AS(
        planner.plan(manifest, {{"build_name", ""}}, std::nullopt),
        tempify::TempifyError);
}

TEST_CASE(BuildPlanner_detects_colliding_rendered_output_paths) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-planner-collision-test");
    const std::filesystem::path first_file = workspace.path() / "a.txt";
    const std::filesystem::path second_file = workspace.path() / "b.txt";
    write_text_file(first_file, "a\n");
    write_text_file(second_file, "b\n");

    tempify::TemplateManifest manifest = make_manifest("collision_case");
    manifest.files.push_back({
        .relative_path = "same.txt",
        .source_path = first_file,
        .render_with_prebyte = false,
        .source_template_id = "collision_case",
    });
    manifest.files.push_back({
        .relative_path = "{{ second_name }}",
        .source_path = second_file,
        .render_with_prebyte = false,
        .source_template_id = "collision_case",
    });

    const tempify::PrebyteRenderer renderer;
    const tempify::BuildPlanner planner(renderer);

    REQUIRE_THROWS_AS(
        planner.plan(
            manifest,
            {{"second_name", "same.txt"}},
            workspace.path() / "out"),
        tempify::TempifyError);
}

TEST_CASE(BuildPlanner_applies_layout_target_and_render_override) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-planner-layout-test");
    const std::filesystem::path source_file = workspace.path() / "raw.txt";
    write_text_file(source_file, "Hello {{ project_slug }}\n");

    tempify::TemplateManifest manifest = make_manifest("layout_case");
    manifest.files.push_back({
        .relative_path = "raw.txt",
        .source_path = source_file,
        .render_with_prebyte = false,
        .source_template_id = "layout_case",
    });
    manifest.layout_rules.push_back({
        .source = "raw.txt",
        .target = std::string("{{ project_slug }}.md.pbt"),
        .exclude = false,
        .render = true,
    });

    const tempify::PrebyteRenderer renderer;
    const tempify::BuildPlanner planner(renderer);
    const tempify::BuildPlan plan = planner.plan(
        manifest,
        {{"project_slug", "stone-app"}},
        workspace.path() / "out");

    REQUIRE_EQ(plan.files.size(), static_cast<std::size_t>(1));
    REQUIRE(plan.files[0].render_with_prebyte);
    REQUIRE_EQ(plan.files[0].output_path.filename().string(), std::string("stone-app.md"));
}

TEST_CASE(BuildPlanner_excludes_layout_marked_entries_and_keeps_pbt_extension_when_render_disabled) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-planner-exclude-test");
    const std::filesystem::path raw_file = workspace.path() / "raw.txt.pbt";
    const std::filesystem::path skip_file = workspace.path() / "skip.txt";
    write_text_file(raw_file, "Raw {{ value }}\n");
    write_text_file(skip_file, "skip\n");

    tempify::TemplateManifest manifest = make_manifest("exclude_case");
    manifest.directories.push_back({.relative_path = "skip_dir"});
    manifest.files.push_back({
        .relative_path = "raw.txt.pbt",
        .source_path = raw_file,
        .render_with_prebyte = true,
        .source_template_id = "exclude_case",
    });
    manifest.files.push_back({
        .relative_path = "skip.txt",
        .source_path = skip_file,
        .render_with_prebyte = false,
        .source_template_id = "exclude_case",
    });
    manifest.layout_rules.push_back({
        .source = "raw.txt.pbt",
        .target = std::string("copied.txt.pbt"),
        .exclude = false,
        .render = false,
    });
    manifest.layout_rules.push_back({
        .source = "skip.txt",
        .target = std::nullopt,
        .exclude = true,
        .render = std::nullopt,
    });
    manifest.layout_rules.push_back({
        .source = "skip_dir",
        .target = std::nullopt,
        .exclude = true,
        .render = std::nullopt,
    });

    const tempify::PrebyteRenderer renderer;
    const tempify::BuildPlanner planner(renderer);
    const tempify::BuildPlan plan = planner.plan(manifest, {{"value", "stone"}}, workspace.path() / "out");

    REQUIRE_EQ(plan.files.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(plan.files[0].output_path.filename().string(), std::string("copied.txt.pbt"));
    REQUIRE(!plan.files[0].render_with_prebyte);

    bool has_skip_dir = false;
    for (const auto& directory : plan.directories) {
        if (directory.filename() == "skip_dir") {
            has_skip_dir = true;
        }
    }
    REQUIRE(!has_skip_dir);
}
