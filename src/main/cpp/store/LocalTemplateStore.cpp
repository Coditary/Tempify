#include "tempify/store/LocalTemplateStore.h"

#include "tempify/support/Errors.h"
#include "tempify/template/TemplateLoader.h"

#include "datatypes/Data.h"
#include "parser/JsonParser.h"

#include <algorithm>
#include <fstream>
#include <map>
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

void write_text_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw TempifyError("Could not write file: " + path.string());
    }
    output << content;
}

std::vector<StoredTemplateRecord> load_index(const std::filesystem::path& path) {
    std::vector<StoredTemplateRecord> entries;
    if (!std::filesystem::is_regular_file(path)) {
        return entries;
    }

    prebyte::JsonParser parser;
    const prebyte::Data data = parser.parse(path);
    if (!data.is_map()) {
        throw TempifyError("Template store index must be JSON object: " + path.string());
    }

    const auto& root = data.as_map();
    const auto it = root.find("templates");
    if (it == root.end() || !it->second.is_array()) {
        return entries;
    }

    for (const auto& item : it->second.as_array()) {
        if (!item.is_map()) {
            continue;
        }
        const auto& object = item.as_map();
        entries.push_back({
            .id = object.at("id").as_string(),
            .name = object.at("name").as_string(),
            .description = object.at("description").as_string(),
            .version = object.at("version").as_string(),
            .path = object.at("path").as_string(),
        });
    }

    std::ranges::sort(entries, {}, &StoredTemplateRecord::id);
    return entries;
}

void save_index(const std::filesystem::path& path,
                const std::vector<StoredTemplateRecord>& entries) {
    std::ostringstream stream;
    stream << "{\n  \"templates\": [\n";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        stream << "    {\"id\": \"" << json_escape(entry.id)
               << "\", \"name\": \"" << json_escape(entry.name)
               << "\", \"description\": \"" << json_escape(entry.description)
               << "\", \"version\": \"" << json_escape(entry.version)
               << "\", \"path\": \"" << json_escape(entry.path.string()) << "\"}";
        if (index + 1 < entries.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n}\n";

    const std::filesystem::path temp_path = path.string() + ".tmp";
    write_text_file(temp_path, stream.str());

    std::error_code error;
    std::filesystem::rename(temp_path, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temp_path, path, error);
        if (error) {
            throw TempifyError("Could not replace template store index: " + path.string());
        }
    }
}

}

LocalTemplateStore::LocalTemplateStore(std::filesystem::path shared_root)
    : root_(std::filesystem::absolute(std::move(shared_root))),
      templates_root_(root_ / "templates"),
      index_root_(root_ / "index"),
      index_file_(index_root_ / "templates.json") {}

const std::filesystem::path& LocalTemplateStore::root() const noexcept {
    return root_;
}

const std::filesystem::path& LocalTemplateStore::templates_root() const noexcept {
    return templates_root_;
}

const std::filesystem::path& LocalTemplateStore::index_root() const noexcept {
    return index_root_;
}

const std::filesystem::path& LocalTemplateStore::index_file() const noexcept {
    return index_file_;
}

std::vector<StoredTemplateRecord> LocalTemplateStore::list_templates() const {
    return load_index(index_file_);
}

std::optional<StoredTemplateRecord> LocalTemplateStore::find_template(const std::string& id) const {
    for (const auto& entry : list_templates()) {
        if (entry.id == id) {
            return entry;
        }
    }
    return std::nullopt;
}

std::size_t LocalTemplateStore::refresh(const TemplateLoader& loader) const {
    std::filesystem::create_directories(templates_root_);
    std::filesystem::create_directories(index_root_);

    std::map<std::string, StoredTemplateRecord> entries;
    std::vector<std::filesystem::path> template_roots;
    for (const auto& entry : std::filesystem::directory_iterator(templates_root_)) {
        if (entry.is_directory()) {
            template_roots.push_back(entry.path());
        }
    }
    std::ranges::sort(template_roots);

    for (const auto& template_root : template_roots) {
        TemplateInfo info;
        try {
            info = loader.summarize(template_root);
        } catch (const TempifyError&) {
            continue;
        }

        const StoredTemplateRecord record{
            .id = info.id,
            .name = info.name,
            .description = info.description,
            .version = info.version,
            .path = std::filesystem::absolute(template_root),
        };

        const auto [it, inserted] = entries.emplace(record.id, record);
        if (!inserted) {
            throw TempifyError("Duplicate shared template id found: " + record.id);
        }
    }

    std::vector<StoredTemplateRecord> ordered;
    ordered.reserve(entries.size());
    for (const auto& [id, entry] : entries) {
        static_cast<void>(id);
        ordered.push_back(entry);
    }

    save_index(index_file_, ordered);
    return ordered.size();
}

}
