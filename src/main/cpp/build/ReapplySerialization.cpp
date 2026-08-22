#include "tempify/build/ReapplySerialization.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace tempify {

namespace {

struct ReapplyPathGroups {
    std::vector<std::string> create_paths;
    std::vector<std::string> update_paths;
    std::vector<std::string> delete_paths;
    std::vector<std::string> keep_paths;
    std::vector<std::string> conflict_paths;
    std::vector<std::string> review_paths;
};

std::string json_escape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::string reapply_status_name(const BuildReapplyStatus status) {
    switch (status) {
    case BuildReapplyStatus::Unavailable:
        return "unavailable";
    case BuildReapplyStatus::Ready:
        return "ready";
    case BuildReapplyStatus::Conflict:
        return "conflict";
    case BuildReapplyStatus::Review:
        return "review";
    }
    return "review";
}

std::string update_kind_name(const BuildUpdateKind kind) {
    switch (kind) {
    case BuildUpdateKind::NoOrigin:
        return "no_origin";
    case BuildUpdateKind::SameTemplate:
        return "same_template";
    case BuildUpdateKind::Upgrade:
        return "upgrade";
    case BuildUpdateKind::Downgrade:
        return "downgrade";
    case BuildUpdateKind::VersionChange:
        return "version_change";
    case BuildUpdateKind::TemplateMismatch:
        return "template_mismatch";
    }
    return "no_origin";
}

std::string update_policy_action_name(const BuildUpdatePolicyAction action) {
    switch (action) {
    case BuildUpdatePolicyAction::Unavailable:
        return "unavailable";
    case BuildUpdatePolicyAction::Allow:
        return "allow";
    case BuildUpdatePolicyAction::Review:
        return "review";
    }
    return "unavailable";
}

std::string template_label(const std::string &template_id, const std::string &template_version) {
    if (template_id.empty() && template_version.empty()) {
        return "<unknown>";
    }
    if (template_version.empty()) {
        return template_id;
    }
    if (template_id.empty()) {
        return template_version;
    }
    return template_id + "@" + template_version;
}

std::string version_label(const std::string &version) {
    return version.empty() ? std::string{"<unknown>"} : version;
}

bool update_requires_review(const BuildDiffReport &report) {
    return report.update_policy.action == BuildUpdatePolicyAction::Review;
}

std::string version_transition_summary(const ReapplyVersionTransitionInfo &transition) {
    if (transition.kind == "downgrade") {
        return "version downgrade";
    }
    if (transition.reason == "major_version_upgrade") {
        return "major version upgrade requires review";
    }
    if (transition.reason == "pre_1_0_minor_upgrade") {
        return "pre-1.0 minor upgrade requires review";
    }
    if (transition.kind == "upgrade") {
        return "version upgrade requires review";
    }
    return "version transition requires review";
}

std::string metadata_review_path(const BuildDiffReport &report) {
    if (!report.origin.detected || !update_requires_review(report)) {
        return {};
    }
    if (report.origin.lockfile_path.empty()) {
        return ".tempify-lock.json";
    }

    std::error_code error;
    const std::filesystem::path relative =
        std::filesystem::relative(report.origin.lockfile_path, report.build_root, error);
    if (!error && !relative.empty()) {
        return relative.generic_string();
    }
    return report.origin.lockfile_path.filename().generic_string();
}

ReapplyPathGroups collect_reapply_path_groups(const BuildDiffReport &report) {
    ReapplyPathGroups groups;
    for (const auto &entry : report.entries) {
        const std::string path = entry.relative_path.generic_string();
        switch (entry.reapply_action) {
        case BuildReapplyAction::None:
            break;
        case BuildReapplyAction::Create:
            groups.create_paths.push_back(path);
            break;
        case BuildReapplyAction::Update:
            groups.update_paths.push_back(path);
            break;
        case BuildReapplyAction::Delete:
            groups.delete_paths.push_back(path);
            break;
        case BuildReapplyAction::Keep:
            groups.keep_paths.push_back(path);
            break;
        case BuildReapplyAction::Conflict:
            groups.conflict_paths.push_back(path);
            break;
        case BuildReapplyAction::Review:
            groups.review_paths.push_back(path);
            break;
        }
    }

    const std::string review_path = metadata_review_path(report);
    if (!review_path.empty() &&
        std::find(groups.review_paths.begin(), groups.review_paths.end(), review_path) == groups.review_paths.end()) {
        groups.review_paths.push_back(review_path);
    }
    return groups;
}

void append_json_string_array(std::ostringstream &stream, const std::string &key,
                              const std::vector<std::string> &values, const bool trailing_comma, const int indent = 2) {
    const std::string outer(indent, ' ');
    const std::string inner(indent + 2, ' ');
    stream << outer << '"' << key << "\": [\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        stream << inner << '"' << json_escape(values[index]) << '"';
        if (index + 1 < values.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << outer << ']';
    if (trailing_comma) {
        stream << ',';
    }
    stream << '\n';
}

std::string format_path_list(const std::vector<std::string> &paths) {
    if (paths.empty()) {
        return {};
    }

    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << paths[index];
    }
    stream << ']';
    return stream.str();
}

} // namespace

std::string format_reapply_result_text(const std::string &template_id, const std::filesystem::path &build_root,
                                       const BuildDiffReport &report) {
    std::ostringstream stream;
    stream << "Reapplied " << template_id << " -> " << build_root.string() << '\n';
    stream << "Origin template: " << template_label(report.origin.template_id, report.origin.template_version) << '\n';
    stream << "Update kind: " << update_kind_name(report.update.kind) << '\n';
    stream << "Update policy: " << update_policy_action_name(report.update_policy.action) << '\n';
    stream << "Next step: " << report.update_policy.next_step << '\n';
    if (report.origin.detected && report.origin.matches_requested_template) {
        stream << "Update version: " << version_label(report.update.from_version) << " -> "
               << version_label(report.update.to_version) << '\n';
    }
    stream << "Created: " << report.reapply.create_count << '\n';
    stream << "Updated: " << report.reapply.update_count << '\n';
    stream << "Deleted: " << report.reapply.delete_count << '\n';
    stream << "Kept local edits: " << report.reapply.keep_count << '\n';
    stream << "Hooks: skipped\n";
    return stream.str();
}

std::string format_reapply_result_json(const BuildDiffReport &report) {
    const ReapplyPathGroups groups = collect_reapply_path_groups(report);

    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"status\": \"ok\",\n";
    stream << "  \"mode\": \"reapply\",\n";
    stream << "  \"template\": {\n";
    stream << "    \"id\": \"" << json_escape(report.template_id) << "\",\n";
    stream << "    \"version\": \"" << json_escape(report.template_version) << "\"\n";
    stream << "  },\n";
    stream << "  \"build_root\": \"" << json_escape(report.build_root.string()) << "\",\n";
    stream << "  \"origin\": {\n";
    stream << "    \"detected\": " << (report.origin.detected ? "true" : "false") << ",\n";
    stream << "    \"matches_requested_template\": " << (report.origin.matches_requested_template ? "true" : "false")
           << ",\n";
    stream << "    \"matches_requested_version\": " << (report.origin.matches_requested_version ? "true" : "false")
           << ",\n";
    stream << "    \"lockfile\": \"" << json_escape(report.origin.lockfile_path.string()) << "\",\n";
    stream << "    \"template_id\": \"" << json_escape(report.origin.template_id) << "\",\n";
    stream << "    \"template_version\": \"" << json_escape(report.origin.template_version) << "\",\n";
    stream << "    \"generated_at\": \"" << json_escape(report.origin.generated_at) << "\"\n";
    stream << "  },\n";
    stream << "  \"update\": {\n";
    stream << "    \"kind\": \"" << update_kind_name(report.update.kind) << "\",\n";
    stream << "    \"from_version\": \"" << json_escape(report.update.from_version) << "\",\n";
    stream << "    \"to_version\": \"" << json_escape(report.update.to_version) << "\",\n";
    stream << "    \"policy\": {\n";
    stream << "      \"action\": \"" << update_policy_action_name(report.update_policy.action) << "\",\n";
    stream << "      \"reason\": \"" << json_escape(report.update_policy.reason) << "\",\n";
    stream << "      \"next_step\": \"" << json_escape(report.update_policy.next_step) << "\"\n";
    stream << "    }\n";
    stream << "  },\n";
    stream << "  \"reapply\": {\n";
    stream << "    \"status\": \"" << reapply_status_name(report.reapply.status) << "\",\n";
    stream << "    \"counts\": {\n";
    stream << "      \"create\": " << report.reapply.create_count << ",\n";
    stream << "      \"update\": " << report.reapply.update_count << ",\n";
    stream << "      \"delete\": " << report.reapply.delete_count << ",\n";
    stream << "      \"keep\": " << report.reapply.keep_count << ",\n";
    stream << "      \"conflict\": " << report.reapply.conflict_count << ",\n";
    stream << "      \"review\": " << report.reapply.review_count << "\n";
    stream << "    }\n";
    stream << "  },\n";
    stream << "  \"applied\": {\n";
    append_json_string_array(stream, "create", groups.create_paths, true, 4);
    append_json_string_array(stream, "update", groups.update_paths, true, 4);
    append_json_string_array(stream, "delete", groups.delete_paths, false, 4);
    stream << "  },\n";
    append_json_string_array(stream, "kept", groups.keep_paths, true, 2);
    stream << "  \"blocked\": {\n";
    append_json_string_array(stream, "conflict", groups.conflict_paths, true, 4);
    append_json_string_array(stream, "review", groups.review_paths, false, 4);
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

ReapplyBlockedError build_reapply_blocked_error(const BuildDiffReport &report) {
    const ReapplyPathGroups groups = collect_reapply_path_groups(report);
    const std::string review_gate_path = metadata_review_path(report);
    std::vector<std::string> review_paths = groups.review_paths;
    std::optional<ReapplyOriginMismatchInfo> origin_mismatch;
    std::optional<ReapplyVersionTransitionInfo> version_transition;
    if (!review_gate_path.empty()) {
        std::erase(review_paths, review_gate_path);
    }
    if (report.update.kind == BuildUpdateKind::TemplateMismatch && !review_gate_path.empty()) {
        origin_mismatch = ReapplyOriginMismatchInfo{
            .lockfile_path = review_gate_path,
            .origin_template_id = report.origin.template_id,
            .origin_template_version = report.origin.template_version,
            .requested_template_id = report.template_id,
            .requested_template_version = report.template_version,
        };
    }
    if (report.update_policy.action == BuildUpdatePolicyAction::Review && report.origin.detected &&
        report.origin.matches_requested_template && report.update.kind != BuildUpdateKind::NoOrigin &&
        report.update.kind != BuildUpdateKind::TemplateMismatch && !review_gate_path.empty()) {
        version_transition = ReapplyVersionTransitionInfo{
            .lockfile_path = review_gate_path,
            .kind = update_kind_name(report.update.kind),
            .reason = report.update_policy.reason,
            .from_version = report.update.from_version,
            .to_version = report.update.to_version,
        };
    }

    std::ostringstream stream;
    stream << "Reapply blocked for target '" << report.build_root.string() << "': ";
    bool wrote_detail = false;
    if (origin_mismatch.has_value()) {
        stream << "origin template mismatch at " << origin_mismatch->lockfile_path
               << " (lockfile: " << template_label(report.origin.template_id, report.origin.template_version)
               << ", requested: " << template_label(report.template_id, report.template_version) << ')';
        wrote_detail = true;
    }
    if (version_transition.has_value()) {
        if (wrote_detail) {
            stream << ", ";
        }
        stream << version_transition_summary(*version_transition) << " at " << version_transition->lockfile_path
               << " (lockfile: " << version_label(version_transition->from_version)
               << ", requested: " << version_label(version_transition->to_version) << ')';
        wrote_detail = true;
    }
    if (report.reapply.conflict_count > 0) {
        if (wrote_detail) {
            stream << ", ";
        }
        stream << report.reapply.conflict_count << " conflict item(s) " << format_path_list(groups.conflict_paths);
        wrote_detail = true;
    }
    if (!review_paths.empty()) {
        if (wrote_detail) {
            stream << ", ";
        }
        stream << review_paths.size() << " review item(s) " << format_path_list(review_paths);
        wrote_detail = true;
    }
    if (!wrote_detail) {
        stream << "status unavailable";
    }
    stream << ". Run `tempify <template-id> <target> --diff` to inspect.";
    return ReapplyBlockedError(stream.str(), std::move(groups.conflict_paths), std::move(groups.review_paths),
                               std::move(origin_mismatch), std::move(version_transition));
}

std::string format_reapply_blocked_error_json(const ReapplyBlockedError &error) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"status\": \"error\",\n";
    stream << "  \"code\": \"REAPPLY_BLOCKED\",\n";
    stream << "  \"message\": \"" << json_escape(error.what()) << "\",\n";
    stream << "  \"blocked\": {\n";
    append_json_string_array(stream, "conflict", error.conflict_paths(), true, 4);
    append_json_string_array(stream, "review", error.review_paths(), true, 4);
    stream << "    \"origin_mismatch\": ";
    if (!error.origin_mismatch().has_value()) {
        stream << "null,\n";
    } else {
        const auto &mismatch = *error.origin_mismatch();
        stream << "{\n";
        stream << "      \"lockfile\": \"" << json_escape(mismatch.lockfile_path) << "\",\n";
        stream << "      \"origin_template\": {\n";
        stream << "        \"id\": \"" << json_escape(mismatch.origin_template_id) << "\",\n";
        stream << "        \"version\": \"" << json_escape(mismatch.origin_template_version) << "\"\n";
        stream << "      },\n";
        stream << "      \"requested_template\": {\n";
        stream << "        \"id\": \"" << json_escape(mismatch.requested_template_id) << "\",\n";
        stream << "        \"version\": \"" << json_escape(mismatch.requested_template_version) << "\"\n";
        stream << "      }\n";
        stream << "    },\n";
    }
    stream << "    \"version_transition\": ";
    if (!error.version_transition().has_value()) {
        stream << "null\n";
    } else {
        const auto &transition = *error.version_transition();
        stream << "{\n";
        stream << "      \"lockfile\": \"" << json_escape(transition.lockfile_path) << "\",\n";
        stream << "      \"kind\": \"" << json_escape(transition.kind) << "\",\n";
        stream << "      \"reason\": \"" << json_escape(transition.reason) << "\",\n";
        stream << "      \"from_version\": \"" << json_escape(transition.from_version) << "\",\n";
        stream << "      \"to_version\": \"" << json_escape(transition.to_version) << "\"\n";
        stream << "    }\n";
    }
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

} // namespace tempify
