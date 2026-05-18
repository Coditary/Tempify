#include "tempify/template/TemplateValidator.h"

#include "tempify/support/Errors.h"

#include <set>

namespace tempify {

namespace {

std::string describe_question_source(const QuestionDefinition& question) {
    return question.source_path.empty()
        ? question.key
        : question.source_path.string() + "#" + std::to_string(question.source_index);
}

}

void TemplateValidator::validate(const TemplateManifest& manifest) const {
    std::set<std::string> question_keys;
    std::set<std::string> aliases;

    for (const auto& question : manifest.questions) {
        if (aliases.contains(question.key)) {
            throw TempifyError("Question key '" + question.key + "' conflicts with existing alias");
        }
        if (!question_keys.insert(question.key).second) {
            throw TempifyError("Duplicate question key '" + question.key + "' in " + describe_question_source(question));
        }

        for (const auto& alias : question.aliases) {
            if (alias == question.key) {
                throw TempifyError("Question alias must differ from key for '" + question.key + "'");
            }
            if (question_keys.contains(alias)) {
                throw TempifyError("Question alias '" + alias + "' conflicts with question key");
            }
            if (!aliases.insert(alias).second) {
                throw TempifyError("Duplicate question alias found: " + alias);
            }
        }
    }

    std::set<std::string> known_layout_sources;
    for (const auto& directory : manifest.directories) {
        known_layout_sources.insert(directory.relative_path);
    }
    for (const auto& file : manifest.files) {
        known_layout_sources.insert(file.relative_path);
    }

    for (const auto& rule : manifest.layout_rules) {
        if (!known_layout_sources.contains(rule.source)) {
            throw TempifyError("Layout rule references missing source path: " + rule.source);
        }
        if (rule.target.has_value() && rule.target->empty()) {
            throw TempifyError("Layout rule target must not be empty for source: " + rule.source);
        }
    }

    std::set<std::string> script_names;
    for (const auto& script : manifest.scripts) {
        if (!script_names.insert(script.name).second) {
            throw TempifyError("Duplicate script name found: " + script.name);
        }
        if (!std::filesystem::is_regular_file(script.path)) {
            throw TempifyError("Script missing on disk: " + script.path.string());
        }
    }

    const auto validate_hook = [](const std::optional<std::filesystem::path>& path, const std::string& name) {
        if (path.has_value() && !std::filesystem::is_regular_file(*path)) {
            throw TempifyError("Missing hook file for " + name + ": " + path->string());
        }
    };
    validate_hook(manifest.pre_hook_path, "pre");
    validate_hook(manifest.before_render_hook_path, "before_render");
    validate_hook(manifest.after_render_hook_path, "after_render");
    validate_hook(manifest.post_hook_path, "post");
}

}
