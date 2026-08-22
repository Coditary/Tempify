#include "TestHarness.h"
#include "tempify/cli/ShellCompletion.h"
#include "tempify/support/Errors.h"

#include <string>
#include <vector>

namespace {

void require_contains_all(const std::string &script, const std::vector<std::string> &needles) {
    for (const std::string &needle : needles) {
        REQUIRE(script.find(needle) != std::string::npos);
    }
}

const std::vector<std::string> &top_level_commands() {
    static const std::vector<std::string> commands = {
        "list", "info", "doctor", "completion", "validate", "inspect", "lint", "test", "refresh", "reapply", "process",
    };
    return commands;
}

const std::vector<std::string> &render_flags() {
    static const std::vector<std::string> flags = {
        "--set",  "--answers", "--non-interactive", "--strict", "--hook-timeout-ms", "--dry-run",      "--plan-json",
        "--diff", "--reapply", "--report",          "--json",   "--questions",       "--accept-hooks", "--no-hooks",
    };
    return flags;
}

const std::vector<std::string> &reapply_flags() {
    static const std::vector<std::string> flags = {
        "--set", "--answers", "--report", "--json", "--hook-timeout-ms",
    };
    return flags;
}

} // namespace

TEST_CASE(ShellCompletion_bash_script_lists_commands_render_and_reapply_flags) {
    const std::string script = tempify::render_shell_completion("bash");
    require_contains_all(script, top_level_commands());
    require_contains_all(script, render_flags());
    require_contains_all(script, reapply_flags());
    REQUIRE(script.find("complete -F _tempify tempify") != std::string::npos);
    REQUIRE(script.find("tempify list 2>/dev/null | cut -f1") != std::string::npos);
    REQUIRE(script.find("bash zsh fish") != std::string::npos);
}

TEST_CASE(ShellCompletion_zsh_script_lists_commands_and_template_lookup) {
    const std::string script = tempify::render_shell_completion("zsh");
    require_contains_all(script, top_level_commands());
    REQUIRE(script.find("#compdef tempify") != std::string::npos);
    REQUIRE(script.find("compdef _tempify tempify") != std::string::npos);
    REQUIRE(script.find("tempify list 2>/dev/null | cut -f1") != std::string::npos);
    REQUIRE(script.find("shells=(bash zsh fish)") != std::string::npos);
    REQUIRE(script.find("--questions") != std::string::npos);
    REQUIRE(script.find("--reapply") != std::string::npos);
}

TEST_CASE(ShellCompletion_fish_script_lists_commands_and_render_flags) {
    const std::string script = tempify::render_shell_completion("fish");
    require_contains_all(script, top_level_commands());
    REQUIRE(script.find("complete -c tempify -f") != std::string::npos);
    REQUIRE(script.find("__fish_use_subcommand") != std::string::npos);
    REQUIRE(script.find("__fish_seen_subcommand_from reapply") != std::string::npos);
    REQUIRE(script.find("dry-run") != std::string::npos);
    REQUIRE(script.find("plan-json") != std::string::npos);
    REQUIRE(script.find("tempify list 2>/dev/null | cut -f1") != std::string::npos);
}

TEST_CASE(ShellCompletion_rejects_unknown_shell) {
    REQUIRE_THROWS_AS(tempify::render_shell_completion("pwsh"), tempify::TempifyError);
}
