#include "tempify/lua/LuaEngine.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace tempify {

namespace {

std::vector<std::string> effective_group_order(const TemplateManifest& manifest) {
    if (!manifest.question_group_order.empty()) {
        return manifest.question_group_order;
    }

    std::vector<std::string> groups;
    for (const auto& question : manifest.questions) {
        if (std::ranges::find(groups, question.group) == groups.end()) {
            groups.push_back(question.group);
        }
    }
    return groups;
}

std::string escape_json(const std::string& value) {
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

void append_json_string_field(std::vector<std::string>& fields,
                              const std::string& indent,
                              const std::string& name,
                              const std::string& value,
                              const bool full) {
    if (!full && value.empty()) {
        return;
    }

    fields.push_back(indent + "\"" + name + "\": \"" + escape_json(value) + "\"");
}

void append_json_bool_field(std::vector<std::string>& fields,
                            const std::string& indent,
                            const std::string& name,
                            const bool value,
                            const bool full) {
    if (!full && !value) {
        return;
    }

    fields.push_back(indent + "\"" + name + "\": " + (value ? "true" : "false"));
}

void append_json_array_field(std::vector<std::string>& fields,
                             const std::string& indent,
                             const std::string& name,
                             const std::vector<std::string>& values,
                             const bool full) {
    if (!full && values.empty()) {
        return;
    }

    std::ostringstream stream;
    stream << indent << '"' << name << "\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << '"' << escape_json(values[index]) << '"';
    }
    stream << ']';
    fields.push_back(stream.str());
}

void write_json_fields(std::ostringstream& stream,
                       const std::vector<std::string>& fields) {
    for (std::size_t index = 0; index < fields.size(); ++index) {
        stream << fields[index];
        if (index + 1 < fields.size()) {
            stream << ',';
        }
        stream << '\n';
    }
}

}

std::string LuaEngine::export_questions_json(const TemplateManifest& manifest,
                                             const bool full) const {
    std::ostringstream stream;
    std::vector<std::string> template_fields;
    template_fields.push_back("    \"id\": \"" + escape_json(manifest.info.id) + "\"");
    append_json_string_field(template_fields, "    ", "name", manifest.info.name, full);
    const std::vector<std::string> group_order = effective_group_order(manifest);

    stream << "{\n";
    stream << "  \"template\": {\n";
    write_json_fields(stream, template_fields);
    stream << "  },\n";
    stream << "  \"order\": [";
    for (std::size_t group_index = 0; group_index < group_order.size(); ++group_index) {
        if (group_index > 0) {
            stream << ", ";
        }
        stream << '"' << escape_json(group_order[group_index]) << '"';
    }
    stream << "],\n";
    stream << "  \"questions\": {\n";

    for (std::size_t group_index = 0; group_index < group_order.size(); ++group_index) {
        const auto& group_name = group_order[group_index];
        stream << "    \"" << escape_json(group_name) << "\": [\n";

        bool wrote_question = false;
        for (const auto& question : manifest.questions) {
            if (question.group != group_name) {
                continue;
            }

            std::vector<std::string> fields;
            fields.push_back("        \"key\": \"" + escape_json(question.key) + "\"");
            fields.push_back("        \"type\": \"" + escape_json(question.type) + "\"");
            append_json_string_field(fields, "        ", "prompt", question.prompt, full);
            append_json_bool_field(fields, "        ", "optional", question.optional, full);
            append_json_bool_field(fields, "        ", "sensitive", question.sensitive, full);
            append_json_string_field(fields, "        ", "help", question.help, full);
            append_json_bool_field(fields,
                                   "        ",
                                   "has_condition",
                                   question.condition_is_function || question.condition_value.has_value(),
                                   full);
            append_json_bool_field(fields,
                                   "        ",
                                   "has_validate",
                                   question.validate_is_function,
                                   full);
            append_json_array_field(fields, "        ", "aliases", question.aliases, full);
            append_json_array_field(fields, "        ", "choices", question.choices, full);

            if (wrote_question) {
                stream << ",\n";
            }
            stream << "      {\n";
            write_json_fields(stream, fields);
            stream << "      }";
            wrote_question = true;
        }

        if (wrote_question) {
            stream << '\n';
        }
        stream << "    ]";
        if (group_index + 1 < group_order.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

}
