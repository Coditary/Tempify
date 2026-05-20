#include "TestHarness.h"
#include "TempifyTestSupport.h"

#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/support/Errors.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
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

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

}

TEST_CASE(LuaEngine_load_partial_manifest_loads_layout_scripts_env_and_hooks) {
    tempify::LuaEngine lua_engine;
    const tempify::TemplateManifest manifest = lua_engine.load_partial_manifest(tempify::test_support::test_template_path("advanced_hooks_layout"));

    REQUIRE_EQ(manifest.info.id, std::string("advanced_hooks_layout"));
    REQUIRE_EQ(manifest.layout_rules.size(), static_cast<std::size_t>(3));
    REQUIRE_EQ(manifest.scripts.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(manifest.scripts[0].name, std::string("create_marker"));
    REQUIRE(manifest.pre_hook_path.has_value());
    REQUIRE(manifest.before_render_hook_path.has_value());
    REQUIRE(manifest.after_render_hook_path.has_value());
    REQUIRE(manifest.post_hook_path.has_value());
    REQUIRE_EQ(manifest.question_group_order.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(manifest.question_group_order[0], std::string("Project"));
    REQUIRE_EQ(manifest.questions.size(), static_cast<std::size_t>(3));
}

TEST_CASE(LuaEngine_load_template_info_and_ignores_pbc_artifacts) {
    tempify::LuaEngine lua_engine;
    const tempify::TemplateInfo info = lua_engine.load_template_info(tempify::test_support::test_template_path("basic_cpp"));
    const tempify::TemplateManifest manifest = lua_engine.load_partial_manifest(tempify::test_support::test_template_path("basic_cpp"));

    REQUIRE_EQ(info.id, std::string("basic_cpp"));
    REQUIRE_EQ(info.name, std::string("Basic C++ App"));

    for (const auto& file : manifest.files) {
        REQUIRE(file.relative_path.ends_with(".pbc") == false);
    }
}

TEST_CASE(LuaEngine_run_hook_supports_file_ops_process_string_process_file_and_script_catalog) {
    ScopedDirectoryCleanup build_root(std::filesystem::temp_directory_path() / "tempify-lua-hook-runtime-test");
    std::filesystem::create_directories(build_root.path());

    tempify::LuaEngine lua_engine;
    const tempify::TemplateManifest manifest = lua_engine.load_partial_manifest(tempify::test_support::test_template_path("advanced_hooks_layout"));
    const tempify::PrebyteRenderer renderer;
    const tempify::BuildContext context{
        .template_root = manifest.root,
        .build_root = build_root.path(),
        .values = {{"project_name", "Hook Demo"}, {"project_slug", "hook-demo"}, {"use_notes", "false"}},
    };

    const std::filesystem::path hook_path = build_root.path() / "hook.lua";
    {
        std::ofstream output(hook_path, std::ios::binary);
        output
            << "mkdir('nested')\n"
            << "write_file('nested/raw.txt', process_string('Name={{ project_name }}'))\n"
            << "process_file('files/docs/readme.txt.pbt', 'nested/readme.md')\n"
            << "copy('files/raw/static.txt.pbt', 'nested/copied.txt')\n"
            << "script('create_marker')\n"
            << "if not exists('nested/readme.md') then error('missing readme') end\n"
            << "local files = list_files('nested')\n"
            << "write_file('nested/count.txt', tostring(#files))\n";
    }

    lua_engine.run_hook(hook_path, manifest, context, renderer);

    REQUIRE_EQ(read_text_file(build_root.path() / "nested" / "raw.txt"), std::string("Name=Hook Demo"));
    REQUIRE(read_text_file(build_root.path() / "nested" / "readme.md").find("# Hook Demo") != std::string::npos);
    REQUIRE(read_text_file(build_root.path() / "nested" / "copied.txt").find("This should stay raw: {{ project_name }}") != std::string::npos);
    REQUIRE_EQ(read_text_file(build_root.path() / "script-marker.txt"), std::string("script-ran\n"));
    REQUIRE_EQ(read_text_file(build_root.path() / "nested" / "count.txt"), std::string("3"));
}

TEST_CASE(LuaEngine_run_hook_unknown_script_throws) {
    ScopedDirectoryCleanup build_root(std::filesystem::temp_directory_path() / "tempify-lua-hook-missing-script-test");
    std::filesystem::create_directories(build_root.path());

    tempify::LuaEngine lua_engine;
    const tempify::TemplateManifest manifest = lua_engine.load_partial_manifest(tempify::test_support::test_template_path("advanced_hooks_layout"));
    const tempify::PrebyteRenderer renderer;
    const tempify::BuildContext context{
        .template_root = manifest.root,
        .build_root = build_root.path(),
        .values = {},
    };

    const std::filesystem::path hook_path = build_root.path() / "hook.lua";
    write_text_file(hook_path, "script('missing_script')\n");

    REQUIRE_THROWS_AS(lua_engine.run_hook(hook_path, manifest, context, renderer), tempify::TempifyError);
}

TEST_CASE(LuaEngine_run_hook_exec_respects_timeout) {
    ScopedDirectoryCleanup build_root(std::filesystem::temp_directory_path() / "tempify-lua-hook-timeout-test");
    std::filesystem::create_directories(build_root.path());

    tempify::LuaEngine lua_engine;
    const tempify::TemplateManifest manifest = lua_engine.load_partial_manifest(tempify::test_support::test_template_path("advanced_hooks_layout"));
    const tempify::PrebyteRenderer renderer;
    const tempify::BuildContext context{
        .template_root = manifest.root,
        .build_root = build_root.path(),
        .values = {},
    };

    const std::filesystem::path hook_path = build_root.path() / "hook.lua";
    write_text_file(hook_path, "exec('sleep 1')\n");

    try {
        lua_engine.run_hook(hook_path, manifest, context, renderer, std::chrono::milliseconds(25));
        REQUIRE(false);
    } catch (const tempify::TempifyError& error) {
        REQUIRE(std::string(error.what()).find("timed out after 25 ms") != std::string::npos);
        REQUIRE(std::string(error.what()).find("running command: sleep 1") != std::string::npos);
    }
}

TEST_CASE(LuaEngine_load_partial_manifest_throws_for_missing_source_dir) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-lua-missing-source-dir-test");
    write_text_file(
        workspace.path() / "template.lua",
        "return { id = 'broken_tpl', source_dir = 'files' }\n");

    tempify::LuaEngine lua_engine;
    REQUIRE_THROWS_AS(lua_engine.load_partial_manifest(workspace.path()), tempify::TempifyError);
}

TEST_CASE(LuaEngine_load_partial_manifest_throws_for_invalid_layout_and_questions_files) {
    ScopedDirectoryCleanup layout_workspace(std::filesystem::temp_directory_path() / "tempify-lua-invalid-layout-test");
    write_text_file(layout_workspace.path() / "template.lua", "return { id = 'layout_bad', source_dir = 'files' }\n");
    std::filesystem::create_directories(layout_workspace.path() / "files");
    write_text_file(layout_workspace.path() / "layout.lua", "return { 'bad' }\n");

    ScopedDirectoryCleanup question_workspace(std::filesystem::temp_directory_path() / "tempify-lua-invalid-questions-test");
    write_text_file(question_workspace.path() / "template.lua", "return { id = 'question_bad', source_dir = 'files' }\n");
    std::filesystem::create_directories(question_workspace.path() / "files");
    write_text_file(question_workspace.path() / "questions.lua", "return { order = { 'Project' }, groups = { Project = 'bad' } }\n");

    tempify::LuaEngine lua_engine;
    REQUIRE_THROWS_AS(lua_engine.load_partial_manifest(layout_workspace.path()), tempify::TempifyError);
    REQUIRE_THROWS_AS(lua_engine.load_partial_manifest(question_workspace.path()), tempify::TempifyError);
}
