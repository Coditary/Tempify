#include "tempify/support/Paths.h"

#include "tempify/support/Errors.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <optional>

namespace tempify {

namespace {

enum class SearchCeilingMode {
    Inclusive,
    Exclusive,
};

struct SearchCeiling {
    std::filesystem::path path;
    SearchCeilingMode mode = SearchCeilingMode::Inclusive;
};

std::optional<std::filesystem::path> environment_path(const char *name) {
    if (const char *value = std::getenv(name)) {
        if (*value != '\0') {
            return std::filesystem::path(value);
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> first_environment_path(std::initializer_list<const char *> names) {
    for (const char *name : names) {
        if (const auto path = environment_path(name)) {
            return path;
        }
    }
    return std::nullopt;
}

std::filesystem::path absolute_path(const std::filesystem::path &path) {
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) {
        return path.lexically_normal();
    }
    return absolute.lexically_normal();
}

bool is_directory_noexcept(const std::filesystem::path &path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

bool is_regular_file_noexcept(const std::filesystem::path &path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

bool exists_noexcept(const std::filesystem::path &path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::string normalized_component(const std::filesystem::path &component) {
    std::string value = component.generic_string();
#if defined(_WIN32)
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif
    return value;
}

bool path_starts_with(const std::filesystem::path &path, const std::filesystem::path &prefix) {
    const std::filesystem::path normalized_path = path.lexically_normal();
    const std::filesystem::path normalized_prefix = prefix.lexically_normal();
    auto path_it = normalized_path.begin();
    auto prefix_it = normalized_prefix.begin();
    for (; prefix_it != normalized_prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == normalized_path.end()) {
            return false;
        }
        if (normalized_component(*path_it) != normalized_component(*prefix_it)) {
            return false;
        }
    }
    return true;
}

bool reached_exclusive_ceiling(const std::filesystem::path &current, const std::optional<SearchCeiling> &ceiling) {
    return ceiling.has_value() && ceiling->mode == SearchCeilingMode::Exclusive && current == ceiling->path;
}

bool reached_inclusive_ceiling(const std::filesystem::path &current, const std::optional<SearchCeiling> &ceiling) {
    return ceiling.has_value() && ceiling->mode == SearchCeilingMode::Inclusive && current == ceiling->path;
}

std::optional<SearchCeiling> search_ceiling(const std::filesystem::path &start) {
    const std::filesystem::path absolute_start = absolute_path(start);

    std::error_code error;
    const std::filesystem::path temp_root = std::filesystem::temp_directory_path(error);
    const std::filesystem::path absolute_temp_root = absolute_path(temp_root);
    if (!error && absolute_start != absolute_temp_root && path_starts_with(absolute_start, absolute_temp_root)) {
        return SearchCeiling{absolute_temp_root, SearchCeilingMode::Exclusive};
    }

    std::filesystem::path current = absolute_start;
    while (true) {
        if (exists_noexcept(current / ".git")) {
            return SearchCeiling{current, SearchCeilingMode::Inclusive};
        }
        if (current == current.root_path() || current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

} // namespace

std::optional<std::filesystem::path> find_workspace_templates_root(const std::filesystem::path &start) {
    std::filesystem::path current = absolute_path(start);
    const std::optional<SearchCeiling> ceiling = search_ceiling(current);

    while (true) {
        if (reached_exclusive_ceiling(current, ceiling)) {
            return std::nullopt;
        }

        if (is_directory_noexcept(current / "templates")) {
            return current / "templates";
        }

        if (reached_inclusive_ceiling(current, ceiling) || current == current.root_path() ||
            current == current.parent_path()) {
            return std::nullopt;
        }

        current = current.parent_path();
    }
}

std::optional<std::filesystem::path> find_workspace_config_file(const std::filesystem::path &start) {
    std::filesystem::path current = absolute_path(start);
    const std::optional<SearchCeiling> ceiling = search_ceiling(current);

    while (true) {
        if (reached_exclusive_ceiling(current, ceiling)) {
            return std::nullopt;
        }

        const std::filesystem::path candidate = current / ".tempify" / "config.json";
        if (is_regular_file_noexcept(candidate)) {
            return candidate;
        }

        if (reached_inclusive_ceiling(current, ceiling) || current == current.root_path() ||
            current == current.parent_path()) {
            return std::nullopt;
        }

        current = current.parent_path();
    }
}

std::filesystem::path resolve_tempify_data_root() {
    if (const auto xdg_data_home = environment_path("XDG_DATA_HOME")) {
        return *xdg_data_home / "tempify";
    }

#if defined(_WIN32)
    if (const auto local_app_data = first_environment_path({"LOCALAPPDATA", "APPDATA"})) {
        return *local_app_data / "tempify";
    }

    if (const auto user_profile = environment_path("USERPROFILE")) {
        return *user_profile / "AppData" / "Local" / "tempify";
    }
#endif

    if (const auto home = environment_path("HOME")) {
        return *home / ".local" / "share" / "tempify";
    }

    throw TempifyError("Could not resolve Tempify data directory: set XDG_DATA_HOME or HOME");
}

std::filesystem::path resolve_tempify_config_root() {
    if (const auto xdg_config_home = environment_path("XDG_CONFIG_HOME")) {
        return *xdg_config_home / "tempify";
    }

#if defined(_WIN32)
    if (const auto app_data = first_environment_path({"APPDATA", "LOCALAPPDATA"})) {
        return *app_data / "tempify";
    }

    if (const auto user_profile = environment_path("USERPROFILE")) {
        return *user_profile / "AppData" / "Roaming" / "tempify";
    }
#endif

    if (const auto home = environment_path("HOME")) {
        return *home / ".config" / "tempify";
    }

    throw TempifyError("Could not resolve Tempify config directory: set XDG_CONFIG_HOME or HOME");
}

std::filesystem::path default_global_config_file_path() {
    return resolve_tempify_config_root() / "config.json";
}

} // namespace tempify
