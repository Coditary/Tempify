#include "TestHarness.h"
#include "tempify/cli/CliParser.h"
#include "tempify/domain/CliRequest.h"
#include "tempify/support/Errors.h"

#include <string>

TEST_CASE(CliParser_parses_q_refresh_prebyte_info_doctor_validate_inspect_lint_test_and_render_flags) {
    tempify::CliParser parser;

    const tempify::CliRequest questions_alias = parser.parse({"advanced_hooks_layout", "-q", "--json", "--full"});
    REQUIRE(questions_alias.mode == tempify::CliMode::QuestionsShow);
    REQUIRE_EQ(questions_alias.template_ref, std::string("advanced_hooks_layout"));
    REQUIRE(questions_alias.questions_output_format == tempify::QuestionsOutputFormat::Json);
    REQUIRE(questions_alias.questions_full);

    const tempify::CliRequest questions_long_alias = parser.parse({"advanced_hooks_layout", "--questions", "--json"});
    REQUIRE(questions_long_alias.mode == tempify::CliMode::QuestionsShow);
    REQUIRE_EQ(questions_long_alias.template_ref, std::string("advanced_hooks_layout"));
    REQUIRE(questions_long_alias.questions_output_format == tempify::QuestionsOutputFormat::Json);
    REQUIRE(!questions_long_alias.questions_full);

    const tempify::CliRequest refresh = parser.parse({"refresh"});
    REQUIRE(refresh.mode == tempify::CliMode::Refresh);
    REQUIRE(!refresh.refresh_json);

    const tempify::CliRequest refresh_json = parser.parse({"refresh", "--json"});
    REQUIRE(refresh_json.mode == tempify::CliMode::Refresh);
    REQUIRE(refresh_json.refresh_json);

    const tempify::CliRequest list = parser.parse({"list"});
    REQUIRE(list.mode == tempify::CliMode::TemplateList);
    REQUIRE(!list.list_json);

    const tempify::CliRequest list_json_catalog = parser.parse({"list", "--json"});
    REQUIRE(list_json_catalog.mode == tempify::CliMode::TemplateList);
    REQUIRE(list_json_catalog.list_json);

    const tempify::CliRequest info = parser.parse({"info", "basic_cpp"});
    REQUIRE(info.mode == tempify::CliMode::TemplateInfo);
    REQUIRE_EQ(info.template_ref, std::string("basic_cpp"));
    REQUIRE(!info.info_json);

    const tempify::CliRequest info_json = parser.parse({"info", "basic_cpp", "--json"});
    REQUIRE(info_json.mode == tempify::CliMode::TemplateInfo);
    REQUIRE(info_json.info_json);

    const tempify::CliRequest doctor = parser.parse({"doctor"});
    REQUIRE(doctor.mode == tempify::CliMode::Doctor);
    REQUIRE(!doctor.doctor_json);

    const tempify::CliRequest doctor_json = parser.parse({"doctor", "--json"});
    REQUIRE(doctor_json.mode == tempify::CliMode::Doctor);
    REQUIRE(doctor_json.doctor_json);

    const tempify::CliRequest completion = parser.parse({"completion", "bash"});
    REQUIRE(completion.mode == tempify::CliMode::Completion);
    REQUIRE(completion.completion_shell.has_value());
    REQUIRE_EQ(*completion.completion_shell, std::string("bash"));

    const tempify::CliRequest validate = parser.parse({"validate", "basic_cpp"});
    REQUIRE(validate.mode == tempify::CliMode::TemplateValidate);
    REQUIRE_EQ(validate.template_ref, std::string("basic_cpp"));
    REQUIRE(!validate.validate_json);

    const tempify::CliRequest validate_json = parser.parse({"validate", "basic_cpp", "--json"});
    REQUIRE(validate_json.mode == tempify::CliMode::TemplateValidate);
    REQUIRE(validate_json.validate_json);

    const tempify::CliRequest inspect = parser.parse({"inspect", "layered_cpp_product"});
    REQUIRE(inspect.mode == tempify::CliMode::TemplateInspect);
    REQUIRE_EQ(inspect.template_ref, std::string("layered_cpp_product"));
    REQUIRE(!inspect.inspect_json);

    const tempify::CliRequest inspect_json = parser.parse({"inspect", "layered_cpp_product", "--json"});
    REQUIRE(inspect_json.mode == tempify::CliMode::TemplateInspect);
    REQUIRE(inspect_json.inspect_json);

    const tempify::CliRequest lint = parser.parse({"lint", "layered_cpp_product"});
    REQUIRE(lint.mode == tempify::CliMode::TemplateLint);
    REQUIRE_EQ(lint.template_ref, std::string("layered_cpp_product"));
    REQUIRE(!lint.lint_json);

    const tempify::CliRequest lint_json = parser.parse({"lint", "layered_cpp_product", "--json"});
    REQUIRE(lint_json.mode == tempify::CliMode::TemplateLint);
    REQUIRE(lint_json.lint_json);

    const tempify::CliRequest test = parser.parse({"test", "basic_cpp", "--fixture", "ci_enabled", "--json"});
    REQUIRE(test.mode == tempify::CliMode::TemplateTest);
    REQUIRE_EQ(test.template_ref, std::string("basic_cpp"));
    REQUIRE(test.test_fixture_name.has_value());
    REQUIRE_EQ(*test.test_fixture_name, std::string("ci_enabled"));
    REQUIRE(!test.test_list_fixtures);
    REQUIRE(test.test_json);
    REQUIRE(!test.test_update_snapshots);

    const tempify::CliRequest list_fixtures = parser.parse({"test", "basic_cpp", "--list-fixtures"});
    REQUIRE(list_fixtures.mode == tempify::CliMode::TemplateTest);
    REQUIRE(list_fixtures.test_list_fixtures);
    REQUIRE(!list_fixtures.test_fixture_name.has_value());

    const tempify::CliRequest list_json = parser.parse({"test", "basic_cpp", "--json", "--list-fixtures"});
    REQUIRE(list_json.mode == tempify::CliMode::TemplateTest);
    REQUIRE(list_json.test_json);
    REQUIRE(list_json.test_list_fixtures);

    const tempify::CliRequest update =
        parser.parse({"test", "basic_cpp", "--update-snapshots", "--fixture", "ci_enabled"});
    REQUIRE(update.mode == tempify::CliMode::TemplateTest);
    REQUIRE(update.test_update_snapshots);
    REQUIRE(update.test_fixture_name.has_value());

    const tempify::CliRequest prebyte = parser.parse({"-p", "-h", "render"});
    REQUIRE(prebyte.mode == tempify::CliMode::Help);
    REQUIRE(prebyte.help_topic.has_value());
    REQUIRE(prebyte.help_topic == tempify::HelpTopic::Process);

    const tempify::CliRequest process = parser.parse({"process", "render", "--flag"});
    REQUIRE(process.mode == tempify::CliMode::PrebytePassthrough);
    REQUIRE_EQ(process.raw_prebyte_args.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(process.raw_prebyte_args[0], std::string("render"));
    REQUIRE_EQ(process.raw_prebyte_args[1], std::string("--flag"));

    const tempify::CliRequest render = parser.parse({
        "basic_cpp",
        "out-dir",
        "--set",
        "project_name=App",
        "--var",
        "namespace=core",
        "-f",
        "--accept-hooks",
        "ask",
        "--answers",
        "answers.json",
        "--non-interactive",
        "--strict",
        "--diff",
        "--json",
        "--hook-timeout-ms",
        "250",
        "--tui",
    });
    REQUIRE(render.mode == tempify::CliMode::TemplateRender);
    REQUIRE_EQ(render.template_ref, std::string("basic_cpp"));
    REQUIRE(render.target_dir.has_value());
    REQUIRE_EQ(render.target_dir->string(), std::string("out-dir"));
    REQUIRE_EQ(render.variables.at("project_name"), std::string("App"));
    REQUIRE_EQ(render.variables.at("namespace"), std::string("core"));
    REQUIRE(render.existing_path_behavior_override.has_value());
    REQUIRE(render.existing_path_behavior_override == tempify::ExistingPathBehavior::Overwrite);
    REQUIRE(render.hook_acceptance == tempify::HookAcceptance::Ask);
    REQUIRE(render.answers_file.has_value());
    REQUIRE_EQ(render.answers_file->string(), std::string("answers.json"));
    REQUIRE(!render.write_answers_file.has_value());
    REQUIRE(render.non_interactive);
    REQUIRE(render.strict);
    REQUIRE(render.diff_only);
    REQUIRE(render.diff_json);
    REQUIRE_EQ(render.hook_timeout_ms, 250);
    REQUIRE(!render.dry_run);
    REQUIRE(!render.plan_json);
    REQUIRE(render.use_tui);

    const tempify::CliRequest reapply = parser.parse({
        "basic_cpp",
        "out-dir",
        "--reapply",
        "--json",
        "--set",
        "project_name=App",
    });
    REQUIRE(reapply.mode == tempify::CliMode::TemplateRender);
    REQUIRE(reapply.reapply);
    REQUIRE(!reapply.diff_only);
    REQUIRE(reapply.diff_json);

    const tempify::CliRequest reapply_report = parser.parse({
        "basic_cpp",
        "out-dir",
        "--reapply",
        "--report",
        "--json",
        "--set",
        "project_name=App",
    });
    REQUIRE(reapply_report.mode == tempify::CliMode::TemplateRender);
    REQUIRE(reapply_report.reapply);
    REQUIRE(reapply_report.reapply_report);
    REQUIRE(reapply_report.diff_json);

    const tempify::CliRequest reapply_subcommand = parser.parse({
        "reapply",
        "basic_cpp",
        "out-dir",
        "--json",
        "--set",
        "project_name=App",
    });
    REQUIRE(reapply_subcommand.mode == tempify::CliMode::TemplateRender);
    REQUIRE_EQ(reapply_subcommand.template_ref, std::string("basic_cpp"));
    REQUIRE(reapply_subcommand.target_dir.has_value());
    REQUIRE_EQ(reapply_subcommand.target_dir->string(), std::string("out-dir"));
    REQUIRE(reapply_subcommand.reapply);
    REQUIRE(reapply_subcommand.diff_json);
    REQUIRE_EQ(reapply_subcommand.variables.at("project_name"), std::string("App"));

    const tempify::CliRequest reapply_subcommand_report = parser.parse({
        "reapply",
        "basic_cpp",
        "out-dir",
        "--report",
        "--json",
    });
    REQUIRE(reapply_subcommand_report.mode == tempify::CliMode::TemplateRender);
    REQUIRE(reapply_subcommand_report.reapply);
    REQUIRE(reapply_subcommand_report.reapply_report);
    REQUIRE(reapply_subcommand_report.diff_json);

    const tempify::CliRequest skip = parser.parse({"basic_cpp", "out-dir", "-s", "--accept-hooks", "no"});
    REQUIRE(skip.existing_path_behavior_override.has_value());
    REQUIRE(skip.existing_path_behavior_override == tempify::ExistingPathBehavior::Skip);
    REQUIRE(skip.hook_acceptance == tempify::HookAcceptance::No);

    const tempify::CliRequest no_hooks_alias = parser.parse({"basic_cpp", "out-dir", "--no-hooks"});
    REQUIRE(!no_hooks_alias.existing_path_behavior_override.has_value());
    REQUIRE(no_hooks_alias.hook_acceptance == tempify::HookAcceptance::No);
}

TEST_CASE(CliParser_routes_all_help_topics) {
    tempify::CliParser parser;

    const tempify::CliRequest top = parser.parse({"--help"});
    REQUIRE(top.mode == tempify::CliMode::Help);
    REQUIRE(top.help_topic.has_value());
    REQUIRE(top.help_topic == tempify::HelpTopic::TopLevel);

    const tempify::CliRequest list = parser.parse({"list", "-h"});
    REQUIRE(list.mode == tempify::CliMode::Help);
    REQUIRE(list.help_topic == tempify::HelpTopic::List);

    const tempify::CliRequest info = parser.parse({"info", "--help"});
    REQUIRE(info.mode == tempify::CliMode::Help);
    REQUIRE(info.help_topic == tempify::HelpTopic::Info);

    const tempify::CliRequest doctor = parser.parse({"doctor", "-h"});
    REQUIRE(doctor.mode == tempify::CliMode::Help);
    REQUIRE(doctor.help_topic == tempify::HelpTopic::Doctor);

    const tempify::CliRequest completion = parser.parse({"completion", "--help"});
    REQUIRE(completion.mode == tempify::CliMode::Help);
    REQUIRE(completion.help_topic == tempify::HelpTopic::Completion);

    const tempify::CliRequest validate = parser.parse({"validate", "--help"});
    REQUIRE(validate.mode == tempify::CliMode::Help);
    REQUIRE(validate.help_topic == tempify::HelpTopic::Validate);

    const tempify::CliRequest inspect = parser.parse({"inspect", "-h"});
    REQUIRE(inspect.mode == tempify::CliMode::Help);
    REQUIRE(inspect.help_topic == tempify::HelpTopic::Inspect);

    const tempify::CliRequest lint = parser.parse({"lint", "--help"});
    REQUIRE(lint.mode == tempify::CliMode::Help);
    REQUIRE(lint.help_topic == tempify::HelpTopic::Lint);

    const tempify::CliRequest test = parser.parse({"test", "-h"});
    REQUIRE(test.mode == tempify::CliMode::Help);
    REQUIRE(test.help_topic == tempify::HelpTopic::Test);

    const tempify::CliRequest refresh = parser.parse({"refresh", "--help"});
    REQUIRE(refresh.mode == tempify::CliMode::Help);
    REQUIRE(refresh.help_topic == tempify::HelpTopic::Refresh);

    const tempify::CliRequest reapply = parser.parse({"reapply", "--help"});
    REQUIRE(reapply.mode == tempify::CliMode::Help);
    REQUIRE(reapply.help_topic == tempify::HelpTopic::Render);
    REQUIRE(reapply.reapply);

    const tempify::CliRequest reapply_with_args = parser.parse({"reapply", "basic_cpp", "out-dir", "-h"});
    REQUIRE(reapply_with_args.mode == tempify::CliMode::Help);
    REQUIRE(reapply_with_args.help_topic == tempify::HelpTopic::Render);
    REQUIRE(reapply_with_args.reapply);
    REQUIRE_EQ(reapply_with_args.template_ref, std::string("basic_cpp"));
    REQUIRE(reapply_with_args.target_dir.has_value());
    REQUIRE_EQ(reapply_with_args.target_dir->string(), std::string("out-dir"));

    const tempify::CliRequest questions = parser.parse({"basic_cpp", "-q", "--help"});
    REQUIRE(questions.mode == tempify::CliMode::Help);
    REQUIRE(questions.help_topic == tempify::HelpTopic::Questions);
    REQUIRE_EQ(questions.template_ref, std::string("basic_cpp"));

    const tempify::CliRequest questions_long = parser.parse({"basic_cpp", "--questions", "--help"});
    REQUIRE(questions_long.mode == tempify::CliMode::Help);
    REQUIRE(questions_long.help_topic == tempify::HelpTopic::Questions);
    REQUIRE_EQ(questions_long.template_ref, std::string("basic_cpp"));

    const tempify::CliRequest prebyte = parser.parse({"-p", "--help"});
    REQUIRE(prebyte.mode == tempify::CliMode::Help);
    REQUIRE(prebyte.help_topic == tempify::HelpTopic::Process);

    const tempify::CliRequest process = parser.parse({"process", "--help"});
    REQUIRE(process.mode == tempify::CliMode::Help);
    REQUIRE(process.help_topic == tempify::HelpTopic::Process);

    const tempify::CliRequest render = parser.parse({"basic_cpp", "out-dir", "-h"});
    REQUIRE(render.mode == tempify::CliMode::Help);
    REQUIRE(render.help_topic == tempify::HelpTopic::Render);
    REQUIRE_EQ(render.template_ref, std::string("basic_cpp"));
    REQUIRE(render.target_dir.has_value());
    REQUIRE_EQ(render.target_dir->string(), std::string("out-dir"));
}

TEST_CASE(CliParser_rejects_invalid_assignments_unknown_options_and_extra_targets) {
    tempify::CliParser parser;

    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "--set"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "--set", "bad-assignment"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "--wat"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-a", "out-b"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"registry", "bogus"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"questions", "advanced_hooks_layout"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"template", "list"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"schema", "advanced_hooks_layout"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "-q"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--questions"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "-f", "-s"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--accept-hooks", "maybe"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--no-hooks", "--accept-hooks", "yes"}),
                      tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--hook-timeout-ms", "-1"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--json"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--diff", "--plan-json"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--reapply", "--diff"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "--reapply"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"reapply"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"reapply", "basic_cpp"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"basic_cpp", "out-dir", "--report"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(
        parser.parse({"basic_cpp", "out-dir", "--reapply", "--report", "--write-answers", "answers.json"}),
        tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"reapply", "basic_cpp", "out-dir", "--diff"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"reapply", "basic_cpp", "out-dir", "--report", "--write-answers", "answers.json"}),
                      tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"doctor", "--wat"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"completion"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"completion", "pwsh"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"info"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"validate"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"inspect"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"lint"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"test"}), tempify::TempifyError);
    REQUIRE_THROWS_AS(parser.parse({"test", "basic_cpp", "--fixture"}), tempify::TempifyError);
    const tempify::CliRequest list_json = parser.parse({"test", "basic_cpp", "--json", "--list-fixtures"});
    REQUIRE(list_json.mode == tempify::CliMode::TemplateTest);
    REQUIRE(list_json.test_json);
    REQUIRE(list_json.test_list_fixtures);
}
