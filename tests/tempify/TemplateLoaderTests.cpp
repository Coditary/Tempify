#include "TestHarness.h"

#include "tempify/lua/LuaEngine.h"
#include "tempify/template/TemplateLoader.h"
#include "tempify/support/Errors.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

void create_template(const std::filesystem::path& root,
                     const std::string& id,
                     const std::vector<std::string>& includes = {},
                     const std::string& merge_body = {},
                     const std::vector<std::pair<std::string, std::string>>& files = {},
                     const bool with_pre_hook = false) {
    std::filesystem::create_directories(root / "files");
    for (const auto& [relative_path, content] : files) {
        write_text_file(root / "files" / relative_path, content);
    }
    if (with_pre_hook) {
        write_text_file(root / "hooks" / "pre.lua", "write_file('pre.txt', 'hook')\n");
    }

    std::ostringstream stream;
    stream << "return {\n"
           << "  id = \"" << id << "\",\n"
           << "  name = \"" << id << "\",\n"
           << "  version = \"0.1.0\",\n"
           << "  source_dir = \"files\",\n";
    if (!includes.empty()) {
        stream << "  includes = { ";
        for (std::size_t index = 0; index < includes.size(); ++index) {
            if (index > 0) {
                stream << ", ";
            }
            stream << '"' << includes[index] << '"';
        }
        stream << " },\n";
    }
    if (!merge_body.empty()) {
        stream << "  merge = {\n" << merge_body << "  },\n";
    }
    stream << "}\n";
    write_text_file(root / "template.lua", stream.str());
}

std::map<std::string, std::filesystem::path> test_index() {
    return {
        {"m3_lang_base", std::filesystem::path{"templates/m3_lang_base"}},
        {"m3_ci_layer", std::filesystem::path{"templates/m3_ci_layer"}},
        {"m3_product", std::filesystem::path{"templates/m3_product"}},
        {"m3_conflict_parent", std::filesystem::path{"templates/m3_conflict_parent"}},
        {"m3_conflict_child", std::filesystem::path{"templates/m3_conflict_child"}},
    };
}

}

TEST_CASE(TemplateLoader_merge_layers_replace_drop_and_append) {
    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);

    const tempify::TemplateManifest manifest = loader.load(std::filesystem::path{"templates/m3_product"}, test_index());

    REQUIRE_EQ(manifest.info.id, std::string("m3_product"));
    REQUIRE(manifest.files.size() >= static_cast<std::size_t>(3));

    bool has_readme = false;
    bool has_ci = false;
    bool has_base_only = false;
    bool main_from_product = false;
    for (const auto& file : manifest.files) {
        if (file.relative_path == "README.md.pbt") {
            has_readme = true;
            REQUIRE_EQ(file.source_template_id, std::string("m3_ci_layer"));
        }
        if (file.relative_path == ".github/workflows/ci.yml.pbt") {
            has_ci = true;
        }
        if (file.relative_path == "base-only.txt") {
            has_base_only = true;
        }
        if (file.relative_path == "src/main.cpp.pbt") {
            main_from_product = file.source_template_id == "m3_product";
        }
    }

    REQUIRE(has_readme);
    REQUIRE(has_ci);
    REQUIRE(!has_base_only);
    REQUIRE(main_from_product);
}

TEST_CASE(TemplateLoader_file_conflict_error_strategy_throws) {
    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);

    REQUIRE_THROWS_AS(
        loader.load(std::filesystem::path{"templates/m3_conflict_child"}, test_index()),
        tempify::TempifyError);
}

TEST_CASE(TemplateLoader_detects_include_cycle_in_dynamic_templates) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-template-cycle-test");
    const std::filesystem::path a = workspace.path() / "cycle_a";
    const std::filesystem::path b = workspace.path() / "cycle_b";

    create_template(a, "cycle_a", {"cycle_b"});
    create_template(b, "cycle_b", {"cycle_a"});

    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    const std::map<std::string, std::filesystem::path> index{
        {"cycle_a", a},
        {"cycle_b", b},
    };

    REQUIRE_THROWS_AS(loader.load(a, index), tempify::TempifyError);
}

TEST_CASE(TemplateLoader_wildcard_file_conflict_keep_preserves_base_file) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-template-wildcard-merge-test");
    const std::filesystem::path base = workspace.path() / "wild_base";
    const std::filesystem::path child = workspace.path() / "wild_child";

    create_template(base, "wild_base", {}, {}, {{"README.md.pbt", "base readme\n"}});
    create_template(
        child,
        "wild_child",
        {"wild_base"},
        "    file_conflicts = { [\"*.pbt\"] = \"keep\" },\n",
        {{"README.md.pbt", "child readme\n"}});

    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    const std::map<std::string, std::filesystem::path> index{
        {"wild_base", base},
        {"wild_child", child},
    };

    const tempify::TemplateManifest manifest = loader.load(child, index);

    bool found = false;
    for (const auto& file : manifest.files) {
        if (file.relative_path == "README.md.pbt") {
            found = true;
            REQUIRE_EQ(file.source_template_id, std::string("wild_base"));
        }
    }
    REQUIRE(found);
}

TEST_CASE(TemplateLoader_pre_hook_conflict_error_for_included_templates_throws) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-template-hook-conflict-test");
    const std::filesystem::path first = workspace.path() / "hook_first";
    const std::filesystem::path second = workspace.path() / "hook_second";
    const std::filesystem::path root = workspace.path() / "hook_root";

    create_template(first, "hook_first", {}, {}, {}, true);
    create_template(second, "hook_second", {}, {}, {}, true);
    create_template(
        root,
        "hook_root",
        {"hook_first", "hook_second"},
        "    pre_hook_conflict = \"error\",\n");

    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    const std::map<std::string, std::filesystem::path> index{
        {"hook_first", first},
        {"hook_second", second},
        {"hook_root", root},
    };

    REQUIRE_THROWS_AS(loader.load(root, index), tempify::TempifyError);
}
