#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

class TemplateLoader;

struct StoredTemplateRecord {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::filesystem::path path;
};

class LocalTemplateStore {
  public:
    explicit LocalTemplateStore(const std::filesystem::path &shared_root);

    const std::filesystem::path &root() const noexcept;
    const std::filesystem::path &templates_root() const noexcept;
    const std::filesystem::path &index_root() const noexcept;
    const std::filesystem::path &index_file() const noexcept;

    std::vector<StoredTemplateRecord> list_templates() const;
    std::optional<StoredTemplateRecord> find_template(const std::string &id) const;
    std::size_t refresh(const TemplateLoader &loader) const;

  private:
    std::filesystem::path root_;
    std::filesystem::path templates_root_;
    std::filesystem::path index_root_;
    std::filesystem::path index_file_;
};

} // namespace tempify
