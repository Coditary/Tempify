#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/support/Errors.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::test_template_path;
using tempify::test_support::write_text_file;

struct HookSandboxFixture {
    ScopedDirectoryCleanup sandbox_root;
    ScopedDirectoryCleanup build_root;
    ScopedDirectoryCleanup outside_root;

    HookSandboxFixture(const char *label)
        : sandbox_root(std::filesystem::temp_directory_path() / (std::string("tempify-hook-sandbox-") + label)),
          build_root(sandbox_root.path() / "build"), outside_root(sandbox_root.path() / "outside") {
        std::filesystem::create_directories(build_root.path());
        std::filesystem::create_directories(outside_root.path());
    }

    void run_hook_script(const std::string &script_body) const {
        const std::filesystem::path hook_path = build_root.path() / "hook.lua";
        write_text_file(hook_path, script_body);

        tempify::LuaEngine lua_engine;
        const tempify::TemplateManifest manifest =
            lua_engine.load_partial_manifest(test_template_path("advanced_hooks_layout"));
        const tempify::PrebyteRenderer renderer;
        const tempify::BuildContext context{
            .template_root = manifest.root,
            .build_root = build_root.path(),
            .values = {{"project_name", "Sandbox"}, {"project_slug", "sandbox"}, {"use_notes", "false"}},
        };

        lua_engine.run_hook(hook_path, manifest, context, renderer);
    }
};

std::string escape_message(const tempify::TempifyError &error) {
    return std::string(error.what());
}

} // namespace

TEST_CASE(LuaEngine_run_hook_write_file_rejects_absolute_path_outside_build_root) {
    HookSandboxFixture fixture("absolute-write");
    const std::filesystem::path escape_file = fixture.outside_root.path() / "escaped.txt";
    const std::string hook = "write_file('" + escape_file.string() + "', 'escaped')\n";

    try {
        fixture.run_hook_script(hook);
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(escape_message(error).find("Hook path escapes build root") != std::string::npos);
    }

    REQUIRE(!std::filesystem::exists(escape_file));
}

TEST_CASE(LuaEngine_run_hook_write_file_rejects_relative_path_traversal) {
    HookSandboxFixture fixture("relative-write");
    const std::filesystem::path escape_file = fixture.outside_root.path() / "escaped.txt";
    const std::string hook = "write_file('../outside/escaped.txt', 'escaped')\n";

    try {
        fixture.run_hook_script(hook);
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        const std::string message = escape_message(error);
        REQUIRE(message.find("Hook path escapes") != std::string::npos);
    }

    REQUIRE(!std::filesystem::exists(escape_file));
}

TEST_CASE(LuaEngine_run_hook_read_file_rejects_absolute_path_outside_allowed_roots) {
    HookSandboxFixture fixture("absolute-read");
    const std::filesystem::path secret_file = fixture.outside_root.path() / "secret.txt";
    write_text_file(secret_file, "top-secret");

    const std::string hook = "local content = read_file('" + secret_file.string() +
                             "')\n"
                             "write_file('copied.txt', content)\n";

    try {
        fixture.run_hook_script(hook);
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(escape_message(error).find("Hook path escapes allowed roots") != std::string::npos);
    }

    REQUIRE(!std::filesystem::exists(fixture.build_root.path() / "copied.txt"));
}

TEST_CASE(LuaEngine_run_hook_mkdir_and_remove_reject_paths_outside_build_root) {
    HookSandboxFixture fixture("mkdir-remove");
    const std::filesystem::path escape_dir = fixture.outside_root.path() / "escaped-dir";

    try {
        fixture.run_hook_script("mkdir('" + escape_dir.string() + "')\n");
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(escape_message(error).find("Hook path escapes build root") != std::string::npos);
    }
    REQUIRE(!std::filesystem::exists(escape_dir));

    write_text_file(fixture.outside_root.path() / "remove-me.txt", "delete");
    try {
        fixture.run_hook_script("remove('" + (fixture.outside_root.path() / "remove-me.txt").string() + "')\n");
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(escape_message(error).find("Hook path escapes build root") != std::string::npos);
    }
    REQUIRE(std::filesystem::exists(fixture.outside_root.path() / "remove-me.txt"));
}

TEST_CASE(LuaEngine_run_hook_copy_rejects_output_outside_build_root) {
    HookSandboxFixture fixture("copy-output");
    write_text_file(fixture.build_root.path() / "inside.txt", "inside");
    const std::filesystem::path escape_file = fixture.outside_root.path() / "escaped-copy.txt";

    try {
        fixture.run_hook_script("copy('inside.txt', '" + escape_file.string() + "')\n");
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(escape_message(error).find("Hook path escapes build root") != std::string::npos);
    }

    REQUIRE(!std::filesystem::exists(escape_file));
}

TEST_CASE(LuaEngine_run_hook_still_allows_template_relative_reads_and_build_root_writes) {
    HookSandboxFixture fixture("allowed-ops");
    fixture.run_hook_script("local template_text = read_file('files/docs/readme.txt.pbt')\n"
                            "write_file('allowed.txt', template_text)\n"
                            "mkdir('nested')\n");

    REQUIRE(std::filesystem::exists(fixture.build_root.path() / "allowed.txt"));
    REQUIRE(read_text_file(fixture.build_root.path() / "allowed.txt").find("{{ project_name }}") != std::string::npos);
    REQUIRE(std::filesystem::is_directory(fixture.build_root.path() / "nested"));
}

TEST_CASE(LuaEngine_run_hook_lists_files_and_directories_within_build_root) {
    HookSandboxFixture fixture("list");
    fixture.run_hook_script("write_file('listed.txt', 'x')\n"
                            "mkdir('listed-dir')\n"
                            "local files = list_files('.')\n"
                            "local dirs = list_dirs('.')\n"
                            "write_file('files.log', table.concat(files, ','))\n"
                            "write_file('dirs.log', table.concat(dirs, ','))\n"
                            "write_file('empty-files.log', tostring(#list_files('listed.txt')))\n");

    const std::string files = read_text_file(fixture.build_root.path() / "files.log");
    const std::string dirs = read_text_file(fixture.build_root.path() / "dirs.log");
    REQUIRE(files.find("listed.txt") != std::string::npos);
    REQUIRE(dirs.find("listed-dir") != std::string::npos);
    REQUIRE_EQ(read_text_file(fixture.build_root.path() / "empty-files.log"), std::string("0"));
}

TEST_CASE(LuaEngine_run_hook_copy_and_process_helpers_work_inside_build_root) {
    HookSandboxFixture fixture("helpers");
    fixture.run_hook_script("write_file('source.txt', 'Hello {{ project_name }}')\n"
                            "copy('source.txt', 'copy.txt')\n"
                            "local rendered = process_string('Hi {{ project_slug }}')\n"
                            "local overridden = process_string('Hi {{ project_slug }}', {project_slug='custom'})\n"
                            "write_file('rendered.txt', rendered)\n"
                            "write_file('override.txt', overridden)\n"
                            "process_file('source.txt', 'processed.txt')\n");

    REQUIRE_EQ(read_text_file(fixture.build_root.path() / "copy.txt"), std::string("Hello {{ project_name }}"));
    REQUIRE_EQ(read_text_file(fixture.build_root.path() / "rendered.txt"), std::string("Hi sandbox"));
    REQUIRE_EQ(read_text_file(fixture.build_root.path() / "override.txt"), std::string("Hi custom"));
    REQUIRE_EQ(read_text_file(fixture.build_root.path() / "processed.txt"), std::string("Hello Sandbox"));
}

TEST_CASE(LuaEngine_run_hook_exec_runs_shell_command_in_build_root) {
    HookSandboxFixture fixture("exec");
    fixture.run_hook_script("local root = get_build_root()\n"
                            "exec('echo hook-exec > \"' .. root .. '/marker.txt\"')\n");

    REQUIRE_EQ(read_text_file(fixture.build_root.path() / "marker.txt"), std::string("hook-exec\n"));
}
