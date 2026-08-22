#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

struct AvailableTemplateRecord {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::vector<std::string> tags;
    std::string source_url;
    std::string source_ref;
    std::string source_subdir;
    std::string repository_id;
};

class AvailableTemplateCache {
  public:
    explicit AvailableTemplateCache(std::filesystem::path shared_root);

    const std::filesystem::path &file() const noexcept;

    std::vector<AvailableTemplateRecord> list_templates() const;
    std::optional<AvailableTemplateRecord> find_template(const std::string &id) const;

  private:
    std::filesystem::path root_;
    std::filesystem::path file_;
};

} // namespace tempify
