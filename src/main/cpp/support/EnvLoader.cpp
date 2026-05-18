#include "tempify/support/EnvLoader.h"

#include <fstream>
#include <sstream>

namespace tempify {

namespace {

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

std::string strip_quotes(std::string value) {
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

}

std::map<std::string, std::string> load_env_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }

    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, separator));
        std::string value = strip_quotes(trim(trimmed.substr(separator + 1)));
        if (!key.empty()) {
            values[key] = value;
        }
    }

    return values;
}

}
