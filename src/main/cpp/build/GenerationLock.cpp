#include "tempify/build/GenerationLock.h"

#include "tempify/support/Errors.h"

#include "datatypes/Data.h"
#include "parser/JsonParser.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace tempify {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw TempifyError("Could not read file: " + path.string());
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

const prebyte::Data* find_field(const prebyte::Data::Map& map, const std::string& key) {
    const auto it = map.find(key);
    if (it == map.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string qualified_field_name(const std::string& field_name, const std::string& prefix) {
    if (prefix.empty()) {
        return field_name;
    }
    return prefix + "." + field_name;
}

std::string optional_string_field(const prebyte::Data::Map& map,
                                  const std::string& field_name,
                                  const std::filesystem::path& path,
                                  const std::string& prefix = {}) {
    const prebyte::Data* value = find_field(map, field_name);
    if (value == nullptr) {
        return {};
    }
    if (!value->is_string()) {
        throw TempifyError("Lock file field '" + qualified_field_name(field_name, prefix)
                           + "' must be string: " + path.string());
    }
    return value->as_string_ref();
}

bool optional_bool_field(const prebyte::Data::Map& map,
                         const std::string& field_name,
                         const std::filesystem::path& path,
                         const bool default_value,
                         const std::string& prefix = {}) {
    const prebyte::Data* value = find_field(map, field_name);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_bool()) {
        throw TempifyError("Lock file field '" + qualified_field_name(field_name, prefix)
                           + "' must be boolean: " + path.string());
    }
    return value->as_bool();
}

std::set<std::string> optional_string_array_field(const prebyte::Data::Map& map,
                                                  const std::string& field_name,
                                                  const std::filesystem::path& path,
                                                  const std::string& prefix = {}) {
    const prebyte::Data* value = find_field(map, field_name);
    if (value == nullptr) {
        return {};
    }
    if (!value->is_array()) {
        throw TempifyError("Lock file field '" + qualified_field_name(field_name, prefix)
                           + "' must be array: " + path.string());
    }

    std::set<std::string> strings;
    for (const auto& entry : value->as_array()) {
        if (!entry.is_string()) {
            throw TempifyError("Lock file field '" + qualified_field_name(field_name, prefix)
                               + "' must contain only strings: " + path.string());
        }
        strings.insert(entry.as_string_ref());
    }
    return strings;
}

std::map<std::string, std::string> optional_string_map_field(const prebyte::Data::Map& map,
                                                             const std::string& field_name,
                                                             const std::filesystem::path& path,
                                                             const std::string& prefix = {}) {
    const prebyte::Data* value = find_field(map, field_name);
    if (value == nullptr) {
        return {};
    }
    if (!value->is_map()) {
        throw TempifyError("Lock file field '" + qualified_field_name(field_name, prefix)
                           + "' must be object: " + path.string());
    }

    std::map<std::string, std::string> strings;
    for (const auto& [key, entry] : value->as_map()) {
        if (!entry.is_string()) {
            throw TempifyError("Lock file field '" + qualified_field_name(field_name, prefix)
                               + "' must contain only string values: " + path.string());
        }
        strings.emplace(key, entry.as_string_ref());
    }
    return strings;
}

}

std::string content_fingerprint_hex(const std::string_view value) {
    std::uint64_t hash = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    for (const unsigned char ch : value) {
        hash ^= ch;
        hash *= prime;
    }

    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

std::map<std::string, std::string> build_generation_lock_managed_file_hashes(const BuildPlan& plan) {
    std::map<std::string, std::string> hashes;
    for (const auto& file : plan.files) {
        if (!std::filesystem::is_regular_file(file.output_path)) {
            continue;
        }
        const std::string relative_path = std::filesystem::relative(file.output_path, plan.build_root).generic_string();
        hashes.emplace(relative_path, content_fingerprint_hex(read_file(file.output_path)));
    }
    return hashes;
}

std::optional<GenerationLockRecord> load_generation_lock(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    prebyte::JsonParser parser;
    prebyte::Data data;
    try {
        data = parser.parse(path);
    } catch (const std::exception& error) {
        throw TempifyError("Could not parse lock file '" + path.string() + "': " + error.what());
    }

    if (!data.is_map()) {
        throw TempifyError("Lock file must be JSON object: " + path.string());
    }

    const auto& root = data.as_map();
    GenerationLockRecord record;
    record.tempify_version = optional_string_field(root, "tempify_version", path);
    record.build_root = optional_string_field(root, "build_root", path);
    record.generated_at = optional_string_field(root, "generated_at", path);
    record.existing_path_behavior = optional_string_field(root, "existing_path_behavior", path);
    record.hook_acceptance = optional_string_field(root, "hook_acceptance", path);
    record.hooks_disabled = optional_bool_field(root, "hooks_disabled", path, false);
    record.managed_files = optional_string_array_field(root, "managed_files", path);
    record.managed_file_hashes = optional_string_map_field(root, "managed_file_hashes", path);
    record.values = optional_string_map_field(root, "values", path);

    if (const prebyte::Data* template_value = find_field(root, "template"); template_value != nullptr) {
        if (!template_value->is_map()) {
            throw TempifyError("Lock file field 'template' must be object: " + path.string());
        }
        const auto& template_map = template_value->as_map();
        record.template_info.id = optional_string_field(template_map, "id", path, "template");
        record.template_info.name = optional_string_field(template_map, "name", path, "template");
        record.template_info.version = optional_string_field(template_map, "version", path, "template");
        record.template_info.root = optional_string_field(template_map, "root", path, "template");
    }

    return record;
}

}
