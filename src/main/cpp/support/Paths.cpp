#include "tempify/support/Paths.h"

#include "tempify/support/Errors.h"

#include <cstdlib>
#include <filesystem>
#include <optional>

namespace tempify {

namespace {

std::optional<std::filesystem::path> environment_path(const char* name) {
    if (const char* value = std::getenv(name)) {
        if (*value != '\0') {
            return std::filesystem::path(value);
        }
    }
    return std::nullopt;
}

}

std::optional<std::filesystem::path> find_workspace_templates_root(const std::filesystem::path& start) {
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true) {
        if (std::filesystem::is_directory(current / "templates")) {
            return current / "templates";
        }

        if (current == current.root_path()) {
            return std::nullopt;
        }

        current = current.parent_path();
    }
}

std::optional<std::filesystem::path> find_workspace_config_file(const std::filesystem::path& start) {
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true) {
        const std::filesystem::path candidate = current / ".tempify" / "config.json";
        if (std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }

        if (current == current.root_path()) {
            return std::nullopt;
        }

        current = current.parent_path();
    }
}

std::filesystem::path resolve_tempify_data_root() {
    if (const auto xdg_data_home = environment_path("XDG_DATA_HOME")) {
        return *xdg_data_home / "tempify";
    }

    if (const auto home = environment_path("HOME")) {
        return *home / ".local" / "share" / "tempify";
    }

    throw TempifyError("Could not resolve Tempify data directory: set XDG_DATA_HOME or HOME");
}

std::filesystem::path resolve_tempify_config_root() {
    if (const auto xdg_config_home = environment_path("XDG_CONFIG_HOME")) {
        return *xdg_config_home / "tempify";
    }

    if (const auto home = environment_path("HOME")) {
        return *home / ".config" / "tempify";
    }

    throw TempifyError("Could not resolve Tempify config directory: set XDG_CONFIG_HOME or HOME");
}

std::filesystem::path default_global_config_file_path() {
    return resolve_tempify_config_root() / "config.json";
}

}
