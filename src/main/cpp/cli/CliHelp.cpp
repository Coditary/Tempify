#include "tempify/cli/CliParser.h"

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tempify {

namespace {

using HelpRow = std::pair<std::string_view, std::string_view>;

void append_section(std::ostringstream& stream,
                    const std::string_view title,
                    const std::vector<std::string>& lines) {
    stream << title << ":\n";
    for (const auto& line : lines) {
        stream << "  " << line << '\n';
    }
    stream << '\n';
}

void append_table(std::ostringstream& stream,
                  const std::string_view title,
                  const std::vector<HelpRow>& rows) {
    stream << title << ":\n";
    std::size_t width = 0;
    for (const auto& [name, description] : rows) {
        static_cast<void>(description);
        width = std::max(width, name.size());
    }

    for (const auto& [name, description] : rows) {
        stream << "  " << name;
        if (name.size() < width) {
            stream << std::string(width - name.size(), ' ');
        }
        stream << "  " << description << '\n';
    }
    stream << '\n';
}

std::string format_top_level_help() {
    std::ostringstream stream;
    stream << "Tempify - Local Template Generator\n\n";

    append_section(stream, "Usage", {
        "tempify <command> [args...] [options]",
        "tempify <template-id> [target] [options]",
        "tempify reapply <template-id> <target> [options]",
    });

    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--version", "Prints Tempify version"},
    });

    append_table(stream, "Commands", {
        {"list", "Lists available templates"},
        {"info", "Shows template details"},
        {"doctor", "Checks Tempify environment"},
        {"completion", "Generates shell completion script"},
        {"validate", "Validates template structure"},
        {"inspect", "Inspects merged template graph"},
        {"lint", "Lints template quality issues"},
        {"test", "Runs template fixtures"},
        {"refresh", "Rebuilds shared template index"},
        {"reapply", "Applies safe managed-file updates to existing target"},
        {"process", "Passes remaining args to embedded Prebyte"},
    });

    append_section(stream, "Examples", {
        "tempify basic_cpp my-app --set project_name=\"My App\"",
        "tempify basic_cpp my-app --diff --json",
        "tempify reapply basic_cpp my-app --report --json",
        "tempify test basic_cpp",
        "tempify basic_cpp my-app --tui",
        "tempify reapply basic_cpp my-app --json",
        "tempify completion bash",
        "tempify refresh",
        "tempify process -h",
        "tempify basic_cpp -h",
    });

    stream << "Run 'tempify <command> -h' for command-specific help.\n";
    stream << "Run 'tempify <template-id> -h' for render help.\n";
    return stream.str();
}

std::string format_list_help() {
    std::ostringstream stream;
    stream << "tempify list - List Available Templates\n\n";

    append_section(stream, "Usage", {"tempify list"});
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON catalog"},
    });
    append_section(stream, "Notes", {
        "Merges workspace templates with shared local template store.",
        "Also shows registry-cached templates when ReqPack has populated reqpack-available.json.",
    });
    return stream.str();
}

std::string format_refresh_help() {
    std::ostringstream stream;
    stream << "tempify refresh - Rebuild Shared Template Index\n\n";

    append_section(stream, "Usage", {"tempify refresh"});
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON summary"},
    });
    append_section(stream, "Notes", {
        "Scans shared templates directory and rebuilds index/templates.json.",
        "Package manager may also maintain same index file.",
    });
    return stream.str();
}

std::string format_completion_help() {
    std::ostringstream stream;
    stream << "tempify completion - Generate Shell Completion Script\n\n";

    append_section(stream, "Usage", {"tempify completion <shell>"});
    append_table(stream, "Arguments", {
        {"shell", "One of: bash, zsh, fish"},
    });
    append_table(stream, "Options", {{"-h,--help", "Displays this help"}});
    append_section(stream, "Notes", {
        "Writes shell completion script to stdout.",
        "Current scripts complete Tempify commands and local template ids via `tempify list`.",
    });
    append_section(stream, "Examples", {
        "tempify completion bash",
        "tempify completion zsh",
        "tempify completion fish",
    });
    return stream.str();
}

std::string format_info_help() {
    std::ostringstream stream;
    stream << "tempify info - Show Template Details\n\n";

    append_section(stream, "Usage", {"tempify info <template-id>"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON details"},
    });
    append_section(stream, "Notes", {
        "Shows template metadata, question summary, include ids, and hook presence.",
        "Falls back to registry-cached metadata when template is not installed locally.",
    });
    return stream.str();
}

std::string format_doctor_help() {
    std::ostringstream stream;
    stream << "tempify doctor - Inspect Tempify Environment\n\n";

    append_section(stream, "Usage", {"tempify doctor"});
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON summary"},
    });
    append_section(stream, "Notes", {
        "Checks data root, workspace template discovery, shared index readability, and catalog state.",
    });
    return stream.str();
}

std::string format_validate_help() {
    std::ostringstream stream;
    stream << "tempify validate - Validate Template Structure\n\n";

    append_section(stream, "Usage", {"tempify validate <template-id>"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON validation result"},
    });
    append_section(stream, "Notes", {
        "Loads full template graph and checks duplicate keys, aliases, and layout references.",
    });
    return stream.str();
}

std::string format_inspect_help() {
    std::ostringstream stream;
    stream << "tempify inspect - Inspect Merged Template Graph\n\n";

    append_section(stream, "Usage", {"tempify inspect <template-id>"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON inspection report"},
    });
    append_section(stream, "Notes", {
        "Shows merged includes, files, questions, layout rules, and hook provenance.",
    });
    return stream.str();
}

std::string format_lint_help() {
    std::ostringstream stream;
    stream << "tempify lint - Lint Template Quality\n\n";

    append_section(stream, "Usage", {"tempify lint <template-id>"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs machine-readable JSON lint report"},
    });
    append_section(stream, "Notes", {
        "Reports non-fatal authoring issues like missing prompts, help text, or unused sources.",
    });
    return stream.str();
}

std::string format_test_help() {
    std::ostringstream stream;
    stream << "tempify test - Run Template Fixtures\n\n";

    append_section(stream, "Usage", {"tempify test <template-id>"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--fixture <name>", "Runs only one named fixture"},
        {"--list-fixtures", "Lists fixture names without running them"},
        {"--json", "Outputs machine-readable test report"},
        {"--update-snapshots", "Rewrites snapshots from current render output"},
    });
    append_section(stream, "Notes", {
        "Looks for fixtures under <template-root>/tests/<fixture-name>/.",
        "Each fixture may provide answers.json, optional lock.json, and must provide snapshot/ expected output.",
        "Runs all selected fixtures and reports every failure before exiting non-zero.",
        "Snapshot update mode rewrites snapshot/ and optional lock.json for selected fixtures.",
    });
    return stream.str();
}

std::string format_questions_help() {
    std::ostringstream stream;
    stream << "tempify -q|--questions - Show Template Questions\n\n";

    append_section(stream, "Usage", {"tempify <template-id> -q|--questions [options]"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"--json", "Outputs question data as JSON"},
        {"--full", "Keeps empty question fields in output"},
    });
    return stream.str();
}

std::string format_process_help() {
    std::ostringstream stream;
    stream << "tempify process - Embedded Prebyte Passthrough\n\n";

    append_section(stream, "Usage", {
        "tempify process [prebyte-args...]",
        "tempify -p [prebyte-args...]",
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
    });
    append_section(stream, "Notes", {
        "Passes remaining arguments to embedded Prebyte command runner.",
        "Use this when you want direct Prebyte behavior through Tempify.",
    });
    return stream.str();
}

std::string format_render_help(const CliRequest& request) {
    std::ostringstream stream;
    stream << "tempify render - Generate Template Output\n\n";

    if (!request.template_ref.empty()) {
        append_section(stream, "Template", {request.template_ref});
    }

    append_section(stream, "Usage", {"tempify <template-id> [target] [options]"});
    append_table(stream, "Arguments", {
        {"template-id", "Template id or template path"},
        {"target", "Output directory"},
    });
    append_table(stream, "Options", {
        {"-h,--help", "Displays this help"},
        {"-q,--questions", "Shows question overview instead of rendering"},
        {"-f,--overwrite-if-exists", "Allows existing target and replaces conflicting files"},
        {"-s,--skip-if-file-exists", "Allows existing target and skips conflicting files"},
        {"--accept-hooks <yes|ask|no>", "Controls whether template hooks run"},
        {"--no-hooks", "Alias for --accept-hooks no"},
        {"--set <key=value>", "Sets template variable"},
        {"--var <key=value>", "Alias for --set"},
        {"--answers <path>", "Loads answer values from JSON file"},
        {"--write-answers <path>", "Writes resolved answers to JSON file"},
        {"--non-interactive", "Fails instead of prompting for missing answers"},
        {"--strict", "Rejects unknown or invalid imported answers"},
        {"--diff", "Compares managed output against target without writing files or running hooks"},
        {"--reapply", "Applies safe managed-file updates when matching origin lock permits"},
        {"--report", "With --reapply, prints reapply report only without writing files"},
        {"--json", "Outputs diff/reapply JSON with origin metadata, update kind, and blocked details"},
        {"--hook-timeout-ms <ms>", "Aborts hook phases that run longer than this timeout (0 disables)"},
        {"--dry-run", "Shows build plan without writing files"},
        {"--plan-json", "Outputs build plan as JSON"},
        {"--tui", "Uses wizard frontend"},
    });
    append_section(stream, "Examples", {
        "tempify basic_cpp my-app --set project_name=\"My App\"",
        "tempify basic_cpp existing-dir --reapply --json",
        "tempify basic_cpp existing-dir -s",
        "tempify path/to/template out-dir --accept-hooks ask",
        "tempify reapply basic_cpp existing-dir --json",
    });
    append_section(stream, "Notes", {
        "Global config: $XDG_CONFIG_HOME/tempify/config.json or ~/.config/tempify/config.json",
        "Nearest .tempify/config.json overlays global config for current workspace.",
        "Question precedence: defaults < .env < global config < workspace config < --answers < --set",
        "Render config is soft default only; explicit CLI flags override it.",
        "`--diff` is report-only. It does not write files and does not run hooks.",
        "`--diff` text output includes update recommendation based on origin lock and version transition.",
        "`--diff --json` outputs machine-readable status counts, origin metadata, and update kind.",
        "`--reapply --report` is report-only alias for reapply planning and does not write files.",
        "`--reapply --json` outputs machine-readable origin metadata, update kind, applied, kept, and blocked details.",
        "`tempify reapply <template-id> <target>` is alias for `tempify <template-id> <target> --reapply`.",
        "`--reapply` blocks cross-template .tempify-lock.json reuse and requires matching origin template id.",
        "`--reapply` allows patch/prerelease upgrades, but major upgrades, pre-1.0 minor upgrades, downgrades, or unknown version transitions require review.",
        "`--reapply` requires existing .tempify-lock.json, applies only ready create/update/delete actions, and does not run hooks.",
    });
    return stream.str();
}

}

std::string CliParser::help_text(const CliRequest& request) const {
    const HelpTopic topic = request.help_topic.value_or(HelpTopic::TopLevel);
    switch (topic) {
    case HelpTopic::TopLevel:
        return format_top_level_help();
    case HelpTopic::List:
        return format_list_help();
    case HelpTopic::Questions:
        return format_questions_help();
    case HelpTopic::Info:
        return format_info_help();
    case HelpTopic::Doctor:
        return format_doctor_help();
    case HelpTopic::Completion:
        return format_completion_help();
    case HelpTopic::Validate:
        return format_validate_help();
    case HelpTopic::Inspect:
        return format_inspect_help();
    case HelpTopic::Lint:
        return format_lint_help();
    case HelpTopic::Test:
        return format_test_help();
    case HelpTopic::Refresh:
        return format_refresh_help();
    case HelpTopic::Process:
        return format_process_help();
    case HelpTopic::Render:
        return format_render_help(request);
    }
    return format_top_level_help();
}

}
