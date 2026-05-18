#include "TestHarness.h"

#include "tempify/config/TempifyConfig.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/hook/HookTrustStore.h"
#include "tempify/store/LocalTemplateStore.h"
#include "tempify/support/Errors.h"
#include "tempify/support/Paths.h"
#include "tempify/template/TemplateLoader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

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

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

void create_shared_template(const std::filesystem::path& shared_root,
                            const std::string& template_id,
                            const std::string& template_name,
                            const std::string& version,
                            const std::string& description) {
    const std::filesystem::path template_root = shared_root / "templates" / template_id;
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"" + template_id + "\",\n"
                    "  name = \"" + template_name + "\",\n"
                    "  version = \"" + version + "\",\n"
                    "  description = \"" + description + "\",\n"
                    "  source_dir = \"files\"\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# shared\n");
}

}

TEST_CASE(Paths_resolve_tempify_data_root_prefers_xdg_and_workspace_templates_root_is_optional) {
    ScopedDirectoryCleanup data_home(std::filesystem::temp_directory_path() / "tempify-paths-data-home-test");
    prebyte::test::ScopedEnvironmentVariable env("XDG_DATA_HOME", data_home.path().string());
    ScopedDirectoryCleanup config_home(std::filesystem::temp_directory_path() / "tempify-paths-config-home-test");
    prebyte::test::ScopedEnvironmentVariable config_env("XDG_CONFIG_HOME", config_home.path().string());

    REQUIRE_EQ(tempify::resolve_tempify_data_root(), data_home.path() / "tempify");
    REQUIRE_EQ(tempify::resolve_tempify_config_root(), config_home.path() / "tempify");
    REQUIRE_EQ(tempify::default_global_config_file_path(), config_home.path() / "tempify" / "config.json");

    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-paths-workspace-test");
    std::filesystem::create_directories(workspace.path() / "nested" / "deep");
    REQUIRE(!tempify::find_workspace_templates_root(workspace.path() / "nested").has_value());
    REQUIRE(!tempify::find_workspace_config_file(workspace.path() / "nested").has_value());

    std::filesystem::create_directories(workspace.path() / "templates");
    std::filesystem::create_directories(workspace.path() / ".tempify");
    write_text_file(workspace.path() / ".tempify" / "config.json", "{}\n");
    const auto templates_root = tempify::find_workspace_templates_root(workspace.path() / "nested" / "deep");
    const auto config_file = tempify::find_workspace_config_file(workspace.path() / "nested" / "deep");
    REQUIRE(templates_root.has_value());
    REQUIRE(config_file.has_value());
    REQUIRE_EQ(*templates_root, workspace.path() / "templates");
    REQUIRE_EQ(*config_file, workspace.path() / ".tempify" / "config.json");
}

TEST_CASE(LocalTemplateStore_missing_index_returns_empty) {
    ScopedDirectoryCleanup shared_root(std::filesystem::temp_directory_path() / "tempify-store-missing-index-test");
    tempify::LocalTemplateStore store(shared_root.path());

    REQUIRE(store.list_templates().empty());
    REQUIRE(!store.find_template("missing").has_value());
}

TEST_CASE(LocalTemplateStore_refresh_creates_empty_index_when_no_templates_exist) {
    ScopedDirectoryCleanup shared_root(std::filesystem::temp_directory_path() / "tempify-store-empty-refresh-test");
    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    tempify::LocalTemplateStore store(shared_root.path());

    REQUIRE_EQ(store.refresh(loader), static_cast<std::size_t>(0));
    REQUIRE(std::filesystem::is_regular_file(store.index_file()));
    REQUIRE(read_text_file(store.index_file()).find("\"templates\": [") != std::string::npos);
    REQUIRE(store.list_templates().empty());
}

TEST_CASE(LocalTemplateStore_refresh_writes_index_and_lists_shared_templates) {
    ScopedDirectoryCleanup shared_root(std::filesystem::temp_directory_path() / "tempify-store-refresh-test");
    create_shared_template(shared_root.path(), "sample_tpl", "Sample Template", "1.2.3", "Stored locally");

    tempify::LuaEngine lua_engine;
    tempify::TemplateLoader loader(lua_engine);
    tempify::LocalTemplateStore store(shared_root.path());

    REQUIRE_EQ(store.refresh(loader), static_cast<std::size_t>(1));

    const auto entries = store.list_templates();
    REQUIRE_EQ(entries.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(entries[0].id, std::string("sample_tpl"));
    REQUIRE_EQ(entries[0].name, std::string("Sample Template"));
    REQUIRE_EQ(entries[0].description, std::string("Stored locally"));
    REQUIRE_EQ(entries[0].version, std::string("1.2.3"));
    REQUIRE_EQ(entries[0].path, std::filesystem::absolute(shared_root.path() / "templates" / "sample_tpl"));
    REQUIRE(read_text_file(store.index_file()).find("sample_tpl") != std::string::npos);
}

TEST_CASE(TempifyConfig_load_and_merge_support_defaults_and_render_overlays) {
    ScopedDirectoryCleanup root(std::filesystem::temp_directory_path() / "tempify-config-load-merge-test");
    const std::filesystem::path global_path = root.path() / "global.json";
    const std::filesystem::path workspace_path = root.path() / "workspace.json";

    write_text_file(global_path,
                    "{\n"
                    "  \"defaults\": {\n"
                    "    \"project_name\": \"Global App\",\n"
                    "    \"include_ci\": false\n"
                    "  },\n"
                    "  \"render\": {\n"
                    "    \"accept_hooks\": \"no\",\n"
                    "    \"hook_timeout_ms\": 1234\n"
                    "  }\n"
                    "}\n");
    write_text_file(workspace_path,
                    "{\n"
                    "  \"defaults\": {\n"
                    "    \"project_name\": \"Workspace App\",\n"
                    "    \"author\": \"Workspace Author\"\n"
                    "  },\n"
                    "  \"render\": {\n"
                    "    \"accept_hooks\": \"yes\",\n"
                    "    \"existing_path_behavior\": \"skip\"\n"
                    "  }\n"
                    "}\n");

    const tempify::TempifyConfig global = tempify::load_tempify_config_file(global_path);
    const tempify::TempifyConfig workspace = tempify::load_tempify_config_file(workspace_path);
    const tempify::TempifyConfig merged = tempify::merge_tempify_config(global, workspace);

    REQUIRE_EQ(merged.defaults.at("project_name"), std::string("Workspace App"));
    REQUIRE_EQ(merged.defaults.at("include_ci"), std::string("false"));
    REQUIRE_EQ(merged.defaults.at("author"), std::string("Workspace Author"));
    REQUIRE(merged.hook_acceptance.has_value());
    REQUIRE(*merged.hook_acceptance == tempify::HookAcceptance::Yes);
    REQUIRE(merged.hook_timeout_ms.has_value());
    REQUIRE_EQ(*merged.hook_timeout_ms, 1234);
    REQUIRE(merged.existing_path_behavior.has_value());
    REQUIRE(*merged.existing_path_behavior == tempify::ExistingPathBehavior::Skip);
}

TEST_CASE(TempifyConfig_unknown_keys_throw) {
    ScopedDirectoryCleanup root(std::filesystem::temp_directory_path() / "tempify-config-unknown-key-test");
    const std::filesystem::path config_path = root.path() / "bad.json";
    write_text_file(config_path,
                    "{\n"
                    "  \"mystery\": true\n"
                    "}\n");

    REQUIRE_THROWS_AS(tempify::load_tempify_config_file(config_path), tempify::TempifyError);
}

TEST_CASE(HookTrustStore_persists_trusted_template_roots) {
    ScopedDirectoryCleanup data_home(std::filesystem::temp_directory_path() / "tempify-hook-trust-store-test");
    const std::filesystem::path store_path = tempify::default_hook_trust_store_path(data_home.path());
    tempify::HookTrustStore trust_store(store_path);
    const std::filesystem::path trusted_root = data_home.path() / "templates" / "trusted";
    std::filesystem::create_directories(trusted_root);

    REQUIRE(!trust_store.is_trusted(trusted_root));
    trust_store.trust(trusted_root);
    REQUIRE(trust_store.is_trusted(trusted_root));
    REQUIRE(read_text_file(store_path).find("trusted_templates") != std::string::npos);
    REQUIRE(read_text_file(store_path).find(std::filesystem::weakly_canonical(trusted_root).string()) != std::string::npos);
}
