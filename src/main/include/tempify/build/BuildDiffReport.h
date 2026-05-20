#pragma once

#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tempify {

class PrebyteRenderer;

enum class BuildDiffStatus {
    Unchanged,
    Create,
    Change,
    Delete,
};

enum class BuildDiffReason {
    None,
    Unknown,
    LocalEdit,
    TemplateUpdate,
    Conflict,
    TypeMismatch,
};

enum class BuildReapplyAction {
    None,
    Create,
    Update,
    Delete,
    Keep,
    Conflict,
    Review,
};

enum class BuildReapplyStatus {
    Unavailable,
    Ready,
    Conflict,
    Review,
};

enum class BuildUpdateKind {
    NoOrigin,
    SameTemplate,
    Upgrade,
    Downgrade,
    VersionChange,
    TemplateMismatch,
};

enum class BuildUpdatePolicyAction {
    Unavailable,
    Allow,
    Review,
};

struct BuildDiffEntry {
    std::filesystem::path relative_path;
    BuildDiffStatus status = BuildDiffStatus::Unchanged;
    BuildDiffReason reason = BuildDiffReason::None;
    BuildReapplyAction reapply_action = BuildReapplyAction::None;
};

struct BuildOriginInfo {
    bool detected = false;
    bool matches_requested_template = false;
    bool matches_requested_version = false;
    std::filesystem::path lockfile_path;
    std::string template_id;
    std::string template_version;
    std::string generated_at;
};

struct BuildUpdateInfo {
    BuildUpdateKind kind = BuildUpdateKind::NoOrigin;
    std::string from_version;
    std::string to_version;
};

struct BuildUpdatePolicy {
    BuildUpdatePolicyAction action = BuildUpdatePolicyAction::Unavailable;
    std::string reason;
    std::string next_step;
};

struct BuildReapplySummary {
    BuildReapplyStatus status = BuildReapplyStatus::Unavailable;
    std::size_t create_count = 0;
    std::size_t update_count = 0;
    std::size_t delete_count = 0;
    std::size_t keep_count = 0;
    std::size_t conflict_count = 0;
    std::size_t review_count = 0;
};

struct BuildDiffReport {
    std::filesystem::path build_root;
    std::string template_id;
    std::string template_version;
    BuildOriginInfo origin;
    BuildUpdateInfo update;
    BuildUpdatePolicy update_policy;
    BuildReapplySummary reapply;
    std::vector<BuildDiffEntry> entries;
};

BuildDiffReport build_diff_report(const BuildPlan& plan,
                                  const TemplateManifest& manifest,
                                  const std::map<std::string, std::string>& values,
                                  const PrebyteRenderer& renderer);

std::string format_build_diff_text(const BuildDiffReport& report);
std::string format_build_diff_json(const BuildDiffReport& report);

}
