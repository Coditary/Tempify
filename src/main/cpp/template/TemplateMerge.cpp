#include "tempify/template/TemplateLoader.h"

#include "tempify/support/Errors.h"

#include <algorithm>

namespace tempify {

namespace {

ConflictStrategy conflict_for_path(const TemplateMergeConfig& policy, const std::string& path) {
    const auto exact = policy.file_conflicts.find(path);
    if (exact != policy.file_conflicts.end()) {
        return exact->second;
    }

    for (const auto& [key, strategy] : policy.file_conflicts) {
        if (key.size() > 2 && key.starts_with("*.") && path.ends_with(key.substr(1))) {
            return strategy;
        }
    }

    return ConflictStrategy::Replace;
}

bool path_matches_drop_rule(const std::string& candidate, const std::string& drop_path) {
    if (candidate == drop_path) {
        return true;
    }

    return candidate.size() > drop_path.size()
        && candidate.starts_with(drop_path)
        && candidate[drop_path.size()] == '/';
}

void merge_scripts(TemplateManifest& result, const TemplateManifest& overlay) {
    for (const auto& script : overlay.scripts) {
        const auto it = std::find_if(result.scripts.begin(), result.scripts.end(), [&](const ScriptCatalogEntry& existing) {
            return existing.name == script.name;
        });
        if (it == result.scripts.end()) {
            result.scripts.push_back(script);
        } else {
            *it = script;
        }
    }
}

void merge_questions(TemplateManifest& result,
                     const TemplateManifest& overlay,
                     const TemplateMergeConfig& policy) {
    const ConflictStrategy question_conflict = policy.question_conflicts.value_or(ConflictStrategy::Replace);
    for (const auto& group_name : overlay.question_group_order) {
        if (std::ranges::find(result.question_group_order, group_name) == result.question_group_order.end()) {
            result.question_group_order.push_back(group_name);
        }
    }

    for (const auto& question : overlay.questions) {
        const auto it = std::find_if(result.questions.begin(), result.questions.end(), [&](const QuestionDefinition& existing) {
            return existing.key == question.key;
        });
        if (it == result.questions.end()) {
            result.questions.push_back(question);
            continue;
        }

        switch (question_conflict) {
        case ConflictStrategy::Replace:
            *it = question;
            break;
        case ConflictStrategy::Keep:
            break;
        case ConflictStrategy::Error:
            throw TempifyError("Question conflict for key '" + question.key + "' between templates '"
                + it->source_path.parent_path().parent_path().filename().string() + "' and '" + overlay.info.id + "'");
        }
    }
}

void apply_drop_paths(TemplateManifest& result, const TemplateMergeConfig& policy) {
    if (policy.drop_paths.empty()) {
        return;
    }

    std::erase_if(result.directories, [&](const TemplateDirectoryEntry& existing) {
        return std::ranges::any_of(policy.drop_paths, [&](const std::string& drop_path) {
            return path_matches_drop_rule(existing.relative_path, drop_path);
        });
    });

    std::erase_if(result.files, [&](const TemplateFileEntry& existing) {
        return std::ranges::any_of(policy.drop_paths, [&](const std::string& drop_path) {
            return path_matches_drop_rule(existing.relative_path, drop_path);
        });
    });
}

void merge_directories(TemplateManifest& result, const TemplateManifest& overlay) {
    for (const auto& directory : overlay.directories) {
        const auto it = std::find_if(result.directories.begin(), result.directories.end(), [&](const TemplateDirectoryEntry& existing) {
            return existing.relative_path == directory.relative_path;
        });
        if (it == result.directories.end()) {
            result.directories.push_back(directory);
        }
    }
}

void merge_files(TemplateManifest& result,
                 const TemplateManifest& overlay,
                 const TemplateMergeConfig& policy) {
    for (const auto& file : overlay.files) {
        const auto it = std::find_if(result.files.begin(), result.files.end(), [&](const TemplateFileEntry& existing) {
            return existing.relative_path == file.relative_path;
        });
        if (it == result.files.end()) {
            result.files.push_back(file);
            continue;
        }

        const ConflictStrategy strategy = conflict_for_path(policy, file.relative_path);
        switch (strategy) {
        case ConflictStrategy::Replace:
            *it = file;
            break;
        case ConflictStrategy::Keep:
            break;
        case ConflictStrategy::Error:
            throw TempifyError("File conflict for '" + file.relative_path + "' between templates '"
                + it->source_template_id + "' and '" + file.source_template_id + "'");
        }
    }
}

void merge_hook(std::optional<std::filesystem::path>& destination,
                const std::optional<std::filesystem::path>& overlay,
                const std::optional<ConflictStrategy>& policy,
                const bool overlay_is_local,
                const std::string& error_prefix,
                const std::string& overlay_id) {
    if (!overlay.has_value()) {
        return;
    }

    switch (policy.value_or(ConflictStrategy::Replace)) {
    case ConflictStrategy::Replace:
        destination = overlay;
        break;
    case ConflictStrategy::Keep:
        if (!destination.has_value()) {
            destination = overlay;
        }
        break;
    case ConflictStrategy::Error:
        if (destination.has_value() && !overlay_is_local) {
            throw TempifyError(error_prefix + " while merging template '" + overlay_id + "'");
        }
        destination = overlay;
        break;
    }
}

}

TemplateManifest TemplateLoader::merge(const TemplateManifest& base,
                                       const TemplateManifest& overlay,
                                       const TemplateMergeConfig& policy,
                                       const bool overlay_is_local) const {
    if (base.info.id.empty()) {
        return overlay;
    }

    TemplateManifest result = base;
    result.root = overlay.root;
    result.info = overlay.info;
    result.version = !overlay.version.empty() ? overlay.version : base.version;
    result.source_dir = !overlay.source_dir.empty() ? overlay.source_dir : base.source_dir;
    result.output_path_template = !overlay.output_path_template.empty() ? overlay.output_path_template : base.output_path_template;
    result.overwrite = overlay.overwrite;

    result.source_roots.insert(result.source_roots.end(), overlay.source_roots.begin(), overlay.source_roots.end());

    for (const auto& include_id : overlay.include_ids) {
        if (std::find(result.include_ids.begin(), result.include_ids.end(), include_id) == result.include_ids.end()) {
            result.include_ids.push_back(include_id);
        }
    }

    for (const auto& [key, value] : overlay.env_defaults) {
        result.env_defaults[key] = value;
    }

    result.prebyte_config.include_paths.insert(result.prebyte_config.include_paths.end(),
                                               overlay.prebyte_config.include_paths.begin(),
                                               overlay.prebyte_config.include_paths.end());
    for (const auto& [key, value] : overlay.prebyte_config.rules) {
        result.prebyte_config.rules[key] = value;
    }

    for (const auto& rule : overlay.layout_rules) {
        result.layout_rules.push_back(rule);
    }

    merge_scripts(result, overlay);
    merge_questions(result, overlay, policy);
    apply_drop_paths(result, policy);
    merge_directories(result, overlay);
    merge_files(result, overlay, policy);

    merge_hook(result.pre_hook_path,
               overlay.pre_hook_path,
               policy.pre_hook_conflict,
               overlay_is_local,
               "Pre-hook conflict",
               overlay.info.id);
    merge_hook(result.post_hook_path,
               overlay.post_hook_path,
               policy.post_hook_conflict,
               overlay_is_local,
               "Post-hook conflict",
               overlay.info.id);

    if (overlay.before_render_hook_path.has_value()) {
        result.before_render_hook_path = overlay.before_render_hook_path;
    }
    if (overlay.after_render_hook_path.has_value()) {
        result.after_render_hook_path = overlay.after_render_hook_path;
    }

    return result;
}

}
