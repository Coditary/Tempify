#pragma once

#include "tempify/domain/RenderOptions.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

struct PlannedFile {
    std::filesystem::path source_path;
    std::filesystem::path output_path;
    bool render_with_prebyte = false;
};

struct BuildContext {
    std::filesystem::path template_root;
    std::filesystem::path build_root;
    std::map<std::string, std::string> values;
};

struct BuildPlan {
    std::filesystem::path build_root;
    ExistingPathBehavior existing_path_behavior = ExistingPathBehavior::Error;
    std::vector<std::filesystem::path> directories;
    std::vector<PlannedFile> files;
    std::optional<std::filesystem::path> pre_hook_path;
    std::optional<std::filesystem::path> before_render_hook_path;
    std::optional<std::filesystem::path> after_render_hook_path;
    std::optional<std::filesystem::path> post_hook_path;
};

struct BuildPlanReport {
    std::filesystem::path build_root;
    ExistingPathBehavior existing_path_behavior = ExistingPathBehavior::Error;
    std::vector<std::filesystem::path> directories;
    std::vector<PlannedFile> files;
    std::vector<std::string> hooks;
};

}
