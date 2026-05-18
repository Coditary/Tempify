#include "tempify/lua/LuaEngine.h"

#include "LuaEngineInternal.h"

#include "tempify/support/EnvLoader.h"
#include "tempify/support/Errors.h"

#include <algorithm>

namespace tempify {

namespace {

ConflictStrategy parse_conflict_strategy(const std::string& value,
                                         const std::filesystem::path& path,
                                         const std::string& field_name) {
    const std::string lowered = lua_internal::lower_copy(lua_internal::trim_copy(value));
    if (lowered == "replace") {
        return ConflictStrategy::Replace;
    }
    if (lowered == "keep") {
        return ConflictStrategy::Keep;
    }
    if (lowered == "error") {
        return ConflictStrategy::Error;
    }

    lua_internal::throw_lua_error(path, "Unsupported conflict strategy in '" + field_name + "': " + value);
}

TemplateMergeConfig load_merge_config_from_stack(lua_State* state,
                                                 const int table_index,
                                                 const std::filesystem::path& path) {
    TemplateMergeConfig config;

    lua_getfield(state, table_index, "file_conflicts");
    if (lua_istable(state, -1) != 0) {
        const int conflicts_index = lua_absindex(state, -1);
        lua_pushnil(state);
        while (lua_next(state, conflicts_index) != 0) {
            if (lua_isstring(state, -2) != 0) {
                const std::string key = lua_tostring(state, -2);
                const std::string value = lua_internal::value_to_string(state, -1, path);
                config.file_conflicts[key] = parse_conflict_strategy(value, path, "file_conflicts");
            }
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "drop_paths");
    if (lua_istable(state, -1) != 0) {
        config.drop_paths = lua_internal::string_list_from_stack(state, lua_gettop(state), path);
    } else if (lua_isnil(state, -1) == 0) {
        lua_internal::throw_lua_error(path, "Field 'drop_paths' must be list");
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "question_conflicts");
    if (lua_isnil(state, -1) == 0) {
        config.question_conflicts = parse_conflict_strategy(
            lua_internal::value_to_string(state, -1, path),
            path,
            "question_conflicts");
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "pre_hook_conflict");
    if (lua_isnil(state, -1) == 0) {
        config.pre_hook_conflict = parse_conflict_strategy(
            lua_internal::value_to_string(state, -1, path),
            path,
            "pre_hook_conflict");
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "post_hook_conflict");
    if (lua_isnil(state, -1) == 0) {
        config.post_hook_conflict = parse_conflict_strategy(
            lua_internal::value_to_string(state, -1, path),
            path,
            "post_hook_conflict");
    }
    lua_pop(state, 1);

    return config;
}

std::vector<LayoutRule> load_layout_rules_from_file(const std::filesystem::path& path) {
    auto state = lua_internal::make_state();
    lua_internal::register_metadata_helpers(state.get());
    lua_internal::load_file_result(state.get(), path);

    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(path, "layout.lua must return a list");
    }

    std::vector<LayoutRule> rules;
    const std::size_t count = lua_rawlen(state.get(), -1);
    rules.reserve(count);

    for (std::size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state.get(), -1, static_cast<lua_Integer>(index));
        if (lua_istable(state.get(), -1) == 0) {
            lua_internal::throw_lua_error(path, "Layout entry must be table");
        }

        LayoutRule rule;
        rule.source = lua_internal::required_string_field(state.get(), lua_gettop(state.get()), "source", path);
        rule.source_path = path;

        if (const auto target = lua_internal::optional_string_field(state.get(), lua_gettop(state.get()), "target", path)) {
            rule.target = *target;
        }
        rule.exclude = lua_internal::optional_bool_field(state.get(), lua_gettop(state.get()), "exclude", false, path);

        lua_getfield(state.get(), -1, "render");
        if (lua_isboolean(state.get(), -1) != 0) {
            rule.render = lua_toboolean(state.get(), -1) != 0;
        }
        lua_pop(state.get(), 1);

        rules.push_back(rule);
        lua_pop(state.get(), 1);
    }

    return rules;
}

std::vector<ScriptCatalogEntry> load_script_catalog(const std::filesystem::path& template_root) {
    const std::filesystem::path scripts_root = template_root / "scripts";
    std::vector<ScriptCatalogEntry> scripts;
    if (!std::filesystem::is_directory(scripts_root)) {
        return scripts;
    }

    for (const auto& entry : std::filesystem::directory_iterator(scripts_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".lua") {
            continue;
        }
        scripts.push_back({
            .name = entry.path().stem().string(),
            .path = entry.path(),
        });
    }

    std::ranges::sort(scripts, {}, &ScriptCatalogEntry::name);
    return scripts;
}

QuestionDefinition load_question_entry(lua_State* state,
                                       const int table_index,
                                       const std::filesystem::path& path,
                                       const std::string& group_name,
                                       const std::size_t source_index) {
    if (lua_istable(state, table_index) == 0) {
        lua_internal::throw_lua_error(path, "Question entry must be table");
    }

    QuestionDefinition question;
    question.key = lua_internal::required_string_field(state, table_index, "key", path);
    question.group = group_name;
    question.source_path = path;
    question.source_index = source_index;

    if (const auto type = lua_internal::optional_string_field(state, table_index, "type", path)) {
        question.type = *type;
    }
    if (const auto prompt = lua_internal::optional_string_field(state, table_index, "prompt", path)) {
        question.prompt = *prompt;
    }
    if (const auto help = lua_internal::optional_string_field(state, table_index, "help", path)) {
        question.help = *help;
    } else if (const auto description = lua_internal::optional_string_field(state, table_index, "description", path)) {
        question.help = *description;
    }
    if (lua_internal::optional_string_field(state, table_index, "group", path).has_value()) {
        lua_internal::throw_lua_error(path, "Question entry must not define 'group'; use top-level groups instead");
    }

    question.optional = lua_internal::optional_bool_field(state, table_index, "optional", false, path);
    question.sensitive = lua_internal::optional_bool_field(state, table_index, "sensitive", false, path);

    lua_getfield(state, table_index, "choices");
    if (lua_istable(state, -1) != 0) {
        question.choices = lua_internal::string_list_from_stack(state, lua_gettop(state), path);
    } else if (lua_isnil(state, -1) == 0) {
        lua_internal::throw_lua_error(path, "Field 'choices' must be list");
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "default");
    if (lua_isfunction(state, -1) != 0) {
        question.default_is_function = true;
    } else if (lua_isnil(state, -1) == 0) {
        question.default_value = lua_internal::value_to_string(state, -1, path);
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "condition");
    if (lua_isfunction(state, -1) != 0) {
        question.condition_is_function = true;
    } else if (lua_isnil(state, -1) == 0) {
        question.condition_value = lua_internal::value_to_string(state, -1, path);
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "validate");
    if (lua_isfunction(state, -1) != 0) {
        question.validate_is_function = true;
    } else if (lua_isnil(state, -1) == 0) {
        lua_internal::throw_lua_error(path, "Field 'validate' must be function when present");
    }
    lua_pop(state, 1);

    lua_getfield(state, table_index, "alias");
    if (lua_isstring(state, -1) != 0) {
        question.aliases.emplace_back(lua_tostring(state, -1));
    } else if (lua_istable(state, -1) != 0) {
        question.aliases = lua_internal::string_list_from_stack(state, lua_gettop(state), path);
    }
    lua_pop(state, 1);

    return question;
}

void load_questions_from_file(const std::filesystem::path& path,
                              std::vector<std::string>& group_order,
                              std::vector<QuestionDefinition>& questions) {
    auto state = lua_internal::make_state();
    lua_internal::register_metadata_helpers(state.get());
    lua_internal::load_file_result(state.get(), path);

    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(path, "questions.lua must return a table with 'order' and 'groups'");
    }

    lua_getfield(state.get(), -1, "order");
    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(path, "Field 'order' must be list");
    }
    group_order = lua_internal::string_list_from_stack(state.get(), lua_gettop(state.get()), path);
    lua_pop(state.get(), 1);

    if (group_order.empty()) {
        lua_internal::throw_lua_error(path, "Field 'order' must not be empty");
    }

    lua_getfield(state.get(), -1, "groups");
    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(path, "Field 'groups' must be table");
    }

    const int groups_index = lua_absindex(state.get(), -1);
    std::vector<std::string> loaded_groups;

    lua_pushnil(state.get());
    while (lua_next(state.get(), groups_index) != 0) {
        if (lua_isstring(state.get(), -2) == 0) {
            lua_internal::throw_lua_error(path, "Group names in 'groups' must be strings");
        }

        const std::string group_name = lua_tostring(state.get(), -2);
        loaded_groups.push_back(group_name);

        if (lua_istable(state.get(), -1) == 0) {
            lua_internal::throw_lua_error(path, "Each group in 'groups' must be question list");
        }

        const std::size_t count = lua_rawlen(state.get(), -1);
        if (count == 0) {
            lua_internal::throw_lua_error(path, "Group must not be empty: " + group_name);
        }

        for (std::size_t index = 1; index <= count; ++index) {
            lua_rawgeti(state.get(), -1, static_cast<lua_Integer>(index));
            questions.push_back(load_question_entry(state.get(), lua_gettop(state.get()), path, group_name, index));
            lua_pop(state.get(), 1);
        }

        lua_pop(state.get(), 1);
    }

    for (const auto& group_name : group_order) {
        if (std::ranges::find(loaded_groups, group_name) == loaded_groups.end()) {
            lua_internal::throw_lua_error(path, "Group declared in 'order' missing from 'groups': " + group_name);
        }
    }

    for (const auto& group_name : loaded_groups) {
        if (std::ranges::find(group_order, group_name) == group_order.end()) {
            lua_internal::throw_lua_error(path, "Group found in 'groups' missing from 'order': " + group_name);
        }
    }

    lua_pop(state.get(), 1);
}

std::vector<std::string> load_include_ids_from_file(const std::filesystem::path& path) {
    auto state = lua_internal::make_state();
    lua_internal::register_metadata_helpers(state.get());
    lua_internal::load_file_result(state.get(), path);
    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(path, "includes.lua must return a list");
    }
    return lua_internal::string_list_from_stack(state.get(), lua_gettop(state.get()), path);
}

PrebyteConfig load_prebyte_config_from_file(const std::filesystem::path& path,
                                            const std::filesystem::path& template_root) {
    auto state = lua_internal::make_state();
    lua_internal::register_metadata_helpers(state.get());
    lua_internal::load_file_result(state.get(), path);
    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(path, "prebyte.lua must return a table");
    }

    PrebyteConfig config;

    lua_getfield(state.get(), -1, "include_paths");
    if (lua_istable(state.get(), -1) != 0) {
        const auto values = lua_internal::string_list_from_stack(state.get(), lua_gettop(state.get()), path);
        for (const auto& value : values) {
            config.include_paths.push_back(template_root / value);
        }
    }
    lua_pop(state.get(), 1);

    lua_getfield(state.get(), -1, "rules");
    if (lua_istable(state.get(), -1) != 0) {
        config.rules = lua_internal::string_map_from_stack(state.get(), lua_gettop(state.get()), path);
    }
    lua_pop(state.get(), 1);

    return config;
}

}

TemplateInfo LuaEngine::load_template_info(const std::filesystem::path& template_root) const {
    const TemplateManifest manifest = load_partial_manifest(template_root);
    return manifest.info;
}

TemplateManifest LuaEngine::load_partial_manifest(const std::filesystem::path& template_root) const {
    const std::filesystem::path manifest_path = template_root / "template.lua";
    if (!std::filesystem::is_regular_file(manifest_path)) {
        throw TempifyError("Missing template manifest: " + manifest_path.string());
    }

    auto state = lua_internal::make_state();
    lua_internal::register_metadata_helpers(state.get());
    lua_internal::load_file_result(state.get(), manifest_path);

    if (lua_istable(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(manifest_path, "template.lua must return a table");
    }

    TemplateManifest manifest;
    manifest.root = template_root;
    manifest.info.root = template_root;
    manifest.info.id = lua_internal::required_string_field(state.get(), lua_gettop(state.get()), "id", manifest_path);
    manifest.info.name = lua_internal::optional_string_field(state.get(), lua_gettop(state.get()), "name", manifest_path).value_or(manifest.info.id);
    manifest.info.description = lua_internal::optional_string_field(state.get(), lua_gettop(state.get()), "description", manifest_path).value_or("");
    manifest.version = lua_internal::optional_string_field(state.get(), lua_gettop(state.get()), "version", manifest_path).value_or("0.1.0");
    manifest.info.version = manifest.version;
    manifest.source_dir = lua_internal::optional_string_field(state.get(), lua_gettop(state.get()), "source_dir", manifest_path).value_or("files");
    manifest.output_path_template = manifest.info.id;

    lua_getfield(state.get(), -1, "output");
    if (lua_istable(state.get(), -1) != 0) {
        manifest.output_path_template = lua_internal::optional_string_field(state.get(), lua_gettop(state.get()), "path", manifest_path)
                                            .value_or(manifest.output_path_template);
        manifest.overwrite = lua_internal::optional_bool_field(state.get(), lua_gettop(state.get()), "overwrite", false, manifest_path);
    }
    lua_pop(state.get(), 1);

    lua_getfield(state.get(), -1, "merge");
    if (lua_istable(state.get(), -1) != 0) {
        manifest.merge_config = load_merge_config_from_stack(state.get(), lua_gettop(state.get()), manifest_path);
    } else if (lua_isnil(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(manifest_path, "Field 'merge' must be table");
    }
    lua_pop(state.get(), 1);

    manifest.include_ids = lua_internal::optional_string_list_field(state.get(), lua_gettop(state.get()), "includes", manifest_path);

    const std::filesystem::path source_root = template_root / manifest.source_dir;
    if (!std::filesystem::is_directory(source_root)) {
        throw TempifyError("Template source directory not found: " + source_root.string());
    }
    manifest.source_roots.push_back({
        .path = source_root,
        .template_id = manifest.info.id,
    });

    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
        const std::filesystem::path relative = std::filesystem::relative(entry.path(), source_root);
        if (relative.empty()) {
            continue;
        }

        if (entry.is_directory()) {
            manifest.directories.push_back({
                .relative_path = relative.generic_string(),
            });
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().extension() == ".pbc") {
            continue;
        }

        manifest.files.push_back({
            .relative_path = relative.generic_string(),
            .source_path = entry.path(),
            .render_with_prebyte = entry.path().extension() == ".pbt",
            .source_template_id = manifest.info.id,
        });
    }

    const std::filesystem::path questions_path = template_root / "questions.lua";
    if (std::filesystem::is_regular_file(questions_path)) {
        load_questions_from_file(questions_path, manifest.question_group_order, manifest.questions);
    }

    for (const auto& question : manifest.questions) {
        if (question.group.empty()) {
            lua_internal::throw_lua_error(manifest_path, "Question missing group after load: " + question.key);
        }
    }

    const std::filesystem::path includes_path = template_root / "includes.lua";
    if (std::filesystem::is_regular_file(includes_path)) {
        const auto include_ids = load_include_ids_from_file(includes_path);
        manifest.include_ids.insert(manifest.include_ids.end(), include_ids.begin(), include_ids.end());
    }

    const std::filesystem::path prebyte_path = template_root / "prebyte.lua";
    if (std::filesystem::is_regular_file(prebyte_path)) {
        manifest.prebyte_config = load_prebyte_config_from_file(prebyte_path, template_root);
    }

    const std::filesystem::path layout_path = template_root / "layout.lua";
    if (std::filesystem::is_regular_file(layout_path)) {
        manifest.layout_rules = load_layout_rules_from_file(layout_path);
        for (auto& rule : manifest.layout_rules) {
            rule.source_template_id = manifest.info.id;
        }
    }

    manifest.scripts = load_script_catalog(template_root);
    manifest.env_defaults = load_env_file(template_root / ".env");

    const std::filesystem::path pre_hook = template_root / "hooks" / "pre.lua";
    if (std::filesystem::is_regular_file(pre_hook)) {
        manifest.pre_hook_path = pre_hook;
    }

    const std::filesystem::path post_hook = template_root / "hooks" / "post.lua";
    if (std::filesystem::is_regular_file(post_hook)) {
        manifest.post_hook_path = post_hook;
    }

    const std::filesystem::path before_render_hook = template_root / "hooks" / "before_render.lua";
    if (std::filesystem::is_regular_file(before_render_hook)) {
        manifest.before_render_hook_path = before_render_hook;
    }

    const std::filesystem::path after_render_hook = template_root / "hooks" / "after_render.lua";
    if (std::filesystem::is_regular_file(after_render_hook)) {
        manifest.after_render_hook_path = after_render_hook;
    }

    return manifest;
}

}
