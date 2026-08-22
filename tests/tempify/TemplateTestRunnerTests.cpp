#include "TestHarness.h"
#include "tempify/build/BuildPlanReport.h"
#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"
#include "tempify/template/TemplateTestRunner.h"

#include <map>
#include <string>

TEST_CASE(TemplateTestRunner_format_report_includes_failures_without_aborting_summary) {
    const tempify::TemplateTestReport report{
        .template_id = "basic_cpp",
        .fixtures =
            {
                tempify::TemplateFixtureResult{
                    .name = "default_no_ci", .snapshot_file_count = 5, .includes_lockfile_snapshot = false},
                tempify::TemplateFixtureResult{.name = "broken", .failure_message = "snapshot mismatch: line 1"},
            },
        .total_snapshot_artifact_count = 5,
        .elapsed_ms = 2,
    };

    const std::string output = tempify::format_template_test_report(report);
    REQUIRE(output.find("PASS default_no_ci (5 files)") != std::string::npos);
    REQUIRE(output.find("FAIL broken: snapshot mismatch: line 1") != std::string::npos);
    REQUIRE(output.find("1/2 fixtures passed, 1 failed (5 snapshot artifacts, 2 ms)") != std::string::npos);
}

TEST_CASE(TemplateTestRunner_canonicalize_lockfile_json_replaces_volatile_fields) {
    const std::string input = "{\n"
                              "  \"root\": \"/tmp/template\",\n"
                              "  \"build_root\": \"/tmp/output\",\n"
                              "  \"generated_at\": \"2026-05-17T15:39:42Z\",\n"
                              "  \"project\": \"demo\"\n"
                              "}\n";

    const std::string output = tempify::canonicalize_template_test_lockfile_json(input);
    REQUIRE(output.find("\"root\": \"<template-root>\"") != std::string::npos);
    REQUIRE(output.find("\"build_root\": \"<build-root>\"") != std::string::npos);
    REQUIRE(output.find("\"generated_at\": \"<generated-at>\"") != std::string::npos);
    REQUIRE(output.find("\"project\": \"demo\"") != std::string::npos);
}

TEST_CASE(TemplateTestRunner_format_report_includes_lock_and_totals) {
    const tempify::TemplateTestReport report{
        .template_id = "basic_cpp",
        .fixtures =
            {
                tempify::TemplateFixtureResult{
                    .name = "default_no_ci", .snapshot_file_count = 5, .includes_lockfile_snapshot = false},
                tempify::TemplateFixtureResult{
                    .name = "hooks_and_lock", .snapshot_file_count = 7, .includes_lockfile_snapshot = true},
            },
        .total_snapshot_artifact_count = 13,
        .elapsed_ms = 4,
    };

    const std::string output = tempify::format_template_test_report(report);
    REQUIRE(output.find("Test basic_cpp") != std::string::npos);
    REQUIRE(output.find("PASS default_no_ci (5 files)") != std::string::npos);
    REQUIRE(output.find("PASS hooks_and_lock (7 files, lock)") != std::string::npos);
    REQUIRE(output.find("2/2 fixtures passed (13 snapshot artifacts, 4 ms)") != std::string::npos);
}

TEST_CASE(TemplateTestRunner_format_report_json_includes_timing_and_failure_fields) {
    const tempify::TemplateTestReport report{
        .template_id = "basic_cpp",
        .fixtures =
            {
                tempify::TemplateFixtureResult{.name = "default_no_ci",
                                               .snapshot_file_count = 5,
                                               .includes_lockfile_snapshot = false,
                                               .elapsed_ms = 1},
                tempify::TemplateFixtureResult{
                    .name = "broken", .elapsed_ms = 2, .failure_message = "snapshot mismatch"},
            },
        .total_snapshot_artifact_count = 5,
        .elapsed_ms = 3,
    };

    const std::string output = tempify::format_template_test_report_json(report);
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"passed\": 1") != std::string::npos);
    REQUIRE(output.find("\"failed\": 1") != std::string::npos);
    REQUIRE(output.find("\"elapsed_ms\": 3") != std::string::npos);
    REQUIRE(output.find("\"name\": \"default_no_ci\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"passed\"") != std::string::npos);
    REQUIRE(output.find("\"elapsed_ms\": 1") != std::string::npos);
    REQUIRE(output.find("\"name\": \"broken\"") != std::string::npos);
    REQUIRE(output.find("\"status\": \"failed\"") != std::string::npos);
    REQUIRE(output.find("\"code\": \"TEST_ERROR\"") != std::string::npos);
    REQUIRE(output.find("\"kind\": \"test_error\"") != std::string::npos);
    REQUIRE(output.find("\"message\": \"snapshot mismatch\"") != std::string::npos);
}

TEST_CASE(TemplateTestRunner_format_fixture_listing_json_includes_fixture_metadata) {
    const std::vector<tempify::TemplateFixtureListing> fixtures = {
        tempify::TemplateFixtureListing{.name = "default_no_ci",
                                        .has_answers_file = true,
                                        .has_lockfile_snapshot = false,
                                        .snapshot_root = "snapshots/default",
                                        .answers_file = "answers/default.json"},
        tempify::TemplateFixtureListing{.name = "hooks_and_lock",
                                        .has_answers_file = true,
                                        .has_lockfile_snapshot = true,
                                        .snapshot_root = "snapshots/hooks",
                                        .answers_file = "answers/hooks.json",
                                        .lockfile_snapshot = "locks/default.json"},
    };

    const std::string output = tempify::format_template_fixture_listing_json("basic_cpp", fixtures);
    REQUIRE(output.find("\"template_id\": \"basic_cpp\"") != std::string::npos);
    REQUIRE(output.find("\"total\": 2") != std::string::npos);
    REQUIRE(output.find("\"name\": \"default_no_ci\"") != std::string::npos);
    REQUIRE(output.find("\"has_answers_file\": true") != std::string::npos);
    REQUIRE(output.find("\"has_lockfile_snapshot\": true") != std::string::npos);
    REQUIRE(output.find("\"snapshot_root\": \"snapshots/default\"") != std::string::npos);
    REQUIRE(output.find("\"answers_file\": \"answers/default.json\"") != std::string::npos);
    REQUIRE(output.find("\"lockfile_snapshot\": \"locks/default.json\"") != std::string::npos);
}

TEST_CASE(BuildPlanReport_redacts_sensitive_values_in_generation_lock) {
    tempify::TemplateManifest manifest;
    manifest.info.id = "secure_tpl";
    manifest.info.name = "Secure Template";
    manifest.info.version = "1.0.0";
    manifest.root = "/tmp/template";
    manifest.questions = {
        tempify::QuestionDefinition{
            .key = "api_token",
            .aliases = {"token"},
            .sensitive = true,
        },
        tempify::QuestionDefinition{
            .key = "project_name",
        },
    };

    tempify::BuildPlan plan;
    plan.build_root = "/tmp/output";

    const std::string output = tempify::format_generation_lock_json(manifest, plan,
                                                                    {
                                                                        {"api_token", "secret-value"},
                                                                        {"project_name", "Stone App"},
                                                                        {"token", "secret-value"},
                                                                    },
                                                                    tempify::HookAcceptance::Yes, false);

    REQUIRE(output.find("\"managed_files\": [") != std::string::npos);
    REQUIRE(output.find("\"managed_file_hashes\": {") != std::string::npos);
    REQUIRE(output.find("\"api_token\": \"<redacted>\"") != std::string::npos);
    REQUIRE(output.find("\"token\": \"<redacted>\"") != std::string::npos);
    REQUIRE(output.find("secret-value") == std::string::npos);
    REQUIRE(output.find("\"project_name\": \"Stone App\"") != std::string::npos);
}
