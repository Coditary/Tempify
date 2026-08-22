#include "TestHarness.h"
#include "tempify/template/TemplateLinter.h"

#include <string>

namespace {

tempify::TemplateManifest make_manifest() {
    tempify::TemplateManifest manifest;
    manifest.info.id = "lint_sample";
    manifest.info.name = "Lint Sample";
    manifest.root = std::filesystem::path("/tmp/lint-sample");
    manifest.source_roots.push_back({
        .path = manifest.root / "files",
        .template_id = "lint_sample",
    });
    return manifest;
}

} // namespace

TEST_CASE(TemplateLinter_warns_about_missing_metadata_and_question_details) {
    tempify::TemplateManifest manifest = make_manifest();
    manifest.info.description.clear();
    manifest.info.version.clear();
    manifest.questions.push_back({
        .key = "project_name",
        .type = "string",
        .prompt = "",
    });
    manifest.questions.push_back({
        .key = "mode",
        .type = "choice",
        .prompt = "Mode",
        .choices = {},
    });
    manifest.questions.push_back({
        .key = "notes",
        .type = "string",
        .prompt = "Notes",
        .optional = true,
        .help = "",
    });

    const tempify::TemplateLinter linter;
    const std::vector<std::string> warnings = linter.lint(manifest);
    REQUIRE(warnings.size() >= 4U);
    REQUIRE(warnings[0].find("description is empty") != std::string::npos);
    REQUIRE(warnings[1].find("version is empty") != std::string::npos);

    bool found_prompt_warning = false;
    bool found_choice_warning = false;
    bool found_optional_help_warning = false;
    for (const std::string &warning : warnings) {
        if (warning.find("missing prompt") != std::string::npos) {
            found_prompt_warning = true;
        }
        if (warning.find("choice question has no choices") != std::string::npos) {
            found_choice_warning = true;
        }
        if (warning.find("optional question missing help text") != std::string::npos) {
            found_optional_help_warning = true;
        }
    }
    REQUIRE(found_prompt_warning);
    REQUIRE(found_choice_warning);
    REQUIRE(found_optional_help_warning);
}

TEST_CASE(TemplateLinter_warns_about_rendered_file_without_pbt_extension) {
    tempify::TemplateManifest manifest = make_manifest();
    manifest.info.description = "desc";
    manifest.info.version = "1.0.0";
    manifest.files.push_back({
        .relative_path = "README.md",
        .source_path = manifest.root / "files" / "README.md",
        .render_with_prebyte = true,
        .source_template_id = "lint_sample",
    });

    const tempify::TemplateLinter linter;
    const std::vector<std::string> warnings = linter.lint(manifest);
    REQUIRE(!warnings.empty());
    REQUIRE(warnings.front().find("does not use .pbt extension") != std::string::npos);
}

TEST_CASE(format_template_lint_text_includes_template_id_and_warnings) {
    const std::vector<std::string> warnings = {"template description is empty"};
    const std::string text = tempify::format_template_lint_text("lint_sample", warnings);
    REQUIRE(text.find("lint_sample") != std::string::npos);
    REQUIRE(text.find("template description is empty") != std::string::npos);
}
