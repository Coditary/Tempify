#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

struct QuestionDefinition {
    std::string key;
    std::string type = "string";
    std::string prompt;
    std::optional<std::string> default_value;
    bool default_is_function = false;
    bool condition_is_function = false;
    std::optional<std::string> condition_value;
    bool validate_is_function = false;
    std::vector<std::string> choices;
    bool optional = false;
    std::string help;
    std::string group;
    std::vector<std::string> aliases;
    std::filesystem::path source_path;
    std::size_t source_index = 0;
    bool sensitive = false;
};

} // namespace tempify
