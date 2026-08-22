#include "tempify/store/AvailableTemplateCache.h"

#include "datatypes/Data.h"
#include "parser/JsonParser.h"
#include "tempify/support/Errors.h"

#include <algorithm>

namespace tempify {

namespace {

std::string string_field(const prebyte::Data::Map &object, const std::string &key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->second.is_string()) {
        return {};
    }
    return it->second.as_string();
}

std::vector<std::string> string_array_field(const prebyte::Data::Map &object, const std::string &key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->second.is_array()) {
        return {};
    }

    std::vector<std::string> values;
    for (const auto &item : it->second.as_array()) {
        if (item.is_string()) {
            values.push_back(item.as_string());
        }
    }
    return values;
}

std::vector<AvailableTemplateRecord> load_cache(const std::filesystem::path &path) {
    std::vector<AvailableTemplateRecord> entries;
    if (!std::filesystem::is_regular_file(path)) {
        return entries;
    }

    prebyte::JsonParser parser;
    const prebyte::Data data = parser.parse(path);
    if (!data.is_map()) {
        throw TempifyError("Available template cache must be JSON object: " + path.string());
    }

    const auto &root = data.as_map();
    const auto templates_it = root.find("templates");
    if (templates_it == root.end() || !templates_it->second.is_array()) {
        return entries;
    }

    for (const auto &item : templates_it->second.as_array()) {
        if (!item.is_map()) {
            continue;
        }

        const auto &object = item.as_map();
        const std::string id = string_field(object, "id");
        const std::string version = string_field(object, "version");
        if (id.empty() || version.empty()) {
            continue;
        }

        AvailableTemplateRecord entry{
            .id = id,
            .name = string_field(object, "name"),
            .description = string_field(object, "description"),
            .version = version,
            .tags = string_array_field(object, "tags"),
        };

        const auto source_it = object.find("source");
        if (source_it != object.end() && source_it->second.is_map()) {
            const auto &source = source_it->second.as_map();
            entry.source_url = string_field(source, "url");
            entry.source_ref = string_field(source, "ref");
            entry.source_subdir = string_field(source, "subdir");
        }

        const auto repository_it = object.find("repository");
        if (repository_it != object.end() && repository_it->second.is_map()) {
            entry.repository_id = string_field(repository_it->second.as_map(), "id");
        }

        entries.push_back(std::move(entry));
    }

    std::ranges::sort(entries, {}, &AvailableTemplateRecord::id);
    return entries;
}

} // namespace

AvailableTemplateCache::AvailableTemplateCache(std::filesystem::path shared_root)
    : root_(std::filesystem::absolute(std::move(shared_root))), file_(root_ / "index" / "reqpack-available.json") {}

const std::filesystem::path &AvailableTemplateCache::file() const noexcept {
    return file_;
}

std::vector<AvailableTemplateRecord> AvailableTemplateCache::list_templates() const {
    return load_cache(file_);
}

std::optional<AvailableTemplateRecord> AvailableTemplateCache::find_template(const std::string &id) const {
    for (const auto &entry : list_templates()) {
        if (entry.id == id) {
            return entry;
        }
    }
    return std::nullopt;
}

} // namespace tempify
