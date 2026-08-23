#pragma once

#include "tempify/domain/RenderOptions.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace tempify {

struct TempifyConfig {
    std::map<std::string, std::string> defaults;
    std::optional<HookAcceptance> hook_acceptance;
    std::optional<int> hook_timeout_ms;
    std::optional<ExistingPathBehavior> existing_path_behavior;
};

TempifyConfig load_tempify_config_file(const std::filesystem::path &path);
TempifyConfig merge_tempify_config(const TempifyConfig &base, const TempifyConfig &overlay);

} // namespace tempify
