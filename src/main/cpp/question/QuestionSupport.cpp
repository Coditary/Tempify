#include "QuestionProcessorInternal.h"

#include "tempify/frontend/IQuestionFrontend.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <string_view>
#include <utility>

namespace tempify::question_internal {

namespace {

constexpr std::string_view kRedactedValue = "<redacted>";

std::string validation_feedback_for(const QuestionDefinition& question) {
    if (question.sensitive) {
        return std::string("Invalid value for '") + question.key + "'";
    }
    return {};
}

}

std::string trim(std::string value) {
    const auto is_space = [](const unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_question_key(const std::vector<QuestionDefinition>& questions, const std::string& key) {
    return std::ranges::any_of(questions, [&](const QuestionDefinition& question) {
        return question.key == key;
    });
}

std::map<std::string, std::string> normalize_assignments(const std::map<std::string, std::string>& raw_values,
                                                         const std::vector<QuestionDefinition>& questions,
                                                         const std::string& source_name) {
    std::map<std::string, std::string> alias_to_key;
    for (const auto& question : questions) {
        for (const auto& alias : question.aliases) {
            alias_to_key[alias] = question.key;
        }
    }

    std::map<std::string, std::string> normalized;
    for (const auto& [raw_key, value] : raw_values) {
        const std::string key = alias_to_key.contains(raw_key) ? alias_to_key.at(raw_key) : raw_key;
        const auto existing = normalized.find(key);
        if (existing != normalized.end() && existing->second != value) {
            throw TempifyError("Conflicting values for key '" + key + "' from " + source_name);
        }
        normalized[key] = value;
    }

    return normalized;
}

void propagate_aliases(std::map<std::string, std::string>& values,
                       const std::vector<QuestionDefinition>& questions) {
    for (const auto& question : questions) {
        const auto it = values.find(question.key);
        if (it == values.end()) {
            continue;
        }
        for (const auto& alias : question.aliases) {
            const auto existing = values.find(alias);
            if (existing != values.end() && existing->second != it->second) {
                throw TempifyError("Alias conflict for '" + alias + "'");
            }
            values[alias] = it->second;
        }
    }
}

std::optional<std::string> default_value_for(const QuestionDefinition& question,
                                             const std::map<std::string, std::string>& env_values,
                                             const std::map<std::string, std::string>& config_values,
                                             const std::map<std::string, std::string>& current_values,
                                             const LuaEngine& lua_engine) {
    const auto config_value = config_values.find(question.key);
    if (config_value != config_values.end()) {
        return config_value->second;
    }

    const auto env_value = env_values.find(question.key);
    if (env_value != env_values.end()) {
        return env_value->second;
    }

    if (question.default_is_function) {
        return lua_engine.evaluate_default(question, current_values);
    }

    return question.default_value;
}

std::string display_default_value(const QuestionDefinition& question, const std::string& value) {
    if (question.sensitive) {
        return std::string(kRedactedValue);
    }
    if (question.type == "bool" || question.type == "boolean") {
        const std::string lowered = to_lower_copy(trim(value));
        return (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on") ? "yes" : "no";
    }
    return value;
}

std::string parse_bool_value(const std::string& input) {
    const std::string lowered = to_lower_copy(trim(input));
    if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "y" || lowered == "on") {
        return "true";
    }
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "n" || lowered == "off") {
        return "false";
    }

    throw TempifyError("Expected boolean value (yes/no, true/false, 1/0)");
}

std::string parse_int_value(const std::string& input) {
    const std::string trimmed = trim(input);
    if (trimmed.empty()) {
        throw TempifyError("Expected integer value");
    }

    int value = 0;
    const auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    if (ec != std::errc{} || ptr != trimmed.data() + trimmed.size()) {
        throw TempifyError("Expected integer value");
    }

    return std::to_string(value);
}

std::string parse_choice_value(const QuestionDefinition& question, const std::string& input) {
    const std::string trimmed = trim(input);
    if (trimmed.empty()) {
        throw TempifyError("Expected choice value");
    }

    const auto exact = std::ranges::find(question.choices, trimmed);
    if (exact != question.choices.end()) {
        return *exact;
    }

    int index = 0;
    const auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), index);
    if (ec == std::errc{} &&
        ptr == trimmed.data() + trimmed.size() &&
        index >= 1 &&
        static_cast<std::size_t>(index) <= question.choices.size()) {
        return question.choices[static_cast<std::size_t>(index - 1)];
    }

    throw TempifyError("Expected one of configured choices");
}

std::string coerce_value(const QuestionDefinition& question, const std::string& raw_value) {
    const std::string type = to_lower_copy(question.type);
    if (type == "string") {
        return raw_value;
    }
    if (type == "bool" || type == "boolean") {
        return parse_bool_value(raw_value);
    }
    if (type == "int" || type == "integer") {
        return parse_int_value(raw_value);
    }
    if (type == "choice") {
        if (question.choices.empty()) {
            throw TempifyError("Question '" + question.key + "' uses type 'choice' without choices");
        }
        return parse_choice_value(question, raw_value);
    }

    throw TempifyError("Unsupported question type: " + question.type);
}

std::string prompt_text_for(const QuestionDefinition& question, const std::optional<std::string>& default_value) {
    std::ostringstream stream;
    stream << (question.prompt.empty() ? question.key : question.prompt);

    if (question.type == "choice" && !question.choices.empty()) {
        stream << " [";
        for (std::size_t index = 0; index < question.choices.size(); ++index) {
            if (index > 0) {
                stream << "/";
            }
            stream << question.choices[index];
        }
        stream << "]";
    }

    if (default_value.has_value()) {
        stream << " (default: " << display_default_value(question, *default_value) << ")";
    }

    if (question.optional) {
        stream << " (optional, '-' = skip)";
    }

    stream << ": ";
    return stream.str();
}

std::optional<std::string> ask_question(const QuestionDefinition& question,
                                        const std::optional<std::string>& default_value,
                                        std::map<std::string, std::string>& values,
                                        const std::vector<QuestionDefinition>& questions,
                                        const LuaEngine& lua_engine,
                                        IQuestionFrontend& frontend) {
    while (true) {
        const auto input = frontend.prompt(prompt_text_for(question, default_value), question.sensitive);
        if (!input.has_value()) {
            throw TempifyError("Input aborted while answering question '" + question.key + "'");
        }

        if (input->action == FrontendAction::Back) {
            return std::string(":__TEMPIFY_BACK__:");
        }
        if (input->action == FrontendAction::Quit) {
            throw TempifyError("Input aborted by user");
        }

        const std::string trimmed_input = trim(input->value);
        if (trimmed_input == "?" && !question.help.empty()) {
            frontend.write_line(question.help);
            continue;
        }

        if (trimmed_input == "-" && question.optional) {
            return std::nullopt;
        }

        std::string candidate;
        if (trimmed_input.empty()) {
            if (default_value.has_value()) {
                candidate = *default_value;
            } else if (question.optional) {
                return std::nullopt;
            } else {
                frontend.write_line("Value required.");
                continue;
            }
        } else {
            candidate = trimmed_input;
        }

        try {
            candidate = coerce_value(question, candidate);
        } catch (const TempifyError& error) {
            frontend.write_line(error.what());
            continue;
        }

        std::map<std::string, std::string> preview = values;
        preview[question.key] = candidate;
        propagate_aliases(preview, questions);

        const auto validation_error = lua_engine.validate_answer(question, candidate, preview);
        if (validation_error.has_value()) {
            if (question.sensitive) {
                frontend.write_line(validation_feedback_for(question));
            } else {
                frontend.write_line(*validation_error);
            }
            continue;
        }

        return candidate;
    }
}

}
