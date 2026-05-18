#include "tempify/hook/HookTrustStore.h"

#include "tempify/support/Errors.h"

#include "datatypes/Data.h"
#include "parser/JsonParser.h"

#include <fstream>
#include <set>
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

std::set<std::string> load_trusted_roots(const std::filesystem::path& path) {
    std::set<std::string> roots;
    if (!std::filesystem::is_regular_file(path)) {
        return roots;
    }

    prebyte::JsonParser parser;
    prebyte::Data data;
    try {
        data = parser.parse(path);
    } catch (const std::exception& error) {
        throw TempifyError("Could not parse hook trust store '" + path.string() + "': " + error.what());
    }

    if (!data.is_map()) {
        throw TempifyError("Hook trust store must be JSON object: " + path.string());
    }

    const auto& root = data.as_map();
    const auto it = root.find("trusted_templates");
    if (it == root.end() || !it->second.is_array()) {
        return roots;
    }

    for (const auto& entry : it->second.as_array()) {
        if (!entry.is_string()) {
            continue;
        }
        roots.insert(entry.as_string());
    }
    return roots;
}

void save_trusted_roots(const std::filesystem::path& path,
                        const std::set<std::string>& roots) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"trusted_templates\": [\n";
    std::size_t index = 0;
    for (const auto& root : roots) {
        stream << "    \"" << json_escape(root) << "\"";
        if (index + 1 < roots.size()) {
            stream << ',';
        }
        stream << '\n';
        ++index;
    }
    stream << "  ]\n";
    stream << "}\n";

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw TempifyError("Could not write hook trust store: " + path.string());
    }
    output << stream.str();
}

std::string canonical_root_string(const std::filesystem::path& template_root) {
    return std::filesystem::weakly_canonical(template_root).string();
}

}

HookTrustStore::HookTrustStore(std::filesystem::path path)
    : path_(std::move(path)) {}

const std::filesystem::path& HookTrustStore::path() const noexcept {
    return path_;
}

bool HookTrustStore::is_trusted(const std::filesystem::path& template_root) const {
    const auto roots = load_trusted_roots(path_);
    return roots.contains(canonical_root_string(template_root));
}

void HookTrustStore::trust(const std::filesystem::path& template_root) const {
    auto roots = load_trusted_roots(path_);
    roots.insert(canonical_root_string(template_root));
    save_trusted_roots(path_, roots);
}

std::filesystem::path default_hook_trust_store_path(const std::filesystem::path& data_root) {
    return data_root / "trust" / "hooks.json";
}

}
