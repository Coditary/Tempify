#pragma once

#include "tempify/domain/BuildPlan.h"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace tempify {

struct GenerationLockTemplateInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string root;
};

struct GenerationLockRecord {
    std::string tempify_version;
    GenerationLockTemplateInfo template_info;
    std::string build_root;
    std::string generated_at;
    std::string existing_path_behavior;
    std::string hook_acceptance;
    bool hooks_disabled = false;
    std::set<std::string> managed_files;
    std::map<std::string, std::string> managed_file_hashes;
    std::map<std::string, std::string> values;
};

std::string content_fingerprint_hex(std::string_view value);
std::map<std::string, std::string> build_generation_lock_managed_file_hashes(const BuildPlan &plan);
std::optional<GenerationLockRecord> load_generation_lock(const std::filesystem::path &path);

} // namespace tempify
