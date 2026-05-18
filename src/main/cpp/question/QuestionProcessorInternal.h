#pragma once

#include "tempify/domain/TemplateManifest.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

class IQuestionFrontend;
class LuaEngine;

namespace question_internal {

std::string trim(std::string value);
std::string to_lower_copy(std::string value);
bool is_question_key(const std::vector<QuestionDefinition>& questions, const std::string& key);

std::map<std::string, std::string> normalize_assignments(const std::map<std::string, std::string>& raw_values,
                                                         const std::vector<QuestionDefinition>& questions,
                                                         const std::string& source_name);
void propagate_aliases(std::map<std::string, std::string>& values,
                       const std::vector<QuestionDefinition>& questions);

std::optional<std::string> default_value_for(const QuestionDefinition& question,
                                             const std::map<std::string, std::string>& env_values,
                                             const std::map<std::string, std::string>& config_values,
                                             const std::map<std::string, std::string>& current_values,
                                             const LuaEngine& lua_engine);
std::string coerce_value(const QuestionDefinition& question, const std::string& raw_value);

std::optional<std::string> ask_question(const QuestionDefinition& question,
                                        const std::optional<std::string>& default_value,
                                        std::map<std::string, std::string>& values,
                                        const std::vector<QuestionDefinition>& questions,
                                        const LuaEngine& lua_engine,
                                        IQuestionFrontend& frontend);

std::vector<std::string> build_group_order(const TemplateManifest& manifest);
void clear_group_values(std::map<std::string, std::string>& values,
                        const TemplateManifest& manifest,
                        const std::map<std::string, std::string>& explicit_cli,
                        const std::map<std::string, std::string>& explicit_imported,
                        const std::string& group);
bool group_has_interactive_questions(const TemplateManifest& manifest,
                                     const std::string& group,
                                     const std::map<std::string, std::string>& explicit_cli,
                                     const std::map<std::string, std::string>& explicit_imported,
                                     const std::map<std::string, std::string>& current_values,
                                     const LuaEngine& lua_engine);

}

}
