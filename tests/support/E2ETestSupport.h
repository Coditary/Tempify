#pragma once

#include "TempifyTestSupport.h"

#include <algorithm>
#include <map>
#include <string>

namespace tempify::test_support::e2e {

inline std::string slug_to_namespace(const std::string &slug) {
    std::string ns = slug;
    std::replace(ns.begin(), ns.end(), '-', '_');
    return ns + "_ns";
}

inline std::map<std::string, std::string> isolated_cli_env(const std::filesystem::path &data_home,
                                                             const std::filesystem::path &config_home = {}) {
    std::map<std::string, std::string> env;
    env["XDG_DATA_HOME"] = data_home.string();
    if (!config_home.empty()) {
        env["XDG_CONFIG_HOME"] = config_home.string();
    }
    return env;
}

inline std::filesystem::path prepare_template_workspace(const std::filesystem::path &workspace_root) {
    std::filesystem::create_directories(workspace_root);
    link_test_templates_into_workspace(workspace_root);
    return workspace_root;
}

} // namespace tempify::test_support::e2e
