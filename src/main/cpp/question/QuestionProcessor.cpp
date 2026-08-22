#include "tempify/question/QuestionProcessor.h"

#include "QuestionProcessorInternal.h"
#include "tempify/frontend/IQuestionFrontend.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace tempify {

namespace {

constexpr std::string_view kRedactedValue = "<redacted>";

std::string review_value_for(const QuestionDefinition &question, const std::string &value) {
    if (question.sensitive) {
        return std::string(kRedactedValue);
    }
    return value;
}

std::string normalize_confirmation_value(std::string value) {
    std::ranges::transform(value, value.begin(),
                           [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    value.erase(
        std::remove_if(value.begin(), value.end(), [](const unsigned char ch) { return std::isspace(ch) != 0; }),
        value.end());
    return value;
}

std::vector<std::string> build_review_lines(const TemplateManifest &manifest,
                                            const std::map<std::string, std::string> &values,
                                            const LuaEngine &lua_engine) {
    std::vector<std::string> lines;
    std::string last_group;
    for (const auto &question : manifest.questions) {
        if (!lua_engine.evaluate_condition(question, values)) {
            continue;
        }
        const auto it = values.find(question.key);
        if (it == values.end()) {
            continue;
        }

        const std::string group = question.group.empty() ? "General" : question.group;
        if (group != last_group) {
            if (!lines.empty()) {
                lines.emplace_back();
            }
            lines.push_back("[" + group + "]");
            last_group = group;
        }

        const std::string label = question.prompt.empty() ? question.key : question.prompt;
        lines.push_back(label + ": " + review_value_for(question, it->second));
    }
    return lines;
}

bool review_answers(const TemplateManifest &manifest, const std::map<std::string, std::string> &values,
                    const LuaEngine &lua_engine, IQuestionFrontend &frontend) {
    frontend.write_line("");
    frontend.write_line("Review values before generate:");
    for (const auto &line : build_review_lines(manifest, values, lua_engine)) {
        frontend.write_line(line);
    }

    while (true) {
        const auto input = frontend.prompt("Generate with these values? [Y/n]: ");
        if (!input.has_value()) {
            throw TempifyError("Input aborted during review");
        }
        if (input->action == FrontendAction::Back) {
            return false;
        }
        if (input->action == FrontendAction::Quit) {
            throw TempifyError("Input aborted by user");
        }

        const std::string value = normalize_confirmation_value(input->value);
        if (value.empty() || value == "y" || value == "yes") {
            return true;
        }
        if (value == "n" || value == "no") {
            return false;
        }

        frontend.write_line("Enter yes or no.");
    }
}

void apply_resolved_value(const QuestionDefinition &question, const std::string &candidate,
                          std::map<std::string, std::string> &values, const std::vector<QuestionDefinition> &questions,
                          const LuaEngine &lua_engine, const std::string &source_label) {
    const std::string coerced = question_internal::coerce_value(question, candidate);
    std::map<std::string, std::string> preview = values;
    preview[question.key] = coerced;
    question_internal::propagate_aliases(preview, questions);
    if (const auto validation_error = lua_engine.validate_answer(question, coerced, preview)) {
        if (question.sensitive) {
            throw TempifyError(source_label + " for '" + question.key + "' invalid.");
        }
        throw TempifyError(source_label + " for '" + question.key + "' invalid: " + *validation_error);
    }
    values[question.key] = coerced;
    question_internal::propagate_aliases(values, questions);
}

} // namespace

QuestionProcessor::QuestionProcessor(const LuaEngine &lua_engine, IQuestionFrontend &frontend)
    : lua_engine_(lua_engine), frontend_(frontend) {}

std::map<std::string, std::string> QuestionProcessor::collect(const TemplateManifest &manifest,
                                                              const std::map<std::string, std::string> &cli_values,
                                                              const std::map<std::string, std::string> &config_values,
                                                              const std::map<std::string, std::string> &imported_values,
                                                              const bool non_interactive, const bool strict,
                                                              const bool review_before_finish) const {
    const auto explicit_cli = question_internal::normalize_assignments(cli_values, manifest.questions, "CLI");
    const auto explicit_config = question_internal::normalize_assignments(config_values, manifest.questions, "config");
    const auto explicit_imported =
        question_internal::normalize_assignments(imported_values, manifest.questions, "answer file");
    const auto env_values = question_internal::normalize_assignments(manifest.env_defaults, manifest.questions, ".env");

    std::map<std::string, std::string> values;
    for (const auto &[key, value] : env_values) {
        if (!question_internal::is_question_key(manifest.questions, key)) {
            values[key] = value;
        }
    }
    for (const auto &[key, value] : explicit_config) {
        if (!question_internal::is_question_key(manifest.questions, key)) {
            values[key] = value;
        }
    }
    for (const auto &[key, value] : explicit_cli) {
        if (!question_internal::is_question_key(manifest.questions, key)) {
            values[key] = value;
        }
    }

    if (strict) {
        for (const auto &[key, value] : explicit_imported) {
            static_cast<void>(value);
            if (!question_internal::is_question_key(manifest.questions, key) &&
                manifest.env_defaults.find(key) == manifest.env_defaults.end()) {
                throw TempifyError("Unknown key '" + key + "' in answer file");
            }
        }
    }

    for (const auto &question : manifest.questions) {
        if (!lua_engine_.evaluate_condition(question, values)) {
            continue;
        }

        if (const auto explicit_value = explicit_cli.find(question.key); explicit_value != explicit_cli.end()) {
            apply_resolved_value(question, explicit_value->second, values, manifest.questions, lua_engine_,
                                 "CLI value");
            continue;
        }

        if (const auto imported_value = explicit_imported.find(question.key);
            imported_value != explicit_imported.end()) {
            apply_resolved_value(question, imported_value->second, values, manifest.questions, lua_engine_,
                                 "Answer file value");
        }
    }

    const std::vector<std::string> groups = question_internal::build_group_order(manifest);
    std::size_t group_index = 0;
    std::optional<std::size_t> last_interactive_group_index;
    while (true) {
        while (group_index < groups.size()) {
            const std::string &current_group = groups[group_index];
            if (non_interactive) {
                for (const auto &question : manifest.questions) {
                    const std::string question_group = question.group.empty() ? "General" : question.group;
                    if (question_group != current_group) {
                        continue;
                    }
                    if (!lua_engine_.evaluate_condition(question, values)) {
                        continue;
                    }
                    if (const auto explicit_value = explicit_cli.find(question.key);
                        explicit_value != explicit_cli.end()) {
                        if (!values.contains(question.key)) {
                            apply_resolved_value(question, explicit_value->second, values, manifest.questions,
                                                 lua_engine_, "CLI value");
                        }
                        continue;
                    }
                    if (const auto imported_value = explicit_imported.find(question.key);
                        imported_value != explicit_imported.end()) {
                        if (!values.contains(question.key)) {
                            apply_resolved_value(question, imported_value->second, values, manifest.questions,
                                                 lua_engine_, "Answer file value");
                        }
                        continue;
                    }

                    const auto default_value = question_internal::default_value_for(
                        question, env_values, explicit_config, values, lua_engine_);
                    if (default_value.has_value()) {
                        apply_resolved_value(question, *default_value, values, manifest.questions, lua_engine_,
                                             "Default value");
                        continue;
                    }

                    if (question.optional) {
                        values.erase(question.key);
                        for (const auto &alias : question.aliases) {
                            values.erase(alias);
                        }
                        continue;
                    }

                    throw TempifyError("Missing required answer for '" + question.key +
                                       "' while `--non-interactive` is enabled.");
                }
                ++group_index;
                continue;
            }

            if (!question_internal::group_has_interactive_questions(manifest, current_group, explicit_cli,
                                                                    explicit_imported, values, lua_engine_)) {
                ++group_index;
                continue;
            }

            frontend_.begin_group(current_group, group_index + 1, groups.size());
            bool go_back = false;
            bool restart_current = false;

            for (const auto &question : manifest.questions) {
                const std::string question_group = question.group.empty() ? "General" : question.group;
                if (question_group != current_group) {
                    continue;
                }
                if (!lua_engine_.evaluate_condition(question, values)) {
                    continue;
                }
                if (explicit_cli.contains(question.key)) {
                    if (!values.contains(question.key)) {
                        apply_resolved_value(question, explicit_cli.at(question.key), values, manifest.questions,
                                             lua_engine_, "CLI value");
                    }
                    continue;
                }
                if (explicit_imported.contains(question.key)) {
                    if (!values.contains(question.key)) {
                        apply_resolved_value(question, explicit_imported.at(question.key), values, manifest.questions,
                                             lua_engine_, "Answer file value");
                    }
                    continue;
                }

                const auto default_value =
                    question_internal::default_value_for(question, env_values, explicit_config, values, lua_engine_);
                const auto answer = question_internal::ask_question(question, default_value, values, manifest.questions,
                                                                    lua_engine_, frontend_);

                if (!answer.has_value()) {
                    values.erase(question.key);
                    for (const auto &alias : question.aliases) {
                        values.erase(alias);
                    }
                    continue;
                }

                if (*answer == ":__TEMPIFY_BACK__:") {
                    if (group_index == 0) {
                        frontend_.write_line("Already at first page.");
                        restart_current = true;
                    } else {
                        go_back = true;
                    }
                    break;
                }

                values[question.key] = *answer;
                question_internal::propagate_aliases(values, manifest.questions);
            }

            frontend_.end_group();

            if (restart_current) {
                question_internal::clear_group_values(values, manifest, explicit_cli, explicit_imported, current_group);
                continue;
            }

            if (go_back) {
                question_internal::clear_group_values(values, manifest, explicit_cli, explicit_imported, current_group);
                const std::size_t previous_group_index = group_index - 1;
                const std::string &previous_group = groups[previous_group_index];
                question_internal::clear_group_values(values, manifest, explicit_cli, explicit_imported,
                                                      previous_group);
                group_index = previous_group_index;
                continue;
            }

            last_interactive_group_index = group_index;
            ++group_index;
        }

        if (!review_before_finish || !last_interactive_group_index.has_value()) {
            break;
        }

        if (review_answers(manifest, values, lua_engine_, frontend_)) {
            break;
        }

        const std::size_t review_group_index = *last_interactive_group_index;
        question_internal::clear_group_values(values, manifest, explicit_cli, explicit_imported,
                                              groups[review_group_index]);
        group_index = review_group_index;
        last_interactive_group_index.reset();
    }

    if (strict) {
        for (const auto &[key, value] : explicit_imported) {
            static_cast<void>(value);
            if (!question_internal::is_question_key(manifest.questions, key) && values.find(key) == values.end()) {
                throw TempifyError("Unused key '" + key + "' in answer file");
            }
        }
    }

    return values;
}

} // namespace tempify
