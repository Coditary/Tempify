#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace tempify {

class HookTrustStore {
public:
    explicit HookTrustStore(std::filesystem::path path);

    const std::filesystem::path& path() const noexcept;
    bool is_trusted(const std::filesystem::path& template_root) const;
    void trust(const std::filesystem::path& template_root) const;

private:
    std::filesystem::path path_;
};

std::filesystem::path default_hook_trust_store_path(const std::filesystem::path& data_root);

}
