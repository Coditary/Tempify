#include "TempifyAppTestSupport.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::create_slow_hook_template;
using tempify::test_support::json_escaped_path;
using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdinCapture;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::test_template_path;

} // namespace

TEST_CASE(TempifyApp_no_hooks_skips_hook_side_effects) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-no-hooks-test");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-no-hooks-data-home");

    tempify::TempifyApp app;
    const int result = app.run({
        "advanced_hooks_layout",
        target.path().string(),
        "--accept-hooks",
        "no",
        "--set",
        "project_name=No Hooks",
        "--set",
        "project_slug=no-hooks",
        "--set",
        "use_notes=false",
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
        "advanced_hooks_layout",
        target.path().string(),
        "--accept-hooks",
        "ask",
        "--set",
        "project_name=Ask Hooks",
        "--set",
        "project_slug=ask-hooks",
        "--set",
        "use_notes=false",
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
                       "advanced_hooks_layout",
                       target.path().string(),
                       "--accept-hooks",
                       "ask",
                       "--set",
                       "project_name=Trusted Hooks",
                       "--set",
                       "project_slug=trusted-hooks",
                       "--set",
                       "use_notes=false",
                   }),
                   0);
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
    REQUIRE(read_text_file(trust_store)
                .find(json_escaped_path(std::filesystem::weakly_canonical(
                    test_template_path("advanced_hooks_layout")))) != std::string::npos);

    {
        ScopedDirectoryCleanup second_target(std::filesystem::temp_directory_path() /
                                             "tempify-app-hook-trust-target-2");
        ScopedStdinCapture input("");
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({
                       "advanced_hooks_layout",
                       second_target.path().string(),
                       "--accept-hooks",
                       "ask",
                       "--set",
                       "project_name=Trusted Hooks Again",
                       "--set",
                       "project_slug=trusted-hooks-again",
                       "--set",
                       "use_notes=false",
                       "--tui",
                   }),
                   0);
        const std::string output = capture.str();
        REQUIRE(output.find("Template hooks trusted from previous approval.") != std::string::npos);
        REQUIRE(output.find("Run hooks and trust this template next time?") == std::string::npos);
    }
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
            "--hook-timeout-ms",
            "25",
            "--set",
            "project_name=Slow App",
        }));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Hook phase 'post' failed") != std::string::npos);
        REQUIRE(std::string(error.what()).find("post.lua") != std::string::npos);
        REQUIRE(std::string(error.what()).find("timed out after 25 ms") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_accept_hooks_ask_tty_declining_prompt_skips_hooks) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-app-accept-hooks-no-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-app-accept-hooks-no-data-home");
    prebyte::test::ScopedEnvironmentVariable force_prompt("TEMPIFY_FORCE_HOOK_PROMPT", "1");
    tempify::TempifyApp app;

    ScopedStdinCapture input("no\n");
    REQUIRE_EQ(app.run({
                   "advanced_hooks_layout",
                   target.path().string(),
                   "--accept-hooks",
                   "ask",
                   "--set",
                   "project_name=Declined Hooks",
                   "--set",
                   "project_slug=declined-hooks",
                   "--set",
                   "use_notes=false",
               }),
               0);

    REQUIRE(std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(!std::filesystem::exists(target.path() / "pre.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "post.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "script-marker.txt"));
}
