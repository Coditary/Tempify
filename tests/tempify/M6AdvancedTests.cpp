#include "TestHarness.h"

#include "tempify/app/TempifyApp.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/template/TemplateLoader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

class ScopedDirectoryCleanup {
public:
    explicit ScopedDirectoryCleanup(std::filesystem::path path)
        : path_(std::move(path)) {
        std::filesystem::remove_all(path_);
    }

    ~ScopedDirectoryCleanup() {
        std::filesystem::remove_all(path_);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

}

TEST_CASE(TempifyApp_m6_layout_scripts_and_hook_lifecycle_work) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-m6-advanced-test");
    tempify::TempifyApp app;

    REQUIRE_EQ(app.run({
        "m6_advanced",
        target.path().string(),
        "--set", "project_name=Advanced Demo",
        "--set", "project_slug=advanced-demo",
        "--set", "use_notes=false",
    }), 0);

    REQUIRE(std::filesystem::exists(target.path() / "README.md"));
    REQUIRE(std::filesystem::exists(target.path() / "static" / "output.txt"));
    REQUIRE(!std::filesystem::exists(target.path() / "notes" / "todo.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "pre.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "before-render.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "after-render.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "post.txt"));
    REQUIRE(std::filesystem::exists(target.path() / "script-marker.txt"));

    REQUIRE(read_text_file(target.path() / "README.md").find("Advanced Demo") != std::string::npos);
    REQUIRE(read_text_file(target.path() / "static" / "output.txt").find("{{ project_name }}") != std::string::npos);
}

TEST_CASE(LuaEngine_export_questions_json_returns_minimal_question_payload) {
    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);

    const tempify::TemplateManifest manifest = loader.load(std::filesystem::path{"templates/m6_advanced"}, {
        {"m6_advanced", std::filesystem::path{"templates/m6_advanced"}},
    });

    const std::string json = lua_engine.export_questions_json(manifest);
    REQUIRE(json.find("\"template\"") != std::string::npos);
    REQUIRE(json.find("\"order\": [\"Project\"]") != std::string::npos);
    REQUIRE(json.find("\"questions\": {") != std::string::npos);
    REQUIRE(json.find("\"Project\": [") != std::string::npos);
    REQUIRE(json.find("\"project_name\"") != std::string::npos);
    REQUIRE(json.find("\"use_notes\"") != std::string::npos);
    REQUIRE(json.find("\"choices\"") == std::string::npos);
}

TEST_CASE(LuaEngine_export_questions_json_full_keeps_empty_fields) {
    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);

    const tempify::TemplateManifest manifest = loader.load(std::filesystem::path{"templates/m6_advanced"}, {
        {"m6_advanced", std::filesystem::path{"templates/m6_advanced"}},
    });

    const std::string json = lua_engine.export_questions_json(manifest, true);
    REQUIRE(json.find("\"choices\": []") != std::string::npos);
    REQUIRE(json.find("\"help\": \"\"") != std::string::npos);
    REQUIRE(json.find("\"optional\": false") != std::string::npos);
}

TEST_CASE(LuaEngine_export_questions_json_includes_sensitive_flag) {
    ScopedDirectoryCleanup temp_root(std::filesystem::temp_directory_path() / "tempify-sensitive-questions-json-test");
    const std::filesystem::path template_root = temp_root.path() / "sensitive_tpl";
    std::filesystem::create_directories(template_root / "files");

    {
        std::ofstream output(template_root / "template.lua", std::ios::binary);
        output << "return {\n"
                  "  id = \"sensitive_tpl\",\n"
                  "  name = \"Sensitive Template\",\n"
                  "  version = \"1.0.0\",\n"
                  "  source_dir = \"files\",\n"
                  "  output = { path = \"out\", overwrite = false },\n"
                  "}\n";
    }
    {
        std::ofstream output(template_root / "questions.lua", std::ios::binary);
        output << "return {\n"
                  "  order = { \"Secrets\" },\n"
                  "  groups = {\n"
                  "    Secrets = {\n"
                  "      { key = \"api_token\", type = \"string\", prompt = \"API token\", sensitive = true },\n"
                  "    },\n"
                  "  },\n"
                  "}\n";
    }
    {
        std::ofstream output(template_root / "files" / "README.md.pbt", std::ios::binary);
        output << "# test\n";
    }

    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    const tempify::TemplateManifest manifest = loader.load(template_root, {
        {"sensitive_tpl", template_root},
    });

    REQUIRE(manifest.questions.size() == static_cast<std::size_t>(1));
    REQUIRE(manifest.questions.front().sensitive);

    const std::string json = lua_engine.export_questions_json(manifest, true);
    REQUIRE(json.find("\"sensitive\": true") != std::string::npos);
}
