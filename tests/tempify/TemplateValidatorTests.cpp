#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/support/Errors.h"
#include "tempify/template/TemplateLoader.h"
#include "tempify/template/TemplateValidator.h"

#include <filesystem>
#include <string>

namespace {

tempify::TemplateManifest load_manifest(const std::filesystem::path &template_root, const std::string &template_id) {
    const tempify::LuaEngine lua_engine;
    const tempify::TemplateLoader loader(lua_engine);
    return loader.load(template_root, {{template_id, template_root}});
}

} // namespace

TEST_CASE(TemplateValidator_accepts_known_good_template) {
    const tempify::TemplateValidator validator;
    const tempify::TemplateManifest manifest =
        load_manifest(tempify::test_support::test_template_path("advanced_hooks_layout"), "advanced_hooks_layout");
    validator.validate(manifest);
}

TEST_CASE(TemplateValidator_rejects_duplicate_question_keys) {
    tempify::TemplateManifest manifest;
    manifest.questions.push_back({
        .key = "project_name",
        .type = "string",
        .prompt = "Name",
    });
    manifest.questions.push_back({
        .key = "project_name",
        .type = "string",
        .prompt = "Duplicate",
        .source_path = std::filesystem::path("questions.lua"),
        .source_index = 2,
    });

    const tempify::TemplateValidator validator;
    REQUIRE_THROWS_AS(validator.validate(manifest), tempify::TempifyError);
}

TEST_CASE(TemplateValidator_rejects_layout_rule_for_missing_source) {
    tempify::TemplateManifest manifest;
    manifest.files.push_back({
        .relative_path = "README.md",
        .source_path = std::filesystem::path("/tmp/README.md"),
        .render_with_prebyte = false,
        .source_template_id = "sample",
    });
    manifest.layout_rules.push_back({
        .source = "missing.txt",
        .source_template_id = "sample",
    });

    const tempify::TemplateValidator validator;
    REQUIRE_THROWS_AS(validator.validate(manifest), tempify::TempifyError);
}

TEST_CASE(TemplateValidator_rejects_missing_hook_file) {
    tempify::TemplateManifest manifest;
    manifest.pre_hook_path = std::filesystem::path("/tmp/tempify-missing-pre-hook.lua");

    const tempify::TemplateValidator validator;
    REQUIRE_THROWS_AS(validator.validate(manifest), tempify::TempifyError);
}
