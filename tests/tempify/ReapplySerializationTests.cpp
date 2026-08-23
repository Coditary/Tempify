#include "TestHarness.h"
#include "tempify/build/BuildDiffReport.h"
#include "tempify/build/ReapplySerialization.h"

#include <string>

namespace {

tempify::BuildDiffReport make_conflict_report() {
    tempify::BuildDiffReport report;
    report.build_root = std::filesystem::path("/tmp/reapply-target");
    report.template_id = "basic_cpp";
    report.template_version = "1.0.0";
    report.origin.detected = true;
    report.origin.matches_requested_template = true;
    report.origin.matches_requested_version = true;
    report.origin.lockfile_path = report.build_root / ".tempify-lock.json";
    report.origin.template_id = "basic_cpp";
    report.origin.template_version = "1.0.0";
    report.update.kind = tempify::BuildUpdateKind::SameTemplate;
    report.update.from_version = "1.0.0";
    report.update.to_version = "1.0.0";
    report.update_policy.action = tempify::BuildUpdatePolicyAction::Allow;
    report.reapply.status = tempify::BuildReapplyStatus::Conflict;
    report.reapply.conflict_count = 1;
    report.entries.push_back({
        .relative_path = "README.md",
        .status = tempify::BuildDiffStatus::Change,
        .reason = tempify::BuildDiffReason::LocalEdit,
        .reapply_action = tempify::BuildReapplyAction::Conflict,
    });
    return report;
}

tempify::BuildDiffReport make_success_report() {
    tempify::BuildDiffReport report;
    report.build_root = std::filesystem::path("/tmp/reapply-target");
    report.template_id = "basic_cpp";
    report.template_version = "1.0.0";
    report.origin.detected = true;
    report.origin.matches_requested_template = true;
    report.origin.template_id = "basic_cpp";
    report.origin.template_version = "1.0.0";
    report.update.kind = tempify::BuildUpdateKind::SameTemplate;
    report.update.from_version = "1.0.0";
    report.update.to_version = "1.0.0";
    report.update_policy.action = tempify::BuildUpdatePolicyAction::Allow;
    report.reapply.status = tempify::BuildReapplyStatus::Ready;
    report.reapply.delete_count = 1;
    report.entries.push_back({
        .relative_path = "old-managed.txt",
        .status = tempify::BuildDiffStatus::Delete,
        .reason = tempify::BuildDiffReason::TemplateUpdate,
        .reapply_action = tempify::BuildReapplyAction::Delete,
    });
    return report;
}

tempify::BuildDiffReport make_template_mismatch_report() {
    tempify::BuildDiffReport report;
    report.build_root = std::filesystem::path("/tmp/reapply-target");
    report.template_id = "basic_cpp";
    report.template_version = "1.0.0";
    report.origin.detected = true;
    report.origin.matches_requested_template = false;
    report.origin.matches_requested_version = false;
    report.origin.lockfile_path = report.build_root / ".tempify-lock.json";
    report.origin.template_id = "layered_cpp_product";
    report.origin.template_version = "2.0.0";
    report.update.kind = tempify::BuildUpdateKind::TemplateMismatch;
    report.update.from_version = "2.0.0";
    report.update.to_version = "1.0.0";
    report.update_policy.action = tempify::BuildUpdatePolicyAction::Review;
    report.update_policy.reason = "template_mismatch";
    report.reapply.status = tempify::BuildReapplyStatus::Review;
    report.reapply.review_count = 1;
    report.entries.push_back({
        .relative_path = ".tempify-lock.json",
        .status = tempify::BuildDiffStatus::Change,
        .reason = tempify::BuildDiffReason::Conflict,
        .reapply_action = tempify::BuildReapplyAction::Review,
    });
    return report;
}

tempify::BuildDiffReport make_version_transition_report() {
    tempify::BuildDiffReport report;
    report.build_root = std::filesystem::path("/tmp/reapply-target");
    report.template_id = "basic_cpp";
    report.template_version = "1.0.0";
    report.origin.detected = true;
    report.origin.matches_requested_template = true;
    report.origin.matches_requested_version = false;
    report.origin.lockfile_path = report.build_root / ".tempify-lock.json";
    report.origin.template_id = "basic_cpp";
    report.origin.template_version = "0.0.9";
    report.update.kind = tempify::BuildUpdateKind::Upgrade;
    report.update.from_version = "0.0.9";
    report.update.to_version = "1.0.0";
    report.update_policy.action = tempify::BuildUpdatePolicyAction::Review;
    report.update_policy.reason = "pre_1_0_minor_upgrade";
    report.reapply.status = tempify::BuildReapplyStatus::Review;
    report.reapply.review_count = 1;
    report.entries.push_back({
        .relative_path = ".tempify-lock.json",
        .status = tempify::BuildDiffStatus::Change,
        .reason = tempify::BuildDiffReason::TemplateUpdate,
        .reapply_action = tempify::BuildReapplyAction::Review,
    });
    return report;
}

} // namespace

TEST_CASE(ReapplySerialization_formats_successful_reapply_text_and_json) {
    const tempify::BuildDiffReport report = make_success_report();

    const std::string text = tempify::format_reapply_result_text("basic_cpp", report.build_root, report);
    REQUIRE(text.find("Reapplied basic_cpp") != std::string::npos);
    REQUIRE(text.find("Deleted: 1") != std::string::npos);

    const std::string json = tempify::format_reapply_result_json(report);
    REQUIRE(json.find("\"status\": \"ok\"") != std::string::npos);
    REQUIRE(json.find("\"delete\": 1") != std::string::npos);
    REQUIRE(json.find("\"old-managed.txt\"") != std::string::npos);
}

TEST_CASE(ReapplySerialization_builds_blocked_error_for_conflicts) {
    const tempify::BuildDiffReport report = make_conflict_report();
    const tempify::ReapplyBlockedError error = tempify::build_reapply_blocked_error(report);
    REQUIRE(std::string(error.what()).find("Reapply blocked") != std::string::npos);
    REQUIRE(std::string(error.what()).find("README.md") != std::string::npos);
    REQUIRE_EQ(error.conflict_paths().size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(error.conflict_paths().front(), std::string("README.md"));

    const std::string json = tempify::format_reapply_blocked_error_json(error);
    REQUIRE(json.find("\"code\": \"REAPPLY_BLOCKED\"") != std::string::npos);
    REQUIRE(json.find("\"README.md\"") != std::string::npos);
}

TEST_CASE(ReapplySerialization_builds_blocked_error_for_origin_template_mismatch) {
    const tempify::BuildDiffReport report = make_template_mismatch_report();
    const tempify::ReapplyBlockedError error = tempify::build_reapply_blocked_error(report);

    REQUIRE(std::string(error.what()).find("origin template mismatch") != std::string::npos);
    REQUIRE(std::string(error.what()).find("layered_cpp_product") != std::string::npos);
    REQUIRE(error.origin_mismatch().has_value());
    REQUIRE_EQ(error.origin_mismatch()->requested_template_id, std::string("basic_cpp"));
    REQUIRE(!error.version_transition().has_value());

    const std::string json = tempify::format_reapply_blocked_error_json(error);
    REQUIRE(json.find("\"origin_mismatch\": {") != std::string::npos);
    REQUIRE(json.find("\"version_transition\": null") != std::string::npos);
    REQUIRE(json.find("\"layered_cpp_product\"") != std::string::npos);
}

TEST_CASE(ReapplySerialization_builds_blocked_error_for_version_transition_review) {
    const tempify::BuildDiffReport report = make_version_transition_report();
    const tempify::ReapplyBlockedError error = tempify::build_reapply_blocked_error(report);

    REQUIRE(std::string(error.what()).find("pre-1.0 minor upgrade") != std::string::npos);
    REQUIRE(error.version_transition().has_value());
    REQUIRE_EQ(error.version_transition()->reason, std::string("pre_1_0_minor_upgrade"));
    REQUIRE(!error.origin_mismatch().has_value());

    const std::string json = tempify::format_reapply_blocked_error_json(error);
    REQUIRE(json.find("\"origin_mismatch\": null") != std::string::npos);
    REQUIRE(json.find("\"version_transition\": {") != std::string::npos);
    REQUIRE(json.find("\"pre_1_0_minor_upgrade\"") != std::string::npos);
}
