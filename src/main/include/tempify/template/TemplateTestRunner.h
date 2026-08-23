#pragma once

#include "tempify/domain/TemplateManifest.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

class LuaEngine;
class PrebyteRenderer;

struct TemplateFixtureResult {
    std::string name;
    std::size_t snapshot_file_count = 0;
    bool includes_lockfile_snapshot = false;
    std::size_t elapsed_ms = 0;
    std::optional<std::string> failure_code;
    std::optional<std::string> failure_kind;
    std::optional<std::string> failure_message;
};

struct TemplateTestReport {
    std::string template_id;
    std::vector<TemplateFixtureResult> fixtures;
    std::size_t total_snapshot_artifact_count = 0;
    std::size_t elapsed_ms = 0;
};

struct TemplateFixtureListing {
    std::string name;
    bool has_answers_file = false;
    bool has_lockfile_snapshot = false;
    std::string snapshot_root;
    std::optional<std::string> answers_file;
    std::optional<std::string> lockfile_snapshot;
};

class TemplateTestRunner {
  public:
    TemplateTestRunner(const LuaEngine &lua_engine, const PrebyteRenderer &renderer);

    TemplateTestReport run(const TemplateManifest &manifest,
                           const std::optional<std::string> &fixture_name = std::nullopt) const;
    TemplateTestReport update_snapshots(const TemplateManifest &manifest,
                                        const std::optional<std::string> &fixture_name = std::nullopt) const;
    std::vector<std::string> list_fixture_names(const TemplateManifest &manifest,
                                                const std::optional<std::string> &fixture_name = std::nullopt) const;
    std::vector<TemplateFixtureListing>
    list_fixtures(const TemplateManifest &manifest,
                  const std::optional<std::string> &fixture_name = std::nullopt) const;

  private:
    const LuaEngine &lua_engine_;
    const PrebyteRenderer &renderer_;
};

std::string canonicalize_template_test_lockfile_json(std::string text);
std::string format_template_test_report(const TemplateTestReport &report);
std::string format_template_test_report_json(const TemplateTestReport &report);
std::string format_template_fixture_listing_json(const std::string &template_id,
                                                 const std::vector<TemplateFixtureListing> &fixtures);

} // namespace tempify
