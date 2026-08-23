#include "tempify/cli/CliParser.h"

#include "tempify/support/Errors.h"
#include "tempify/support/Version.h"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cctype>

namespace tempify {

namespace {

bool is_help_token(const std::string &arg) {
    return arg == "-h" || arg == "--help";
}

bool is_questions_token(const std::string &arg) {
    return arg == "-q" || arg == "--questions";
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

HookAcceptance parse_hook_acceptance(std::string value) {
    value = lowercase(std::move(value));
    if (value == "yes") {
        return HookAcceptance::Yes;
    }
    if (value == "ask") {
        return HookAcceptance::Ask;
    }
    if (value == "no") {
        return HookAcceptance::No;
    }
    throw TempifyError("Invalid value for `--accept-hooks`: " + value + ". Expected yes, ask, or no.");
}

bool contains_help_token(const std::vector<std::string> &args, const std::size_t start_index = 0) {
    return std::ranges::any_of(args.begin() + static_cast<std::ptrdiff_t>(start_index), args.end(),
                               [](const std::string &arg) { return is_help_token(arg); });
}

bool contains_value(const std::vector<std::string> &args, const std::string &value) {
    return std::ranges::find(args, value) != args.end();
}

CliRequest help_request(const HelpTopic topic) {
    CliRequest request;
    request.mode = CliMode::Help;
    request.help_topic = topic;
    return request;
}

CliRequest reapply_help_request(const std::vector<std::string> &args) {
    CliRequest request = help_request(HelpTopic::Render);
    request.reapply = true;
    if (args.size() > 1 && !is_help_token(args[1]) && !args[1].starts_with('-')) {
        request.template_ref = args[1];
    }
    if (args.size() > 2 && !is_help_token(args[2]) && !args[2].starts_with('-')) {
        request.target_dir = std::filesystem::path(args[2]);
    }
    return request;
}

std::pair<std::string, std::string> split_assignment(const std::string &value) {
    const std::size_t separator = value.find('=');
    if (separator == std::string::npos || separator == 0) {
        throw TempifyError("Expected assignment in form key=value, got: " + value);
    }

    return {value.substr(0, separator), value.substr(separator + 1)};
}

bool parse_cli_args(CLI::App &app, const std::vector<std::string> &args) {
    std::vector<std::string> mutable_args = args;
    std::reverse(mutable_args.begin(), mutable_args.end());
    try {
        app.parse(mutable_args);
    } catch (const CLI::CallForHelp &) {
        return true;
    } catch (const CLI::ParseError &error) {
        throw TempifyError(error.what());
    }
    return false;
}

CliRequest parse_render_request(const std::vector<std::string> &args);

CliRequest parse_list_request(const std::vector<std::string> &args) {
    CliRequest request;
    bool json_output = false;
    CLI::App app{"List available templates"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_flag("--json", json_output, "Output machine-readable JSON catalog");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        return request;
    }
    request.mode = CliMode::TemplateList;
    request.list_json = json_output;
    return request;
}

CliRequest parse_info_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string template_ref;
    bool json_output = false;

    CLI::App app{"Show template details"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("template-id", template_ref, "Template id or template path")->required();
    app.add_flag("--json", json_output, "Output machine-readable JSON template details");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Info;
        return request;
    }

    request.mode = CliMode::TemplateInfo;
    request.template_ref = template_ref;
    request.info_json = json_output;
    return request;
}

CliRequest parse_doctor_request(const std::vector<std::string> &args) {
    CliRequest request;
    bool json_output = false;
    CLI::App app{"Inspect Tempify environment"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_flag("--json", json_output, "Output machine-readable JSON environment summary");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Doctor;
        return request;
    }

    request.mode = CliMode::Doctor;
    request.doctor_json = json_output;
    return request;
}

CliRequest parse_completion_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string shell;

    CLI::App app{"Generate shell completion script"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("shell", shell, "Shell name: bash, zsh, fish")->required();
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Completion;
        return request;
    }

    if (shell != "bash" && shell != "zsh" && shell != "fish") {
        throw TempifyError("Invalid shell for `completion`: " + shell + ". Expected bash, zsh, or fish.");
    }

    request.mode = CliMode::Completion;
    request.completion_shell = shell;
    return request;
}

CliRequest parse_validate_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string template_ref;
    bool json_output = false;

    CLI::App app{"Validate template structure and references"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("template-id", template_ref, "Template id or template path")->required();
    app.add_flag("--json", json_output, "Output machine-readable JSON validation result");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Validate;
        return request;
    }

    request.mode = CliMode::TemplateValidate;
    request.template_ref = template_ref;
    request.validate_json = json_output;
    return request;
}

CliRequest parse_inspect_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string template_ref;
    bool json_output = false;

    CLI::App app{"Inspect merged template graph and provenance"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("template-id", template_ref, "Template id or template path")->required();
    app.add_flag("--json", json_output, "Output machine-readable JSON inspection report");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Inspect;
        return request;
    }

    request.mode = CliMode::TemplateInspect;
    request.template_ref = template_ref;
    request.inspect_json = json_output;
    return request;
}

CliRequest parse_lint_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string template_ref;
    bool json_output = false;

    CLI::App app{"Lint template quality and authoring issues"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("template-id", template_ref, "Template id or template path")->required();
    app.add_flag("--json", json_output, "Output machine-readable JSON lint report");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Lint;
        return request;
    }

    request.mode = CliMode::TemplateLint;
    request.template_ref = template_ref;
    request.lint_json = json_output;
    return request;
}

CliRequest parse_test_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string template_ref;
    std::string fixture_name;
    bool list_fixtures = false;
    bool json_output = false;
    bool update_snapshots = false;

    CLI::App app{"Run template fixtures and compare snapshots"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("template-id", template_ref, "Template id or template path")->required();
    app.add_option("--fixture", fixture_name, "Run single named fixture");
    app.add_flag("--list-fixtures", list_fixtures, "List fixture names without running them");
    app.add_flag("--json", json_output, "Output machine-readable JSON test report");
    app.add_flag("--update-snapshots", update_snapshots, "Rewrite fixture snapshots from current render output");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        request.help_topic = HelpTopic::Test;
        return request;
    }

    request.mode = CliMode::TemplateTest;
    request.template_ref = template_ref;
    if (!fixture_name.empty()) {
        request.test_fixture_name = fixture_name;
    }
    request.test_list_fixtures = list_fixtures;
    request.test_json = json_output;
    request.test_update_snapshots = update_snapshots;
    return request;
}

CliRequest parse_questions_request(const std::vector<std::string> &args) {
    CliRequest request;
    if (args.size() < 2 || !is_questions_token(args[1])) {
        throw TempifyError("Question overview syntax is `tempify <template-id> -q|--questions [--json] [--full]`.");
    }

    request.template_ref = args[0];

    for (std::size_t index = 2; index < args.size(); ++index) {
        const auto &arg = args[index];
        if (arg == "--json") {
            request.questions_output_format = QuestionsOutputFormat::Json;
            continue;
        }
        if (arg == "--full") {
            request.questions_full = true;
            continue;
        }

        throw TempifyError("Unknown question option: " + arg);
    }

    request.mode = CliMode::QuestionsShow;
    return request;
}

CliRequest parse_refresh_request(const std::vector<std::string> &args) {
    CliRequest request;
    bool json_output = false;
    CLI::App app{"Refresh shared template index"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_flag("--json", json_output, "Output machine-readable JSON refresh summary");
    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        return request;
    }
    request.mode = CliMode::Refresh;
    request.refresh_json = json_output;
    return request;
}

CliRequest parse_reapply_request(const std::vector<std::string> &args) {
    std::vector<std::string> render_args(args.begin() + 1, args.end());
    render_args.push_back("--reapply");
    return parse_render_request(render_args);
}

CliRequest parse_render_request(const std::vector<std::string> &args) {
    CliRequest request;
    std::string render_template_ref;
    std::string render_target_dir;
    std::string accept_hooks;
    std::string answers_file;
    std::string write_answers_file;
    std::vector<std::string> assignments;
    int hook_timeout_ms = request.hook_timeout_ms;
    bool overwrite_if_exists = false;
    bool skip_if_file_exists = false;
    bool no_hooks = false;
    bool json_output = false;
    bool reapply_report = false;

    CLI::App app{"Tempify template generator"};
    app.set_help_flag("-h,--help", "Show help");
    app.add_option("template-id", render_template_ref, "Template id or template path")->required();
    app.add_option("target", render_target_dir, "Target directory");
    app.add_option("--set,--var", assignments, "Set template variable in form key=value")->allow_extra_args(false);
    app.add_option("--answers", answers_file, "Load answer values from JSON file");
    app.add_option("--write-answers", write_answers_file, "Write resolved answers to JSON file after generation");
    app.add_flag("-f,--overwrite-if-exists", overwrite_if_exists, "Replace conflicting output files");
    app.add_flag("-s,--skip-if-file-exists", skip_if_file_exists, "Skip conflicting output files");
    app.add_option("--accept-hooks", accept_hooks, "Hook execution policy: yes, ask, no");
    app.add_flag("--no-hooks", no_hooks, "Alias for --accept-hooks no");
    app.add_flag("--tui", request.use_tui, "Use wizard frontend");
    app.add_flag("--non-interactive", request.non_interactive, "Fail instead of prompting for missing answers");
    app.add_flag("--strict", request.strict, "Reject unknown or invalid imported answers");
    app.add_flag("--diff", request.diff_only,
                 "Compare managed output against target without writing files or running hooks");
    app.add_flag("--reapply", request.reapply,
                 "Apply safe managed-file updates to existing target when origin lock permits");
    app.add_flag("--report", reapply_report, "With --reapply, print reapply report only without writing files");
    app.add_flag("--json", json_output, "Output diff report as JSON when used with --diff");
    app.add_option("--hook-timeout-ms", hook_timeout_ms,
                   "Abort hook phases that exceed this timeout in milliseconds (0 disables)");
    app.add_flag("--dry-run", request.dry_run, "Show build plan without writing files or running hooks");
    app.add_flag("--plan-json", request.plan_json, "Output build plan as JSON");

    if (parse_cli_args(app, args)) {
        request.mode = CliMode::Help;
        return request;
    }

    if (overwrite_if_exists && skip_if_file_exists) {
        throw TempifyError("Cannot combine `-f/--overwrite-if-exists` with `-s/--skip-if-file-exists`.");
    }

    if (overwrite_if_exists) {
        request.existing_path_behavior_override = ExistingPathBehavior::Overwrite;
    }
    if (skip_if_file_exists) {
        request.existing_path_behavior_override = ExistingPathBehavior::Skip;
    }

    if (no_hooks && !accept_hooks.empty() && lowercase(accept_hooks) != "no") {
        throw TempifyError("`--no-hooks` conflicts with `--accept-hooks` unless value is `no`.");
    }

    if (hook_timeout_ms < 0) {
        throw TempifyError("`--hook-timeout-ms` must be zero or greater.");
    }

    if (json_output && !request.diff_only && !request.reapply) {
        throw TempifyError("`--json` in render mode requires `--diff` or `--reapply`.");
    }

    if (request.reapply && request.diff_only) {
        throw TempifyError("`--reapply` cannot be combined with `--diff`.");
    }

    if (reapply_report && !request.reapply) {
        throw TempifyError("`--report` in render mode requires `--reapply`.");
    }

    if (request.diff_only && (request.dry_run || request.plan_json)) {
        throw TempifyError("`--diff` cannot be combined with `--dry-run` or `--plan-json`.");
    }

    if (request.reapply && (request.dry_run || request.plan_json)) {
        throw TempifyError("`--reapply` cannot be combined with `--dry-run` or `--plan-json`.");
    }

    if (reapply_report && !write_answers_file.empty()) {
        throw TempifyError("`--reapply --report` cannot be combined with `--write-answers`.");
    }

    if (request.diff_only && !write_answers_file.empty()) {
        throw TempifyError("`--diff` cannot be combined with `--write-answers`.");
    }

    if (request.reapply && render_target_dir.empty()) {
        throw TempifyError("`--reapply` requires explicit target directory.");
    }

    if (no_hooks) {
        request.hook_acceptance = HookAcceptance::No;
    } else if (!accept_hooks.empty()) {
        request.hook_acceptance = parse_hook_acceptance(accept_hooks);
    }
    request.hook_timeout_ms = hook_timeout_ms;
    request.diff_json = json_output;
    request.reapply_report = reapply_report;

    if (!answers_file.empty()) {
        request.answers_file = answers_file;
    }
    if (!write_answers_file.empty()) {
        request.write_answers_file = write_answers_file;
    }

    request.mode = CliMode::TemplateRender;
    request.template_ref = render_template_ref;
    if (!render_target_dir.empty()) {
        request.target_dir = render_target_dir;
    }
    for (const std::string &assignment : assignments) {
        const auto [key, value] = split_assignment(assignment);
        request.variables[key] = value;
    }
    return request;
}

} // namespace

CliRequest CliParser::parse(const std::vector<std::string> &args) const {
    CliRequest request;

    if (args.empty()) {
        return help_request(HelpTopic::TopLevel);
    }

    if (args.size() == 1 && (is_help_token(args[0]) || args[0] == "help")) {
        return help_request(HelpTopic::TopLevel);
    }

    if (args.size() == 1 && (args[0] == "-v" || args[0] == "--version" || args[0] == "version")) {
        request.mode = CliMode::Version;
        return request;
    }

    if (is_questions_token(args[0])) {
        throw TempifyError("Question overview syntax is `tempify <template-id> -q|--questions [--json] [--full]`.");
    }

    if (args[0] == "list" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::List);
    }

    if (args[0] == "info" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Info);
    }

    if (args[0] == "doctor" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Doctor);
    }

    if (args[0] == "completion" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Completion);
    }

    if (args[0] == "validate" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Validate);
    }

    if (args[0] == "inspect" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Inspect);
    }

    if (args[0] == "lint" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Lint);
    }

    if (args[0] == "test" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Test);
    }

    if (args[0] == "refresh" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Refresh);
    }

    if (args[0] == "reapply" && contains_help_token(args, 1)) {
        return reapply_help_request(args);
    }

    if (args.size() >= 2 && is_questions_token(args[1]) && contains_help_token(args, 2)) {
        request = help_request(HelpTopic::Questions);
        request.template_ref = args[0];
        return request;
    }

    if (args[0] == "process" && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Process);
    }

    if ((args[0] == "-p" || args[0] == "--prebyte") && contains_help_token(args, 1)) {
        return help_request(HelpTopic::Process);
    }

    if (args.size() >= 3 && !args[0].starts_with('-') && !args[1].starts_with('-') && is_questions_token(args[2])) {
        throw TempifyError("Question overview does not accept target path. Use `tempify <template-id> -q|--questions "
                           "[--json] [--full]`.");
    }

    if (!args.empty() && !is_help_token(args[0]) && contains_help_token(args, 1)) {
        request = help_request(HelpTopic::Render);
        request.template_ref = args[0];
        if (args.size() > 1 && !is_help_token(args[1]) && !args[1].starts_with('-')) {
            request.target_dir = std::filesystem::path(args[1]);
        }
        return request;
    }

    if (args[0] == "template") {
        throw TempifyError("Command `template` removed. Use `tempify list` or `tempify <template-id> -q|--questions`.");
    }

    if (args[0] == "prebyte") {
        throw TempifyError("Command `prebyte` removed. Use `tempify -p [prebyte-args...]`.");
    }

    if (args[0] == "schema") {
        throw TempifyError("Command `schema` removed. Use `tempify <template-id> -q|--questions`.");
    }

    if (args[0] == "questions") {
        throw TempifyError("Command `questions` removed. Use `tempify <template-id> -q|--questions`.");
    }

    if (args[0] == "registry") {
        throw TempifyError("Command `registry` removed. Use shared local template store and `tempify refresh`.");
    }

    if (args[0] == "-p" || args[0] == "--prebyte") {
        request.mode = CliMode::PrebytePassthrough;
        request.raw_prebyte_args.assign(args.begin() + 1, args.end());
        return request;
    }

    if (contains_value(args, "-q") || contains_value(args, "--questions")) {
        return parse_questions_request(args);
    }

    if (args[0] == "list") {
        return parse_list_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "info") {
        return parse_info_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "doctor") {
        return parse_doctor_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "completion") {
        return parse_completion_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "validate") {
        return parse_validate_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "inspect") {
        return parse_inspect_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "lint") {
        return parse_lint_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "test") {
        return parse_test_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "refresh") {
        return parse_refresh_request({args.begin() + 1, args.end()});
    }

    if (args[0] == "reapply") {
        return parse_reapply_request(args);
    }

    if (args[0] == "process") {
        request.mode = CliMode::PrebytePassthrough;
        request.raw_prebyte_args.assign(args.begin() + 1, args.end());
        return request;
    }

    return parse_render_request(args);
}

} // namespace tempify
