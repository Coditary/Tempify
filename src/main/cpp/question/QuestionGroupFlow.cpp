#include "QuestionProcessorInternal.h"
#include "tempify/lua/LuaEngine.h"

#include <algorithm>

namespace tempify::question_internal {

std::vector<std::string> build_group_order(const TemplateManifest &manifest) {
    if (!manifest.question_group_order.empty()) {
        return manifest.question_group_order;
    }

    std::vector<std::string> groups;
    for (const auto &question : manifest.questions) {
        const std::string group = question.group.empty() ? "General" : question.group;
        if (std::ranges::find(groups, group) == groups.end()) {
            groups.push_back(group);
        }
    }
    return groups;
}

void clear_group_values(std::map<std::string, std::string> &values, const TemplateManifest &manifest,
                        const std::map<std::string, std::string> &explicit_cli,
                        const std::map<std::string, std::string> &explicit_imported, const std::string &group) {
    for (const auto &question : manifest.questions) {
        const std::string question_group = question.group.empty() ? "General" : question.group;
        if (question_group != group) {
            continue;
        }
        if (explicit_cli.contains(question.key)) {
            continue;
        }
        if (explicit_imported.contains(question.key)) {
            continue;
        }
        values.erase(question.key);
        for (const auto &alias : question.aliases) {
            values.erase(alias);
        }
    }
}

bool group_has_interactive_questions(const TemplateManifest &manifest, const std::string &group,
                                     const std::map<std::string, std::string> &explicit_cli,
                                     const std::map<std::string, std::string> &explicit_imported,
                                     const std::map<std::string, std::string> &current_values,
                                     const LuaEngine &lua_engine) {
    for (const auto &question : manifest.questions) {
        const std::string question_group = question.group.empty() ? "General" : question.group;
        if (question_group != group) {
            continue;
        }
        if (explicit_cli.contains(question.key)) {
            continue;
        }
        if (explicit_imported.contains(question.key)) {
            continue;
        }
        if (!lua_engine.evaluate_condition(question, current_values)) {
            continue;
        }
        return true;
    }
    return false;
}

} // namespace tempify::question_internal
