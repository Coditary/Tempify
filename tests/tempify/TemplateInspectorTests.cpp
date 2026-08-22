#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/template/TemplateInspector.h"
#include "tempify/template/TemplateLoader.h"

#include <string>

TEST_CASE(TemplateInspector_describes_loaded_template_structure) {
    const tempify::LuaEngine lua_engine;
    const tempify::TemplateLoader loader(lua_engine);
    const std::filesystem::path template_root =
        tempify::test_support::test_template_path("advanced_hooks_layout");
    const tempify::TemplateManifest manifest =
        loader.load(template_root, {{"advanced_hooks_layout", template_root}});

    const std::string text = tempify::inspect_template_text(manifest);
    REQUIRE(text.find("advanced_hooks_layout") != std::string::npos);
    REQUIRE(text.find("Source Roots:") != std::string::npos);
    REQUIRE(text.find("Files:") != std::string::npos);
    REQUIRE(text.find("Questions:") != std::string::npos);
    REQUIRE(text.find("use_notes") != std::string::npos);
    REQUIRE(text.find("Hooks:") != std::string::npos);
}
