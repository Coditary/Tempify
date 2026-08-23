#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/lua/LuaEngine.h"

#include <filesystem>
#include <map>
#include <string>

namespace {

tempify::QuestionDefinition find_question(const tempify::TemplateManifest &manifest, const std::string &key) {
    for (const auto &question : manifest.questions) {
        if (question.key == key) {
            return question;
        }
    }
    throw std::runtime_error("question not found");
}

tempify::TemplateManifest load_basic_cpp_manifest() {
    tempify::LuaEngine lua_engine;
    return lua_engine.load_partial_manifest(tempify::test_support::test_template_path("basic_cpp"));
}

} // namespace

TEST_CASE(LuaEngine_evaluate_default_condition_and_validate_for_questions) {
    tempify::LuaEngine lua_engine;
    const tempify::TemplateManifest manifest = load_basic_cpp_manifest();

    REQUIRE_EQ(manifest.question_group_order.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(manifest.question_group_order[0], std::string("Project"));
    REQUIRE_EQ(manifest.question_group_order[1], std::string("CI"));

    const tempify::QuestionDefinition slug = find_question(manifest, "project_slug");
    const tempify::QuestionDefinition provider = find_question(manifest, "ci_provider");
    const tempify::QuestionDefinition docs = find_question(manifest, "docs_url");

    std::map<std::string, std::string> values{{"project_name", "Stone App"}, {"include_ci", "true"}};

    const auto slug_value = lua_engine.evaluate_default(slug, values);
    REQUIRE_EQ(REQUIRE_VALUE(slug_value), std::string("stone-app"));

    REQUIRE(lua_engine.evaluate_condition(provider, values));
    values["include_ci"] = "false";
    REQUIRE(!lua_engine.evaluate_condition(provider, values));

    const auto invalid = lua_engine.validate_answer(docs, "ftp://bad", values);
    REQUIRE(invalid.has_value());
    const auto valid = lua_engine.validate_answer(docs, "https://docs.example", values);
    REQUIRE(!valid.has_value());
}
