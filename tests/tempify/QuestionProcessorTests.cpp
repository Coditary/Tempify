#include "TestHarness.h"

#include "tempify/frontend/IQuestionFrontend.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/question/QuestionProcessor.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class StubFrontend final : public tempify::IQuestionFrontend {
public:
    explicit StubFrontend(std::vector<std::string> answers)
        : answers_(std::move(answers)) {}

    std::optional<tempify::PromptResult> prompt(const std::string& text, const bool sensitive = false) override {
        prompts.push_back(text);
        sensitive_prompts.push_back(sensitive);
        if (index_ >= answers_.size()) {
            return std::nullopt;
        }
        const std::string value = answers_[index_++];
        if (value == ":back") {
            return tempify::PromptResult{.action = tempify::FrontendAction::Back, .value = {}};
        }
        if (value == ":quit") {
            return tempify::PromptResult{.action = tempify::FrontendAction::Quit, .value = {}};
        }
        return tempify::PromptResult{.action = tempify::FrontendAction::Submit, .value = value};
    }

    void write_line(const std::string& text) override {
        lines.push_back(text);
    }

    void begin_group(const std::string& name, const std::size_t index, const std::size_t total) override {
        groups.push_back(name + ":" + std::to_string(index) + "/" + std::to_string(total));
    }

    void end_group() override {
        ends += 1;
    }

    std::vector<std::string> prompts;
    std::vector<bool> sensitive_prompts;
    std::vector<std::string> lines;
    std::vector<std::string> groups;
    int ends = 0;

private:
    std::vector<std::string> answers_;
    std::size_t index_ = 0;
};

tempify::TemplateManifest load_basic_cpp_manifest() {
    tempify::LuaEngine lua_engine;
    return lua_engine.load_partial_manifest(std::filesystem::path{"templates/basic_cpp"});
}

}

TEST_CASE(QuestionProcessor_collect_prompt_loop_and_condition) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({
        "ab",
        "Stone App",
        "",
        "123",
        "core",
        "yes",
        "2",
        "ftp://bad",
        "https://docs.example",
    });

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(manifest, {{"author", "Leodoras"}});

    REQUIRE_EQ(values.at("project_name"), std::string("Stone App"));
    REQUIRE_EQ(values.at("project_slug"), std::string("stone-app"));
    REQUIRE_EQ(values.at("name_slug"), std::string("stone-app"));
    REQUIRE_EQ(values.at("namespace"), std::string("core"));
    REQUIRE_EQ(values.at("include_ci"), std::string("true"));
    REQUIRE_EQ(values.at("ci_provider"), std::string("gitlab"));
    REQUIRE_EQ(values.at("docs_url"), std::string("https://docs.example"));
    REQUIRE_EQ(values.at("author"), std::string("Leodoras"));
    REQUIRE(frontend.lines.size() >= 3);
    REQUIRE_EQ(frontend.groups.size(), static_cast<std::size_t>(2));
}

TEST_CASE(QuestionProcessor_skip_condition_when_ci_disabled) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({
        "Quiet App",
        "",
        "quiet",
        "no",
    });

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(manifest, {{"author", "Leodoras"}});

    REQUIRE_EQ(values.at("project_name"), std::string("Quiet App"));
    REQUIRE_EQ(values.at("project_slug"), std::string("quiet-app"));
    REQUIRE_EQ(values.at("include_ci"), std::string("false"));
    REQUIRE(values.find("ci_provider") == values.end());
    REQUIRE(values.find("docs_url") == values.end());
    REQUIRE_EQ(frontend.prompts.size(), static_cast<std::size_t>(4));
}

TEST_CASE(QuestionProcessor_cli_values_bypass_prompts_and_validate) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {
            {"project_name", "CLI App"},
            {"project_slug", "cli-app"},
            {"namespace", "cli_ns"},
            {"include_ci", "false"},
            {"author", "Leodoras"},
        });

    REQUIRE_EQ(values.at("project_name"), std::string("CLI App"));
    REQUIRE_EQ(values.at("project_slug"), std::string("cli-app"));
    REQUIRE_EQ(values.at("namespace"), std::string("cli_ns"));
    REQUIRE_EQ(values.at("include_ci"), std::string("false"));
    REQUIRE(frontend.prompts.empty());
}

TEST_CASE(QuestionProcessor_invalid_cli_value_fails_validation) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);

    REQUIRE_THROWS_AS(
        processor.collect(
            manifest,
            {
                {"project_name", "Ok Name"},
                {"project_slug", "Bad Slug"},
                {"namespace", "cli_ns"},
                {"include_ci", "false"},
            }),
        tempify::TempifyError);
}

TEST_CASE(QuestionProcessor_manual_int_help_and_optional_skip) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"General"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "threads",
            .type = "int",
            .prompt = "Thread count",
            .default_value = std::string("4"),
            .optional = false,
            .help = "How many worker threads to use.",
        },
        tempify::QuestionDefinition{
            .key = "notes",
            .type = "string",
            .prompt = "Notes",
            .optional = true,
        },
    };

    tempify::LuaEngine lua_engine;
    StubFrontend frontend({
        "?",
        "abc",
        "8",
        "",
    });

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(manifest, {});

    REQUIRE_EQ(values.at("threads"), std::string("8"));
    REQUIRE(values.find("notes") == values.end());
    REQUIRE(std::ranges::find(frontend.lines, std::string("How many worker threads to use.")) != frontend.lines.end());
    REQUIRE(std::ranges::find(frontend.lines, std::string("Expected integer value")) != frontend.lines.end());
}

TEST_CASE(QuestionProcessor_alias_cli_value_maps_to_canonical_key) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {
            {"project_name", "Alias App"},
            {"name_slug", "alias-slug"},
            {"namespace", "alias_ns"},
            {"include_ci", "false"},
        });

    REQUIRE_EQ(values.at("project_slug"), std::string("alias-slug"));
    REQUIRE_EQ(values.at("name_slug"), std::string("alias-slug"));
}

TEST_CASE(QuestionProcessor_hidden_cli_values_are_ignored_when_condition_false) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {
            {"project_name", "Alias App"},
            {"project_slug", "alias-app"},
            {"namespace", "alias_ns"},
            {"include_ci", "false"},
            {"ci_provider", "gitlab"},
            {"docs_url", "https://docs.example"},
        });

    REQUIRE(values.find("ci_provider") == values.end());
    REQUIRE(values.find("docs_url") == values.end());
}

TEST_CASE(QuestionProcessor_group_navigation_back_reasks_previous_page) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({
        "First App",
        "",
        "core",
        "yes",
        ":back",
        "Second App",
        "",
        "core2",
        "yes",
        "1",
        "-",
    });

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(manifest, {{"author", "Leodoras"}});

    REQUIRE_EQ(values.at("project_name"), std::string("Second App"));
    REQUIRE_EQ(values.at("project_slug"), std::string("second-app"));
    REQUIRE_EQ(values.at("namespace"), std::string("core2"));
    REQUIRE_EQ(values.at("ci_provider"), std::string("github"));
    REQUIRE(values.find("docs_url") == values.end());
    REQUIRE(frontend.groups.size() >= static_cast<std::size_t>(3));
}

TEST_CASE(QuestionProcessor_review_stage_can_go_back_and_confirm) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({
        "First App",
        "",
        "core",
        "no",
        "n",
        ":back",
        "Second App",
        "",
        "core2",
        "no",
        "yes",
    });

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {{"author", "Leodoras"}},
        {},
        {},
        false,
        false,
        true);

    REQUIRE_EQ(values.at("project_name"), std::string("Second App"));
    REQUIRE_EQ(values.at("project_slug"), std::string("second-app"));
    REQUIRE_EQ(values.at("namespace"), std::string("core2"));
    REQUIRE_EQ(values.at("include_ci"), std::string("false"));
    REQUIRE(std::ranges::find(frontend.lines, std::string("Review values before generate:")) != frontend.lines.end());
}

TEST_CASE(QuestionProcessor_sensitive_questions_redact_review_defaults_and_mark_sensitive_prompts) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"Secrets"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "api_token",
            .type = "string",
            .prompt = "API token",
            .default_value = std::string("secret-default"),
            .validate_is_function = false,
            .group = "Secrets",
            .sensitive = true,
        },
        tempify::QuestionDefinition{
            .key = "project_name",
            .type = "string",
            .prompt = "Project name",
            .group = "Secrets",
        },
    };

    tempify::LuaEngine lua_engine;
    StubFrontend frontend({
        "",
        "Stone App",
        "yes",
    });

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {},
        {},
        {},
        false,
        false,
        true);

    REQUIRE_EQ(values.at("api_token"), std::string("secret-default"));
    REQUIRE_EQ(values.at("project_name"), std::string("Stone App"));
    REQUIRE_EQ(frontend.prompts.at(0), std::string("API token (default: <redacted>): "));
    REQUIRE(frontend.sensitive_prompts.at(0));
    REQUIRE(std::ranges::find(frontend.lines, std::string("API token: <redacted>")) != frontend.lines.end());
}

TEST_CASE(QuestionProcessor_conflicting_alias_and_canonical_cli_values_throw) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);

    REQUIRE_THROWS_AS(
        processor.collect(
            manifest,
            {
                {"project_name", "Alias App"},
                {"project_slug", "slug-a"},
                {"name_slug", "slug-b"},
                {"namespace", "alias_ns"},
                {"include_ci", "false"},
            }),
        tempify::TempifyError);
}

TEST_CASE(QuestionProcessor_cli_bool_and_choice_numeric_values_are_coerced) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {
            {"project_name", "Numeric Choice App"},
            {"project_slug", "numeric-choice-app"},
            {"namespace", "numeric_ns"},
            {"include_ci", "1"},
            {"ci_provider", "2"},
            {"docs_url", ""},
        });

    REQUIRE_EQ(values.at("include_ci"), std::string("true"));
    REQUIRE_EQ(values.at("ci_provider"), std::string("gitlab"));
    REQUIRE_EQ(values.at("docs_url"), std::string(""));
    REQUIRE(frontend.prompts.empty());
}

TEST_CASE(QuestionProcessor_env_defaults_override_question_defaults) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"General"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "flavor",
            .type = "string",
            .prompt = "Flavor",
            .default_value = std::string("question-default"),
        },
    };
    manifest.env_defaults["flavor"] = "env-default";

    tempify::LuaEngine lua_engine;
    StubFrontend frontend({""});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(manifest, {});

    REQUIRE_EQ(values.at("flavor"), std::string("env-default"));
    REQUIRE(frontend.prompts.size() == static_cast<std::size_t>(1));
}

TEST_CASE(QuestionProcessor_config_defaults_override_env_but_answers_file_and_cli_override_them) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"General"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "flavor",
            .type = "string",
            .prompt = "Flavor",
            .default_value = std::string("question-default"),
        },
    };
    manifest.env_defaults["flavor"] = "env-default";

    tempify::LuaEngine lua_engine;
    StubFrontend default_frontend({""});
    tempify::QuestionProcessor default_processor(lua_engine, default_frontend);

    const std::map<std::string, std::string> config_default_values = default_processor.collect(
        manifest,
        {},
        {{"flavor", "config-default"}},
        {},
        false,
        false);
    REQUIRE_EQ(config_default_values.at("flavor"), std::string("config-default"));
    REQUIRE_EQ(default_frontend.prompts.at(0), std::string("Flavor (default: config-default): "));

    StubFrontend imported_frontend({});
    tempify::QuestionProcessor imported_processor(lua_engine, imported_frontend);
    const std::map<std::string, std::string> answer_over_config = imported_processor.collect(
        manifest,
        {},
        {{"flavor", "config-default"}},
        {{"flavor", "answer-default"}},
        true,
        false);
    REQUIRE_EQ(answer_over_config.at("flavor"), std::string("answer-default"));

    StubFrontend cli_frontend({});
    tempify::QuestionProcessor cli_processor(lua_engine, cli_frontend);
    const std::map<std::string, std::string> cli_over_config = cli_processor.collect(
        manifest,
        {{"flavor", "cli-default"}},
        {{"flavor", "config-default"}},
        {{"flavor", "answer-default"}},
        true,
        false);
    REQUIRE_EQ(cli_over_config.at("flavor"), std::string("cli-default"));
}

TEST_CASE(QuestionProcessor_imported_values_bypass_prompts_and_support_aliases) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(
        manifest,
        {},
        {},
        {
            {"project_name", "Imported App"},
            {"name_slug", "imported-app"},
            {"namespace", "imported_ns"},
            {"include_ci", "false"},
        });

    REQUIRE_EQ(values.at("project_name"), std::string("Imported App"));
    REQUIRE_EQ(values.at("project_slug"), std::string("imported-app"));
    REQUIRE_EQ(values.at("name_slug"), std::string("imported-app"));
    REQUIRE_EQ(values.at("namespace"), std::string("imported_ns"));
    REQUIRE(frontend.prompts.empty());
}

TEST_CASE(QuestionProcessor_non_interactive_applies_defaults_and_skips_optional) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"General"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "name",
            .type = "string",
            .prompt = "Name",
            .default_value = std::string("stone"),
        },
        tempify::QuestionDefinition{
            .key = "notes",
            .type = "string",
            .prompt = "Notes",
            .optional = true,
        },
    };

    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    const std::map<std::string, std::string> values = processor.collect(manifest, {}, {}, {}, true, false);

    REQUIRE_EQ(values.at("name"), std::string("stone"));
    REQUIRE(values.find("notes") == values.end());
    REQUIRE(frontend.prompts.empty());
}

TEST_CASE(QuestionProcessor_non_interactive_missing_required_answer_throws) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"General"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "name",
            .type = "string",
            .prompt = "Name",
        },
    };

    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    REQUIRE_THROWS_AS(processor.collect(manifest, {}, {}, {}, true, false), tempify::TempifyError);
}

TEST_CASE(QuestionProcessor_strict_rejects_unknown_imported_keys) {
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    REQUIRE_THROWS_AS(
        processor.collect(manifest, {}, {}, {{"mystery_key", "value"}}, false, true),
        tempify::TempifyError);
}

TEST_CASE(QuestionProcessor_quit_aborts_collection) {
    tempify::TemplateManifest manifest;
    manifest.question_group_order = {"General"};
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "name",
            .type = "string",
            .prompt = "Name",
        },
    };

    tempify::LuaEngine lua_engine;
    StubFrontend frontend({":quit"});

    tempify::QuestionProcessor processor(lua_engine, frontend);
    REQUIRE_THROWS_AS(processor.collect(manifest, {}), tempify::TempifyError);
}

TEST_CASE(QuestionProcessor_invalid_choice_configuration_and_unknown_type_throw) {
    tempify::LuaEngine lua_engine;
    StubFrontend frontend({});
    tempify::QuestionProcessor processor(lua_engine, frontend);

    tempify::TemplateManifest missing_choices_manifest;
    missing_choices_manifest.question_group_order = {"General"};
    missing_choices_manifest.questions = {
        tempify::QuestionDefinition{
            .key = "provider",
            .type = "choice",
        },
    };
    REQUIRE_THROWS_AS(
        processor.collect(missing_choices_manifest, {{"provider", "1"}}),
        tempify::TempifyError);

    tempify::TemplateManifest unknown_type_manifest;
    unknown_type_manifest.question_group_order = {"General"};
    unknown_type_manifest.questions = {
        tempify::QuestionDefinition{
            .key = "ratio",
            .type = "float",
        },
    };
    REQUIRE_THROWS_AS(
        processor.collect(unknown_type_manifest, {{"ratio", "1.5"}}),
        tempify::TempifyError);
}
