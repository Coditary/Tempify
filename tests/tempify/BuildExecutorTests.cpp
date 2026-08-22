#include "TestHarness.h"
#include "tempify/build/BuildExecutor.h"
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
    explicit ScopedDirectoryCleanup(std::filesystem::path path) : path_(std::move(path)) {
        std::filesystem::remove_all(path_);
    }

    ~ScopedDirectoryCleanup() {
        std::filesystem::remove_all(path_);
    }

    const std::filesystem::path &path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

void write_text_file(const std::filesystem::path &path, const std::string &text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string read_text_file(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::filesystem::path write_hook_script(const std::filesystem::path &path, const std::string &phase) {
    write_text_file(path, "local previous = \"\"\n"
                          "if exists(\"hook.log\") then\n"
                          "  previous = read_file(\"hook.log\")\n"
                          "end\n"
                          "write_file(\"hook.log\", previous .. \"" +
                              phase + "\\n\")\n");
    return path;
}

tempify::TemplateManifest make_manifest(const std::filesystem::path &template_root) {
    tempify::TemplateManifest manifest;
    manifest.root = template_root;
    manifest.info.id = "build_exec";
    manifest.source_roots.push_back({
        .path = template_root / "files",
        .template_id = "build_exec",
    });
    return manifest;
}

} // namespace

TEST_CASE(BuildExecutor_throws_when_target_exists_and_overwrite_disabled) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() /
                                     "tempify-build-executor-existing-target-test");
    write_text_file(workspace.path() / "existing" / "keep.txt", "keep\n");

    tempify::BuildPlan plan;
    plan.build_root = workspace.path() / "existing";
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;

    tempify::TemplateManifest manifest = make_manifest(workspace.path());
    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    REQUIRE_THROWS_AS(executor.execute(plan, manifest, {}, false), tempify::TempifyError);
}

TEST_CASE(BuildExecutor_runs_hooks_in_expected_order_and_renders_files) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-executor-hooks-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "hello.txt.pbt";
    write_text_file(source_file, "Hello {{ name }}\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);
    manifest.pre_hook_path = write_hook_script(template_root / "hooks" / "pre.lua", "pre");
    manifest.before_render_hook_path = write_hook_script(template_root / "hooks" / "before_render.lua", "before");
    manifest.after_render_hook_path = write_hook_script(template_root / "hooks" / "after_render.lua", "after");
    manifest.post_hook_path = write_hook_script(template_root / "hooks" / "post.lua", "post");

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "hello.txt",
            .render_with_prebyte = true,
        },
    };
    plan.pre_hook_path = manifest.pre_hook_path;
    plan.before_render_hook_path = manifest.before_render_hook_path;
    plan.after_render_hook_path = manifest.after_render_hook_path;
    plan.post_hook_path = manifest.post_hook_path;

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    executor.execute(plan, manifest, {{"name", "Stone"}}, false);

    REQUIRE_EQ(read_text_file(build_root / "hello.txt"), std::string("Hello Stone\n"));
    REQUIRE_EQ(read_text_file(build_root / "hook.log"), std::string("pre\nbefore\nafter\npost\n"));
}

TEST_CASE(BuildExecutor_disable_hooks_skips_hook_scripts_but_still_renders_files) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() /
                                     "tempify-build-executor-disable-hooks-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "hello.txt.pbt";
    write_text_file(source_file, "Hello {{ name }}\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);
    manifest.pre_hook_path = write_hook_script(template_root / "hooks" / "pre.lua", "pre");
    manifest.before_render_hook_path = write_hook_script(template_root / "hooks" / "before_render.lua", "before");
    manifest.after_render_hook_path = write_hook_script(template_root / "hooks" / "after_render.lua", "after");
    manifest.post_hook_path = write_hook_script(template_root / "hooks" / "post.lua", "post");

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "hello.txt",
            .render_with_prebyte = true,
        },
    };
    plan.pre_hook_path = manifest.pre_hook_path;
    plan.before_render_hook_path = manifest.before_render_hook_path;
    plan.after_render_hook_path = manifest.after_render_hook_path;
    plan.post_hook_path = manifest.post_hook_path;

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    executor.execute(plan, manifest, {{"name", "Stone"}}, true);

    REQUIRE_EQ(read_text_file(build_root / "hello.txt"), std::string("Hello Stone\n"));
    REQUIRE(!std::filesystem::exists(build_root / "hook.log"));
}

TEST_CASE(BuildExecutor_overwrite_true_allows_existing_target_and_copies_raw_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-executor-overwrite-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "raw.txt";
    write_text_file(source_file, "RAW {{ name }}\n");
    write_text_file(build_root / "raw.txt", "old\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Overwrite;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "raw.txt",
            .render_with_prebyte = false,
        },
    };

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    executor.execute(plan, manifest, {{"name", "Stone"}}, false);

    REQUIRE_EQ(read_text_file(build_root / "raw.txt"), std::string("RAW {{ name }}\n"));
}

TEST_CASE(BuildExecutor_skip_existing_files_preserves_old_content_and_writes_missing_files) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-executor-skip-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path kept_source = template_root / "files" / "keep.txt";
    const std::filesystem::path new_source = template_root / "files" / "new.txt";
    write_text_file(kept_source, "new keep\n");
    write_text_file(new_source, "new file\n");
    write_text_file(build_root / "keep.txt", "old keep\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Skip;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = kept_source,
            .output_path = build_root / "keep.txt",
            .render_with_prebyte = false,
        },
        tempify::PlannedFile{
            .source_path = new_source,
            .output_path = build_root / "new.txt",
            .render_with_prebyte = false,
        },
    };

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    executor.execute(plan, manifest, {}, false);

    REQUIRE_EQ(read_text_file(build_root / "keep.txt"), std::string("old keep\n"));
    REQUIRE_EQ(read_text_file(build_root / "new.txt"), std::string("new file\n"));
}

TEST_CASE(BuildExecutor_before_and_after_render_hooks_observe_expected_file_visibility) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-build-executor-visibility-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "hello.txt.pbt";
    write_text_file(source_file, "Hello {{ name }}\n");
    write_text_file(template_root / "hooks" / "before_render.lua",
                    "write_file('before.txt', exists('hello.txt') and 'yes' or 'no')\n");
    write_text_file(template_root / "hooks" / "after_render.lua",
                    "write_file('after.txt', exists('hello.txt') and 'yes' or 'no')\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);
    manifest.before_render_hook_path = template_root / "hooks" / "before_render.lua";
    manifest.after_render_hook_path = template_root / "hooks" / "after_render.lua";

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "hello.txt",
            .render_with_prebyte = true,
        },
    };
    plan.before_render_hook_path = manifest.before_render_hook_path;
    plan.after_render_hook_path = manifest.after_render_hook_path;

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    executor.execute(plan, manifest, {{"name", "Stone"}}, false);

    REQUIRE_EQ(read_text_file(build_root / "before.txt"), std::string("no"));
    REQUIRE_EQ(read_text_file(build_root / "after.txt"), std::string("yes"));
}

TEST_CASE(BuildExecutor_hook_failures_include_phase_name) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() /
                                     "tempify-build-executor-hook-failure-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "hello.txt.pbt";
    write_text_file(source_file, "Hello {{ name }}\n");
    write_text_file(template_root / "hooks" / "after_render.lua", "error('boom')\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);
    manifest.after_render_hook_path = template_root / "hooks" / "after_render.lua";

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "hello.txt",
            .render_with_prebyte = true,
        },
    };
    plan.after_render_hook_path = manifest.after_render_hook_path;

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    try {
        executor.execute(plan, manifest, {{"name", "Stone"}}, false);
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Hook phase 'after_render' failed") != std::string::npos);
        REQUIRE(std::string(error.what()).find("after_render.lua") != std::string::npos);
    }
}

TEST_CASE(BuildExecutor_hook_timeouts_include_phase_and_script_path) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() /
                                     "tempify-build-executor-hook-timeout-test");
    const std::filesystem::path template_root = workspace.path() / "template";
    const std::filesystem::path build_root = workspace.path() / "out";
    const std::filesystem::path source_file = template_root / "files" / "hello.txt.pbt";
    write_text_file(source_file, "Hello {{ name }}\n");
    write_text_file(template_root / "hooks" / "post.lua", "while true do end\n");

    tempify::TemplateManifest manifest = make_manifest(template_root);
    manifest.post_hook_path = template_root / "hooks" / "post.lua";

    tempify::BuildPlan plan;
    plan.build_root = build_root;
    plan.existing_path_behavior = tempify::ExistingPathBehavior::Error;
    plan.directories = {build_root};
    plan.files = {
        tempify::PlannedFile{
            .source_path = source_file,
            .output_path = build_root / "hello.txt",
            .render_with_prebyte = true,
        },
    };
    plan.post_hook_path = manifest.post_hook_path;

    const tempify::PrebyteRenderer renderer;
    const tempify::LuaEngine lua_engine;
    const tempify::BuildExecutor executor(renderer, lua_engine);

    try {
        executor.execute(plan, manifest, {{"name", "Stone"}}, false, std::chrono::milliseconds(25));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Hook phase 'post' failed") != std::string::npos);
        REQUIRE(std::string(error.what()).find("post.lua") != std::string::npos);
        REQUIRE(std::string(error.what()).find("timed out after 25 ms") != std::string::npos);
    }
}
