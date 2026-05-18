#include "tempify/question/AnswerFile.h"

#include "tempify/support/Errors.h"

#include "datatypes/Data.h"
#include "parser/JsonParser.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace tempify {

namespace {

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string scalar_to_string(const prebyte::Data& value,
                             const std::filesystem::path& path,
                             const std::string& key,
                             const bool strict) {
    static_cast<void>(strict);
    if (value.is_string()) {
        return value.as_string_ref();
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    if (value.is_int()) {
        return std::to_string(value.as_int());
    }
    if (value.is_double()) {
        return value.as_string();
    }

    throw TempifyError("Answer file value for key '" + key + "' must be scalar in " + path.string());
}

}

std::map<std::string, std::string> load_answer_file(const std::filesystem::path& path,
                                                    const bool strict) {
    if (!std::filesystem::is_regular_file(path)) {
        throw TempifyError("Answer file not found: " + path.string());
    }

    prebyte::JsonParser parser;
    prebyte::Data data;
    try {
        data = parser.parse(path);
    } catch (const std::exception& error) {
        throw TempifyError("Could not parse answer file '" + path.string() + "': " + error.what());
    }

    if (!data.is_map()) {
        throw TempifyError("Answer file must be JSON object: " + path.string());
    }

    std::map<std::string, std::string> values;
    for (const auto& [key, value] : data.as_map()) {
        values[key] = scalar_to_string(value, path, key, strict);
    }
    return values;
}

void write_answer_file(const std::filesystem::path& path,
                       const std::map<std::string, std::string>& values) {
    std::ostringstream stream;
    stream << "{\n";
    std::size_t index = 0;
    for (const auto& [key, value] : values) {
        stream << "  \"" << json_escape(key) << "\": \"" << json_escape(value) << "\"";
        if (index + 1 < values.size()) {
            stream << ',';
        }
        stream << '\n';
        ++index;
    }
    stream << "}\n";

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw TempifyError("Could not write answer file: " + path.string());
    }
    output << stream.str();
}

}
