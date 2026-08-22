#pragma once

#include <filesystem>
#include <optional>

namespace tempify {

std::optional<std::filesystem::path> find_workspace_templates_root(const std::filesystem::path &start);
std::optional<std::filesystem::path> find_workspace_config_file(const std::filesystem::path &start);
std::filesystem::path resolve_tempify_data_root();
std::filesystem::path resolve_tempify_config_root();
std::filesystem::path default_global_config_file_path();

} // namespace tempify
