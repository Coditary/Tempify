#include "tempify/build/BuildDiffReport.h"

#include "tempify/build/GenerationLock.h"

#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/support/Errors.h"

#include "PrebyteEngine.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace tempify {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw TempifyError("Could not read file: " + path.string());
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::string render_planned_file(const PlannedFile& file,
                                const PrebyteRenderer& renderer,
                                prebyte::Prebyte& engine) {
    static_cast<void>(renderer);
    if (file.render_with_prebyte) {
        return renderer.render_string(engine, read_file(file.source_path));
    }
    return read_file(file.source_path);
}

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string status_name(const BuildDiffStatus status) {
    switch (status) {
    case BuildDiffStatus::Unchanged:
        return "unchanged";
    case BuildDiffStatus::Create:
        return "create";
    case BuildDiffStatus::Change:
        return "change";
    case BuildDiffStatus::Delete:
        return "delete";
    }
    return "change";
}

std::string reason_name(const BuildDiffReason reason) {
    switch (reason) {
    case BuildDiffReason::None:
        return "";
    case BuildDiffReason::Unknown:
        return "unknown";
    case BuildDiffReason::LocalEdit:
        return "local_edit";
    case BuildDiffReason::TemplateUpdate:
        return "template_update";
    case BuildDiffReason::Conflict:
        return "conflict";
    case BuildDiffReason::TypeMismatch:
        return "type_mismatch";
    }
    return "unknown";
}

std::string reapply_action_name(const BuildReapplyAction action) {
    switch (action) {
    case BuildReapplyAction::None:
        return "";
    case BuildReapplyAction::Create:
        return "create";
    case BuildReapplyAction::Update:
        return "update";
    case BuildReapplyAction::Delete:
        return "delete";
    case BuildReapplyAction::Keep:
        return "keep";
    case BuildReapplyAction::Conflict:
        return "conflict";
    case BuildReapplyAction::Review:
        return "review";
    }
    return "review";
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

std::string template_label(const std::string& template_id, const std::string& template_version) {
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

std::string version_label(const std::string& version) {
    return version.empty() ? std::string{"<unknown>"} : version;
}

struct ParsedSemVerIdentifier {
    std::string value;
    bool numeric = false;
    int numeric_value = 0;
};

struct ParsedSemVer {
    std::vector<int> core;
    std::vector<ParsedSemVerIdentifier> prerelease;
};

std::optional<int> parse_numeric_identifier(const std::string_view token) {
    if (token.empty()) {
        return std::nullopt;
    }

    int value = 0;
    for (const unsigned char ch : token) {
        if (!std::isdigit(ch)) {
            return std::nullopt;
        }
        if (value > (std::numeric_limits<int>::max() - static_cast<int>(ch - '0')) / 10) {
            return std::nullopt;
        }
        value = value * 10 + static_cast<int>(ch - '0');
    }
    return value;
}

bool is_semver_identifier_char(const unsigned char ch) {
    return std::isalnum(ch) || ch == '-';
}

std::optional<ParsedSemVer> parse_semver(std::string_view version) {
    if (version.empty()) {
        return std::nullopt;
    }
    if (version.front() == 'v' || version.front() == 'V') {
        version.remove_prefix(1);
    }
    if (version.empty()) {
        return std::nullopt;
    }

    std::string_view build = {};
    const std::size_t build_pos = version.find('+');
    if (build_pos != std::string_view::npos) {
        build = version.substr(build_pos + 1);
        version = version.substr(0, build_pos);
    }
    if (version.empty()) {
        return std::nullopt;
    }

    std::string_view prerelease = {};
    const std::size_t prerelease_pos = version.find('-');
    if (prerelease_pos != std::string_view::npos) {
        prerelease = version.substr(prerelease_pos + 1);
        version = version.substr(0, prerelease_pos);
    }
    if (version.empty()) {
        return std::nullopt;
    }

    ParsedSemVer parsed;
    std::size_t start = 0;
    while (start <= version.size()) {
        const std::size_t end = version.find('.', start);
        const std::string_view token = end == std::string_view::npos
            ? version.substr(start)
            : version.substr(start, end - start);
        if (token.empty()) {
            return std::nullopt;
        }

        const auto value = parse_numeric_identifier(token);
        if (!value.has_value()) {
            return std::nullopt;
        }
        parsed.core.push_back(*value);

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    while (parsed.core.size() > 1 && parsed.core.back() == 0) {
        parsed.core.pop_back();
    }

    if (!build.empty()) {
        std::size_t build_start = 0;
        while (build_start <= build.size()) {
            const std::size_t build_end = build.find('.', build_start);
            const std::string_view token = build_end == std::string_view::npos
                ? build.substr(build_start)
                : build.substr(build_start, build_end - build_start);
            if (token.empty()) {
                return std::nullopt;
            }
            for (const unsigned char ch : token) {
                if (!is_semver_identifier_char(ch)) {
                    return std::nullopt;
                }
            }
            if (build_end == std::string_view::npos) {
                break;
            }
            build_start = build_end + 1;
        }
    }

    if (!prerelease.empty()) {
        std::size_t prerelease_start = 0;
        while (prerelease_start <= prerelease.size()) {
            const std::size_t prerelease_end = prerelease.find('.', prerelease_start);
            const std::string_view token = prerelease_end == std::string_view::npos
                ? prerelease.substr(prerelease_start)
                : prerelease.substr(prerelease_start, prerelease_end - prerelease_start);
            if (token.empty()) {
                return std::nullopt;
            }

            for (const unsigned char ch : token) {
                if (!is_semver_identifier_char(ch)) {
                    return std::nullopt;
                }
            }

            ParsedSemVerIdentifier identifier{
                .value = std::string{token},
            };
            if (const auto numeric_value = parse_numeric_identifier(token); numeric_value.has_value()) {
                identifier.numeric = true;
                identifier.numeric_value = *numeric_value;
            }
            parsed.prerelease.push_back(std::move(identifier));

            if (prerelease_end == std::string_view::npos) {
                break;
            }
            prerelease_start = prerelease_end + 1;
        }
    }

    return parsed;
}

int compare_version_components(const std::vector<int>& left,
                               const std::vector<int>& right) {
    const std::size_t size = std::max(left.size(), right.size());
    for (std::size_t index = 0; index < size; ++index) {
        const int left_value = index < left.size() ? left[index] : 0;
        const int right_value = index < right.size() ? right[index] : 0;
        if (left_value < right_value) {
            return -1;
        }
        if (left_value > right_value) {
            return 1;
        }
    }
    return 0;
}

int compare_prerelease_identifiers(const std::vector<ParsedSemVerIdentifier>& left,
                                   const std::vector<ParsedSemVerIdentifier>& right) {
    const std::size_t size = std::max(left.size(), right.size());
    for (std::size_t index = 0; index < size; ++index) {
        if (index >= left.size()) {
            return -1;
        }
        if (index >= right.size()) {
            return 1;
        }

        const auto& left_identifier = left[index];
        const auto& right_identifier = right[index];
        if (left_identifier.numeric && right_identifier.numeric) {
            if (left_identifier.numeric_value < right_identifier.numeric_value) {
                return -1;
            }
            if (left_identifier.numeric_value > right_identifier.numeric_value) {
                return 1;
            }
            continue;
        }

        if (left_identifier.numeric != right_identifier.numeric) {
            return left_identifier.numeric ? -1 : 1;
        }

        if (left_identifier.value < right_identifier.value) {
            return -1;
        }
        if (left_identifier.value > right_identifier.value) {
            return 1;
        }
    }
    return 0;
}

int compare_semver(const ParsedSemVer& left,
                   const ParsedSemVer& right) {
    const int core_comparison = compare_version_components(left.core, right.core);
    if (core_comparison != 0) {
        return core_comparison;
    }

    const bool left_prerelease = !left.prerelease.empty();
    const bool right_prerelease = !right.prerelease.empty();
    if (!left_prerelease && !right_prerelease) {
        return 0;
    }
    if (!left_prerelease) {
        return 1;
    }
    if (!right_prerelease) {
        return -1;
    }

    return compare_prerelease_identifiers(left.prerelease, right.prerelease);
}

int semver_core_component(const ParsedSemVer& version,
                          const std::size_t index) {
    return index < version.core.size() ? version.core[index] : 0;
}

BuildUpdatePolicy build_update_policy(const BuildUpdateInfo& update) {
    switch (update.kind) {
    case BuildUpdateKind::NoOrigin:
        return BuildUpdatePolicy{
            .action = BuildUpdatePolicyAction::Unavailable,
            .reason = "missing_origin_lock",
            .next_step = "Restore .tempify-lock.json or regenerate project before reapply.",
        };
    case BuildUpdateKind::SameTemplate:
        if (update.from_version != update.to_version) {
            return BuildUpdatePolicy{
                .action = BuildUpdatePolicyAction::Allow,
                .reason = "equivalent_version_change",
                .next_step = "Equivalent SemVer precedence detected. Review diff, then reapply ready actions if metadata-only version drift is expected.",
            };
        }
        return BuildUpdatePolicy{
            .action = BuildUpdatePolicyAction::Allow,
            .reason = "matching_template_and_version",
            .next_step = "Review diff and reapply ready actions when output matches intended template state.",
        };
    case BuildUpdateKind::Upgrade: {
        const auto from_version = parse_semver(update.from_version);
        const auto to_version = parse_semver(update.to_version);
        if (from_version.has_value() && to_version.has_value()) {
            const int from_major = semver_core_component(*from_version, 0);
            const int to_major = semver_core_component(*to_version, 0);
            const int from_minor = semver_core_component(*from_version, 1);
            const int to_minor = semver_core_component(*to_version, 1);
            if (to_major > from_major) {
                return BuildUpdatePolicy{
                    .action = BuildUpdatePolicyAction::Review,
                    .reason = "major_version_upgrade",
                    .next_step = "Major version upgrade detected. Automatic reapply blocked; review compatibility and managed-file changes before continuing.",
                };
            }
            if (from_major == 0 && to_major == 0 && to_minor > from_minor) {
                return BuildUpdatePolicy{
                    .action = BuildUpdatePolicyAction::Review,
                    .reason = "pre_1_0_minor_upgrade",
                    .next_step = "Pre-1.0 minor upgrade detected. Automatic reapply blocked; review compatibility before continuing.",
                };
            }
        }
        return BuildUpdatePolicy{
            .action = BuildUpdatePolicyAction::Allow,
            .reason = "forward_version_change",
            .next_step = "Version upgrade detected. Review diff, then reapply ready actions to move managed files forward.",
        };
    }
    case BuildUpdateKind::Downgrade:
        return BuildUpdatePolicy{
            .action = BuildUpdatePolicyAction::Review,
            .reason = "backward_version_change",
            .next_step = "Version downgrade detected. Automatic reapply blocked; inspect lockfile and template version before continuing.",
        };
    case BuildUpdateKind::VersionChange:
        return BuildUpdatePolicy{
            .action = BuildUpdatePolicyAction::Review,
            .reason = "unclassified_version_change",
            .next_step = "Version ordering unknown. Review template change manually before reapply.",
        };
    case BuildUpdateKind::TemplateMismatch:
        return BuildUpdatePolicy{
            .action = BuildUpdatePolicyAction::Review,
            .reason = "template_id_mismatch",
            .next_step = "Template id mismatch. Use matching template id or regenerate from correct template origin.",
        };
    }

    return BuildUpdatePolicy{};
}

std::string update_recommendation(const BuildDiffReport& report) {
    return report.update_policy.next_step;
}

bool origin_matches_requested_template(const GenerationLockRecord& lock,
                                       const TemplateManifest& manifest) {
    return !lock.template_info.id.empty() && lock.template_info.id == manifest.info.id;
}

std::optional<std::string> find_managed_file_hash(const std::optional<GenerationLockRecord>& lock,
                                                  const std::string& relative_path) {
    if (!lock.has_value()) {
        return std::nullopt;
    }
    const auto it = lock->managed_file_hashes.find(relative_path);
    if (it == lock->managed_file_hashes.end()) {
        return std::nullopt;
    }
    return it->second;
}

BuildDiffReason classify_change_reason(const bool origin_detected,
                                       const std::optional<std::string>& baseline_hash,
                                       const std::string& actual_hash,
                                       const std::string& desired_hash) {
    if (!baseline_hash.has_value()) {
        return origin_detected ? BuildDiffReason::Unknown : BuildDiffReason::None;
    }

    const bool actual_matches_baseline = actual_hash == *baseline_hash;
    const bool desired_matches_baseline = desired_hash == *baseline_hash;
    if (!actual_matches_baseline && desired_matches_baseline) {
        return BuildDiffReason::LocalEdit;
    }
    if (actual_matches_baseline && !desired_matches_baseline) {
        return BuildDiffReason::TemplateUpdate;
    }
    if (!actual_matches_baseline && !desired_matches_baseline) {
        return BuildDiffReason::Conflict;
    }
    return BuildDiffReason::Unknown;
}

BuildDiffReason classify_create_reason(const bool origin_detected,
                                       const bool previously_managed,
                                       const std::optional<std::string>& baseline_hash,
                                       const std::string& desired_hash) {
    if (!previously_managed) {
        return origin_detected ? BuildDiffReason::TemplateUpdate : BuildDiffReason::None;
    }
    if (!baseline_hash.has_value()) {
        return origin_detected ? BuildDiffReason::Unknown : BuildDiffReason::None;
    }
    return desired_hash == *baseline_hash ? BuildDiffReason::LocalEdit : BuildDiffReason::Conflict;
}

BuildDiffReason classify_delete_reason(const bool origin_detected,
                                       const std::optional<std::string>& baseline_hash,
                                       const std::filesystem::path& actual_path) {
    if (!std::filesystem::is_regular_file(actual_path)) {
        return BuildDiffReason::TypeMismatch;
    }
    if (!baseline_hash.has_value()) {
        return origin_detected ? BuildDiffReason::Unknown : BuildDiffReason::None;
    }
    const std::string actual_hash = content_fingerprint_hex(read_file(actual_path));
    return actual_hash == *baseline_hash ? BuildDiffReason::TemplateUpdate : BuildDiffReason::LocalEdit;
}

BuildReapplyAction classify_reapply_action(const BuildDiffStatus status,
                                           const BuildDiffReason reason) {
    switch (status) {
    case BuildDiffStatus::Unchanged:
        return BuildReapplyAction::None;
    case BuildDiffStatus::Create:
        switch (reason) {
        case BuildDiffReason::None:
        case BuildDiffReason::TemplateUpdate:
            return BuildReapplyAction::Create;
        case BuildDiffReason::LocalEdit:
        case BuildDiffReason::Conflict:
            return BuildReapplyAction::Conflict;
        case BuildDiffReason::Unknown:
        case BuildDiffReason::TypeMismatch:
            return BuildReapplyAction::Review;
        }
        break;
    case BuildDiffStatus::Change:
        switch (reason) {
        case BuildDiffReason::LocalEdit:
            return BuildReapplyAction::Keep;
        case BuildDiffReason::TemplateUpdate:
            return BuildReapplyAction::Update;
        case BuildDiffReason::Conflict:
            return BuildReapplyAction::Conflict;
        case BuildDiffReason::Unknown:
        case BuildDiffReason::TypeMismatch:
        case BuildDiffReason::None:
            return BuildReapplyAction::Review;
        }
        break;
    case BuildDiffStatus::Delete:
        switch (reason) {
        case BuildDiffReason::TemplateUpdate:
            return BuildReapplyAction::Delete;
        case BuildDiffReason::LocalEdit:
        case BuildDiffReason::Conflict:
            return BuildReapplyAction::Conflict;
        case BuildDiffReason::Unknown:
        case BuildDiffReason::TypeMismatch:
        case BuildDiffReason::None:
            return BuildReapplyAction::Review;
        }
        break;
    }
    return BuildReapplyAction::Review;
}

BuildReapplySummary build_reapply_summary(const BuildDiffReport& report) {
    BuildReapplySummary summary;
    for (const auto& entry : report.entries) {
        switch (entry.reapply_action) {
        case BuildReapplyAction::None:
            break;
        case BuildReapplyAction::Create:
            ++summary.create_count;
            break;
        case BuildReapplyAction::Update:
            ++summary.update_count;
            break;
        case BuildReapplyAction::Delete:
            ++summary.delete_count;
            break;
        case BuildReapplyAction::Keep:
            ++summary.keep_count;
            break;
        case BuildReapplyAction::Conflict:
            ++summary.conflict_count;
            break;
        case BuildReapplyAction::Review:
            ++summary.review_count;
            break;
        }
    }

    if (report.update_policy.action == BuildUpdatePolicyAction::Review) {
        ++summary.review_count;
    }

    if (!report.origin.detected) {
        summary.status = BuildReapplyStatus::Unavailable;
    } else if (report.update_policy.action == BuildUpdatePolicyAction::Review) {
        summary.status = BuildReapplyStatus::Review;
    } else if (summary.review_count > 0) {
        summary.status = BuildReapplyStatus::Review;
    } else if (summary.conflict_count > 0) {
        summary.status = BuildReapplyStatus::Conflict;
    } else {
        summary.status = BuildReapplyStatus::Ready;
    }

    return summary;
}

BuildUpdateInfo build_update_info(const BuildDiffReport& report) {
    BuildUpdateInfo update;
    update.from_version = report.origin.template_version;
    update.to_version = report.template_version;

    if (!report.origin.detected) {
        update.kind = BuildUpdateKind::NoOrigin;
    } else if (!report.origin.matches_requested_template) {
        update.kind = BuildUpdateKind::TemplateMismatch;
    } else if (report.origin.matches_requested_version) {
        update.kind = BuildUpdateKind::SameTemplate;
    } else {
        const auto from_version = parse_semver(report.origin.template_version);
        const auto to_version = parse_semver(report.template_version);
        if (from_version.has_value() && to_version.has_value()) {
            const int comparison = compare_semver(*from_version, *to_version);
            if (comparison < 0) {
                update.kind = BuildUpdateKind::Upgrade;
            } else if (comparison > 0) {
                update.kind = BuildUpdateKind::Downgrade;
            } else {
                update.kind = BuildUpdateKind::SameTemplate;
            }
        } else {
            update.kind = BuildUpdateKind::VersionChange;
        }
    }

    return update;
}

std::array<std::size_t, 4> status_counts(const BuildDiffReport& report) {
    std::array<std::size_t, 4> counts = {0, 0, 0, 0};
    for (const auto& entry : report.entries) {
        switch (entry.status) {
        case BuildDiffStatus::Unchanged:
            ++counts[0];
            break;
        case BuildDiffStatus::Create:
            ++counts[1];
            break;
        case BuildDiffStatus::Change:
            ++counts[2];
            break;
        case BuildDiffStatus::Delete:
            ++counts[3];
            break;
        }
    }
    return counts;
}

}

BuildDiffReport build_diff_report(const BuildPlan& plan,
                                  const TemplateManifest& manifest,
                                  const std::map<std::string, std::string>& values,
                                  const PrebyteRenderer& renderer) {
    prebyte::Prebyte engine;
    renderer.configure(engine, values, manifest);

    const std::filesystem::path lock_path = plan.build_root / ".tempify-lock.json";
    const std::optional<GenerationLockRecord> previous_lock = load_generation_lock(lock_path);

    BuildDiffReport report{
        .build_root = plan.build_root,
        .template_id = manifest.info.id,
        .template_version = manifest.info.version,
    };
    if (previous_lock.has_value()) {
        report.origin.detected = true;
        report.origin.matches_requested_template = origin_matches_requested_template(*previous_lock, manifest);
        report.origin.matches_requested_version = report.origin.matches_requested_template
            && previous_lock->template_info.version == manifest.info.version;
        report.origin.lockfile_path = lock_path;
        report.origin.template_id = previous_lock->template_info.id;
        report.origin.template_version = previous_lock->template_info.version;
        report.origin.generated_at = previous_lock->generated_at;
    }

    report.entries.reserve(plan.files.size());
    const std::set<std::string> previous_managed_files = previous_lock.has_value()
        ? previous_lock->managed_files
        : std::set<std::string>{};
    std::set<std::string> current_managed_files;

    for (const auto& file : plan.files) {
        const std::filesystem::path relative_path = std::filesystem::relative(file.output_path, plan.build_root);
        const std::string relative_key = relative_path.generic_string();
        current_managed_files.insert(relative_key);
        const bool previously_managed = previous_managed_files.contains(relative_key);
        const std::optional<std::string> baseline_hash = find_managed_file_hash(previous_lock, relative_key);
        const std::string expected = render_planned_file(file, renderer, engine);
        const std::string expected_hash = content_fingerprint_hex(expected);
        const std::filesystem::path actual_path = plan.build_root / relative_path;

        BuildDiffStatus status = BuildDiffStatus::Unchanged;
        BuildDiffReason reason = BuildDiffReason::None;
        if (!std::filesystem::exists(actual_path)) {
            status = BuildDiffStatus::Create;
            reason = classify_create_reason(report.origin.detected,
                                           previously_managed,
                                           baseline_hash,
                                           expected_hash);
        } else if (!std::filesystem::is_regular_file(actual_path)) {
            status = BuildDiffStatus::Change;
            reason = BuildDiffReason::TypeMismatch;
        } else {
            const std::string actual = read_file(actual_path);
            if (actual != expected) {
                status = BuildDiffStatus::Change;
                reason = classify_change_reason(report.origin.detected,
                                                baseline_hash,
                                                content_fingerprint_hex(actual),
                                                expected_hash);
            }
        }

        report.entries.push_back({
            .relative_path = relative_path,
            .status = status,
            .reason = reason,
            .reapply_action = classify_reapply_action(status, reason),
        });
    }

    for (const auto& relative_key : previous_managed_files) {
        if (current_managed_files.contains(relative_key)) {
            continue;
        }
        const std::filesystem::path actual_path = plan.build_root / relative_key;
        if (!std::filesystem::exists(actual_path)) {
            continue;
        }
        report.entries.push_back({
            .relative_path = relative_key,
            .status = BuildDiffStatus::Delete,
            .reason = classify_delete_reason(report.origin.detected,
                                             find_managed_file_hash(previous_lock, relative_key),
                                             actual_path),
            .reapply_action = classify_reapply_action(BuildDiffStatus::Delete,
                                                      classify_delete_reason(report.origin.detected,
                                                                             find_managed_file_hash(previous_lock, relative_key),
                                                                             actual_path)),
        });
    }

    std::sort(report.entries.begin(), report.entries.end(), [](const BuildDiffEntry& left, const BuildDiffEntry& right) {
        return left.relative_path.generic_string() < right.relative_path.generic_string();
    });

    report.update = build_update_info(report);
    report.update_policy = build_update_policy(report.update);
    report.reapply = build_reapply_summary(report);

    return report;
}

std::string format_build_diff_text(const BuildDiffReport& report) {
    std::ostringstream stream;
    stream << "Diff " << report.build_root.string() << '\n';
    stream << "Template: " << template_label(report.template_id, report.template_version) << '\n';
    if (report.origin.detected) {
        stream << "Origin lock: " << report.origin.lockfile_path.string() << '\n';
        stream << "Origin template: " << template_label(report.origin.template_id, report.origin.template_version) << '\n';
        stream << "Origin generated_at: " << (report.origin.generated_at.empty() ? std::string{"<unknown>"} : report.origin.generated_at) << '\n';
        stream << "Origin matches requested template: " << (report.origin.matches_requested_template ? "yes" : "no") << '\n';
        stream << "Origin matches requested version: " << (report.origin.matches_requested_version ? "yes" : "no") << '\n';
    } else {
        stream << "Origin lock: <none>\n";
    }
    stream << "Update kind: " << update_kind_name(report.update.kind) << '\n';
    stream << "Update policy: " << update_policy_action_name(report.update_policy.action) << '\n';
    stream << "Update reason: " << report.update_policy.reason << '\n';
    if (report.origin.detected && report.origin.matches_requested_template) {
        stream << "Update version: " << version_label(report.update.from_version)
               << " -> " << version_label(report.update.to_version) << '\n';
    }
    stream << "Update recommendation: " << update_recommendation(report) << '\n';

    const auto counts = status_counts(report);
    stream << "Unchanged: " << counts[0] << '\n';
    stream << "Create: " << counts[1] << '\n';
    stream << "Change: " << counts[2] << '\n';
    stream << "Delete: " << counts[3] << '\n';
    stream << "Reapply status: " << reapply_status_name(report.reapply.status) << '\n';
    stream << "Reapply create: " << report.reapply.create_count << '\n';
    stream << "Reapply update: " << report.reapply.update_count << '\n';
    stream << "Reapply delete: " << report.reapply.delete_count << '\n';
    stream << "Reapply keep: " << report.reapply.keep_count << '\n';
    stream << "Reapply conflict: " << report.reapply.conflict_count << '\n';
    stream << "Reapply review: " << report.reapply.review_count << '\n';
    for (const auto& entry : report.entries) {
        stream << "  " << status_name(entry.status) << "  " << entry.relative_path.generic_string();
        if (entry.reason != BuildDiffReason::None) {
            stream << " [" << reason_name(entry.reason) << ']';
        }
        if (entry.reapply_action != BuildReapplyAction::None) {
            stream << " -> " << reapply_action_name(entry.reapply_action);
        }
        stream << '\n';
    }
    return stream.str();
}

std::string format_build_diff_json(const BuildDiffReport& report) {
    const auto counts = status_counts(report);
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"build_root\": \"" << json_escape(report.build_root.string()) << "\",\n";
    stream << "  \"template\": {\n";
    stream << "    \"id\": \"" << json_escape(report.template_id) << "\",\n";
    stream << "    \"version\": \"" << json_escape(report.template_version) << "\"\n";
    stream << "  },\n";
    stream << "  \"origin\": {\n";
    stream << "    \"detected\": " << (report.origin.detected ? "true" : "false") << ",\n";
    stream << "    \"matches_requested_template\": " << (report.origin.matches_requested_template ? "true" : "false") << ",\n";
    stream << "    \"matches_requested_version\": " << (report.origin.matches_requested_version ? "true" : "false") << ",\n";
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
    stream << "  \"counts\": {\n";
    stream << "    \"unchanged\": " << counts[0] << ",\n";
    stream << "    \"create\": " << counts[1] << ",\n";
    stream << "    \"change\": " << counts[2] << ",\n";
    stream << "    \"delete\": " << counts[3] << "\n";
    stream << "  },\n";
    stream << "  \"entries\": [\n";
    for (std::size_t index = 0; index < report.entries.size(); ++index) {
        const auto& entry = report.entries[index];
        stream << "    {\"path\": \"" << json_escape(entry.relative_path.generic_string())
               << "\", \"status\": \"" << status_name(entry.status) << "\", \"reason\": ";
        if (entry.reason == BuildDiffReason::None) {
            stream << "null";
        } else {
            stream << "\"" << reason_name(entry.reason) << "\"";
        }
        stream << ", \"reapply_action\": ";
        if (entry.reapply_action == BuildReapplyAction::None) {
            stream << "null";
        } else {
            stream << "\"" << reapply_action_name(entry.reapply_action) << "\"";
        }
        stream << '}';
        if (index + 1 < report.entries.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n";
    stream << "}\n";
    return stream.str();
}

}
