#include "tempify/config/TempifyConfig.h"

#include "tempify/support/Errors.h"

#include "datatypes/Data.h"
#include "parser/JsonParser.h"

#include <filesystem>
#include <sstream>

namespace tempify {

namespace {

std::string scalar_to_string(const prebyte::Data& value,
                             const std::filesystem::path& path,
                             const std::string& key) {
    if (value.is_string()) {
        return value.as_string_ref();
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    if (value.is_int()) {
        return std::to_string(value.as_int());
    }
    if (value.is_double()) {
        return value.as_string();
    }

    throw TempifyError("Config value for key '" + key + "' must be scalar in " + path.string());
}

HookAcceptance parse_hook_acceptance(const std::string& value,
                                     const std::filesystem::path& path) {
    if (value == "yes") {
        return HookAcceptance::Yes;
    }
    if (value == "ask") {
        return HookAcceptance::Ask;
    }
    if (value == "no") {
        return HookAcceptance::No;
    }

    throw TempifyError("Invalid render.accept_hooks value in config '" + path.string() + "': " + value);
}

ExistingPathBehavior parse_existing_path_behavior(const std::string& value,
                                                  const std::filesystem::path& path) {
    if (value == "error") {
        return ExistingPathBehavior::Error;
    }
    if (value == "overwrite") {
        return ExistingPathBehavior::Overwrite;
    }
    if (value == "skip") {
        return ExistingPathBehavior::Skip;
    }

    throw TempifyError("Invalid render.existing_path_behavior value in config '" + path.string() + "': " + value);
}

void load_defaults(const prebyte::Data& data,
                   const std::filesystem::path& path,
                   TempifyConfig& config) {
    if (!data.is_map()) {
        throw TempifyError("Field 'defaults' in config must be JSON object: " + path.string());
    }

    for (const auto& [key, value] : data.as_map()) {
        config.defaults[key] = scalar_to_string(value, path, key);
    }
}

void load_render_config(const prebyte::Data& data,
                        const std::filesystem::path& path,
                        TempifyConfig& config) {
    if (!data.is_map()) {
        throw TempifyError("Field 'render' in config must be JSON object: " + path.string());
    }

    for (const auto& [key, value] : data.as_map()) {
        if (key == "accept_hooks") {
            if (!value.is_string()) {
                throw TempifyError("Field 'render.accept_hooks' in config must be string: " + path.string());
            }
            config.hook_acceptance = parse_hook_acceptance(value.as_string_ref(), path);
            continue;
        }

        if (key == "hook_timeout_ms") {
            if (!value.is_int()) {
                throw TempifyError("Field 'render.hook_timeout_ms' in config must be integer: " + path.string());
            }
            const int timeout = value.as_int();
            if (timeout < 0) {
                throw TempifyError("Field 'render.hook_timeout_ms' in config must be zero or greater: " + path.string());
            }
            config.hook_timeout_ms = timeout;
            continue;
        }

        if (key == "existing_path_behavior") {
            if (!value.is_string()) {
                throw TempifyError("Field 'render.existing_path_behavior' in config must be string: " + path.string());
            }
            config.existing_path_behavior = parse_existing_path_behavior(value.as_string_ref(), path);
            continue;
        }

        throw TempifyError("Unknown render config key '" + key + "' in " + path.string());
    }
}

}

TempifyConfig load_tempify_config_file(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw TempifyError("Config file not found: " + path.string());
    }

    prebyte::JsonParser parser;
    prebyte::Data data;
    try {
        data = parser.parse(path);
    } catch (const std::exception& error) {
        throw TempifyError("Could not parse config file '" + path.string() + "': " + error.what());
    }

    if (!data.is_map()) {
        throw TempifyError("Config file must be JSON object: " + path.string());
    }

    TempifyConfig config;
    for (const auto& [key, value] : data.as_map()) {
        if (key == "defaults") {
            load_defaults(value, path, config);
            continue;
        }

        if (key == "render") {
            load_render_config(value, path, config);
            continue;
        }

        throw TempifyError("Unknown config key '" + key + "' in " + path.string());
    }

    return config;
}

TempifyConfig merge_tempify_config(const TempifyConfig& base, const TempifyConfig& overlay) {
    TempifyConfig merged = base;
    for (const auto& [key, value] : overlay.defaults) {
        merged.defaults[key] = value;
    }
    if (overlay.hook_acceptance.has_value()) {
        merged.hook_acceptance = overlay.hook_acceptance;
    }
    if (overlay.hook_timeout_ms.has_value()) {
        merged.hook_timeout_ms = overlay.hook_timeout_ms;
    }
    if (overlay.existing_path_behavior.has_value()) {
        merged.existing_path_behavior = overlay.existing_path_behavior;
    }
    return merged;
}

}
