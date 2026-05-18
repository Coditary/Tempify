#pragma once

#include "tempify/domain/Question.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

struct TemplateInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::filesystem::path root;
};

struct PrebyteConfig {
    std::vector<std::filesystem::path> include_paths;
    std::map<std::string, std::string> rules;
};

enum class ConflictStrategy {
    Replace,
    Keep,
    Error,
};

struct TemplateSourceRoot {
    std::filesystem::path path;
    std::string template_id;
};

struct TemplateDirectoryEntry {
    std::string relative_path;
};

struct TemplateFileEntry {
    std::string relative_path;
    std::filesystem::path source_path;
    bool render_with_prebyte = false;
    std::string source_template_id;
    bool excluded = false;
};

struct LayoutRule {
    std::string source;
    std::optional<std::string> target;
    bool exclude = false;
    std::optional<bool> render;
    std::filesystem::path source_path;
    std::string source_template_id;
};

struct ScriptCatalogEntry {
    std::string name;
    std::filesystem::path path;
};

struct TemplateMergeConfig {
    std::map<std::string, ConflictStrategy> file_conflicts;
    std::vector<std::string> drop_paths;
    std::optional<ConflictStrategy> question_conflicts;
    std::optional<ConflictStrategy> pre_hook_conflict;
    std::optional<ConflictStrategy> post_hook_conflict;
};

struct TemplateManifest {
    TemplateInfo info;
    std::filesystem::path root;
    std::filesystem::path source_dir = "files";
    std::vector<TemplateSourceRoot> source_roots;
    std::vector<TemplateDirectoryEntry> directories;
    std::vector<TemplateFileEntry> files;
    std::string version;
    std::string output_path_template;
    bool overwrite = false;
    std::vector<std::string> include_ids;
    std::vector<std::string> question_group_order;
    std::vector<QuestionDefinition> questions;
    std::map<std::string, std::string> env_defaults;
    PrebyteConfig prebyte_config;
    TemplateMergeConfig merge_config;
    std::vector<LayoutRule> layout_rules;
    std::vector<ScriptCatalogEntry> scripts;
    std::optional<std::filesystem::path> pre_hook_path;
    std::optional<std::filesystem::path> before_render_hook_path;
    std::optional<std::filesystem::path> after_render_hook_path;
    std::optional<std::filesystem::path> post_hook_path;
};

}
