#pragma once

#include "tempify/domain/RenderOptions.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

enum class HelpTopic {
    TopLevel,
    List,
    Info,
    Doctor,
    Completion,
    Validate,
    Inspect,
    Lint,
    Test,
    Questions,
    Refresh,
    Process,
    Render,
};

enum class CliMode {
    Help,
    Version,
    TemplateList,
    TemplateInfo,
    Doctor,
    Completion,
    TemplateValidate,
    TemplateInspect,
    TemplateLint,
    TemplateTest,
    Refresh,
    TemplateRender,
    PrebytePassthrough,
    QuestionsShow,
};

enum class QuestionsOutputFormat {
    Summary,
    Json,
};

struct CliRequest {
    CliMode mode = CliMode::Help;
    std::string template_ref;
    std::optional<std::filesystem::path> target_dir;
    std::map<std::string, std::string> variables;
    std::vector<std::string> raw_prebyte_args;
    std::optional<std::string> completion_shell;
    std::optional<ExistingPathBehavior> existing_path_behavior_override;
    HookAcceptance hook_acceptance = HookAcceptance::Yes;
    std::optional<std::filesystem::path> answers_file;
    std::optional<std::filesystem::path> write_answers_file;
    bool use_tui = false;
    int hook_timeout_ms = 30000;
    bool list_json = false;
    bool info_json = false;
    bool doctor_json = false;
    bool refresh_json = false;
    bool validate_json = false;
    bool inspect_json = false;
    bool lint_json = false;
    bool diff_json = false;
    bool non_interactive = false;
    bool strict = false;
    bool diff_only = false;
    bool reapply = false;
    bool reapply_report = false;
    bool dry_run = false;
    bool plan_json = false;
    std::optional<std::string> test_fixture_name;
    bool test_list_fixtures = false;
    bool test_json = false;
    bool test_update_snapshots = false;
    QuestionsOutputFormat questions_output_format = QuestionsOutputFormat::Summary;
    bool questions_full = false;
    std::optional<HelpTopic> help_topic;
};

}
