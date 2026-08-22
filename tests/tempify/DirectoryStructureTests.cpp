#include "TempifyAppTestSupport.h"
#include "TestHarness.h"
#include "tempify/build/BuildExecutor.h"
#include "tempify/build/BuildPlanner.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/template/TemplateLoader.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace {

using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::write_text_file;

class ScopedDirectoryCleanupLocal {
  public:
    explicit ScopedDirectoryCleanupLocal(std::filesystem::path path) : path_(std::move(path)) {
        std::filesystem::remove_all(path_);
    }

    ~ScopedDirectoryCleanupLocal() {
        std::filesystem::remove_all(path_);
    }

    const std::filesystem::path &path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

std::string read_text_file(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::filesystem::path create_directory_layout_template(const std::filesystem::path &root) {
    const std::filesystem::path template_root = root / "directory_layout";
    std::filesystem::create_directories(template_root / "files" / "empty_keep");
    std::filesystem::create_directories(template_root / "files" / "deep" / "nested");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"directory_layout\",\n"
                    "  name = \"Directory Layout\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ project_slug }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"project_name\", type = \"string\", prompt = \"Project name\" },\n"
                    "      { key = \"project_slug\", type = \"string\", prompt = \"Project slug\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n");
    write_text_file(template_root / "files" / "deep" / "nested" / "leaf.txt.pbt", "leaf={{ project_slug }}\n");
    return template_root;
}

tempify::TemplateManifest make_flat_manifest(const std::filesystem::path &template_root) {
    tempify::TemplateManifest manifest;
    manifest.root = template_root;
    manifest.info.id = "directory_exec";
    manifest.source_roots.push_back({
        .path = template_root / "files",
        .template_id = "directory_exec",
    });
    return manifest;
}

} // namespace

TEST_CASE(BuildPlanner_collects_parent_and_empty_template_directories_in_plan) {
    ScopedDirectoryCleanupLocal workspace(std::filesystem::temp_directory_path() / "tempify-build-planner-directory-plan");
    const std::filesystem::path template_root = create_directory_layout_template(workspace.path());

    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    const tempify::TemplateManifest manifest =
        loader.load(template_root, {{"directory_layout", template_root}});

    bool has_empty_keep = false;
    for (const auto &directory : manifest.directories) {
        if (directory.relative_path == "empty_keep") {
            has_empty_keep = true;
        }
    }
    REQUIRE(has_empty_keep);

    const tempify::PrebyteRenderer renderer;
    const tempify::BuildPlanner planner(renderer);
    const std::filesystem::path target = workspace.path() / "planned-out";
    const tempify::BuildPlan plan =
        planner.plan(manifest, {{"project_name", "Dir Plan"}, {"project_slug", "dir-plan"}}, target);

    REQUIRE(plan.build_root == target.lexically_normal());
    REQUIRE(std::filesystem::exists(plan.build_root) == false);

    bool has_build_root = false;
    bool has_empty_keep_dir = false;
    bool has_deep_parent = false;
    for (const auto &directory : plan.directories) {
        if (directory == plan.build_root) {
            has_build_root = true;
        }
        if (directory.filename() == "empty_keep") {
            has_empty_keep_dir = true;
        }
        if (directory.filename() == "nested" && directory.parent_path().filename() == "deep") {
            has_deep_parent = true;
        }
    }

    REQUIRE(has_build_root);
    REQUIRE(has_empty_keep_dir);
    REQUIRE(has_deep_parent);
    REQUIRE_EQ(plan.files.size(), static_cast<std::size_t>(2));
}

TEST_CASE(BuildExecutor_creates_build_root_and_nested_output_directories) {
    ScopedDirectoryCleanupLocal workspace(std::filesystem::temp_directory_path() /
                                          "tempify-build-executor-directory-create");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "brand" / "new" / "out";
    const std::filesystem::path source_file = template_root / "files" / "deep" / "nested.txt";
    write_text_file(source_file, "nested content\n");

    tempify::TemplateManifest manifest = make_flat_manifest(template_root);

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root, build_root / "deep"};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "deep" / "nested.txt",
            .render_with_prebyte = false,
        },
    };

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    REQUIRE(!std::filesystem::exists(build_root.parent_path()));
    executor.execute(plan, manifest, {}, true);

    REQUIRE(std::filesystem::is_directory(build_root));
    REQUIRE(std::filesystem::is_directory(build_root / "deep"));
    REQUIRE_EQ(read_text_file(build_root / "deep" / "nested.txt"), std::string("nested content\n"));
}

TEST_CASE(BuildExecutor_rejects_build_root_that_exists_as_file) {
    ScopedDirectoryCleanupLocal workspace(std::filesystem::temp_directory_path() /
                                          "tempify-build-executor-build-root-file");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "ok.txt";
    write_text_file(source_file, "ok\n");
    write_text_file(build_root, "target is a file\n");

    tempify::TemplateManifest manifest = make_flat_manifest(template_root);

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "ok.txt",
            .render_with_prebyte = false,
        },
    };

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    try {
        executor.execute(plan, manifest, {}, true);
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Target path exists as file and cannot be used as directory") !=
                std::string::npos);
        REQUIRE(std::string(error.what()).find("out") != std::string::npos);
    }
}

TEST_CASE(BuildExecutor_rejects_output_file_path_that_exists_as_directory) {
    ScopedDirectoryCleanupLocal workspace(std::filesystem::temp_directory_path() /
                                          "tempify-build-executor-file-directory-conflict");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "README.md";
    write_text_file(source_file, "readme\n");
    std::filesystem::create_directories(build_root);
    std::filesystem::create_directories(build_root / "README.md");

    tempify::TemplateManifest manifest = make_flat_manifest(template_root);

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Overwrite;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "README.md",
            .render_with_prebyte = false,
        },
    };

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    try {
        executor.execute(plan, manifest, {}, true);
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Output file path already exists as directory") != std::string::npos);
        REQUIRE(std::string(error.what()).find("README.md") != std::string::npos);
    }
}

TEST_CASE(LuaEngine_run_hook_mkdir_creates_nested_directories) {
    ScopedDirectoryCleanupLocal build_root(std::filesystem::temp_directory_path() /
                                           "tempify-lua-hook-nested-mkdir-test");
    std::filesystem::create_directories(build_root.path());

    tempify::LuaEngine lua_engine;
    const tempify::TemplateManifest manifest =
        lua_engine.load_partial_manifest(tempify::test_support::test_template_path("advanced_hooks_layout"));
    const tempify::PrebyteRenderer renderer;
    const tempify::BuildContext context{
        .template_root = manifest.root,
        .build_root = build_root.path(),
        .values = {{"project_name", "Nested Mkdir"}, {"project_slug", "nested-mkdir"}, {"use_notes", "false"}},
    };

    const std::filesystem::path hook_path = build_root.path() / "nested_mkdir.lua";
    write_text_file(hook_path,
                    "mkdir('nested/deep/leaf')\n"
                    "write_file('nested/deep/leaf/marker.txt', 'created')\n");

    lua_engine.run_hook(hook_path, manifest, context, renderer);

    REQUIRE(std::filesystem::is_directory(build_root.path() / "nested" / "deep" / "leaf"));
    REQUIRE_EQ(read_text_file(build_root.path() / "nested" / "deep" / "leaf" / "marker.txt"), std::string("created"));
}

TEST_CASE(TempifyApp_render_creates_nested_target_path_and_template_directories) {
    ScopedDirectoryCleanupLocal workspace(std::filesystem::temp_directory_path() /
                                          "tempify-app-directory-structure-workspace");
    ScopedDirectoryCleanup target_root(std::filesystem::temp_directory_path() /
                                       "tempify-app-directory-structure-target-root");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-directory-structure-data-home");

    const std::filesystem::path template_root = create_directory_layout_template(workspace.path());
    const std::filesystem::path target = target_root.path() / "deep" / "nested" / "render-target";
    REQUIRE(!std::filesystem::exists(target.parent_path()));

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({
                       template_root.string(),
                       target.string(),
                       "--set",
                       "project_name=Directory Structure",
                       "--set",
                       "project_slug=directory-structure",
                   }),
               0);

    REQUIRE(std::filesystem::is_directory(target));
    REQUIRE(std::filesystem::is_directory(target / "empty_keep"));
    REQUIRE(std::filesystem::is_directory(target / "deep" / "nested"));
    REQUIRE(std::filesystem::is_regular_file(target / "deep" / "nested" / "leaf.txt"));
    REQUIRE(read_text_file(target / "README.md").find("# Directory Structure") != std::string::npos);
    REQUIRE_EQ(read_text_file(target / "deep" / "nested" / "leaf.txt"), std::string("leaf=directory-structure\n"));
}

TEST_CASE(TempifyApp_render_rejects_target_path_that_exists_as_file) {
    ScopedDirectoryCleanupLocal workspace(std::filesystem::temp_directory_path() /
                                          "tempify-app-target-file-conflict-workspace");
    ScopedDirectoryCleanup target_parent(std::filesystem::temp_directory_path() /
                                         "tempify-app-target-file-conflict-parent");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-app-target-file-conflict-data-home");

    const std::filesystem::path template_root = create_directory_layout_template(workspace.path());
    const std::filesystem::path target = target_parent.path() / "not-a-directory";
    write_text_file(target, "already a file\n");

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({
                                template_root.string(),
                                target.string(),
                                "--set",
                                "project_name=Blocked Target",
                                "--set",
                                "project_slug=blocked-target",
                            }),
                            tempify::TempifyError);
}
