#include "tempify/template/TemplateTestRunner.h"

#include "tempify/build/BuildExecutor.h"
#include "tempify/build/BuildPlanner.h"
#include "tempify/build/BuildPlanReport.h"
#include "tempify/frontend/IQuestionFrontend.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/question/AnswerFile.h"
#include "tempify/question/QuestionProcessor.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace tempify {

std::string canonicalize_template_test_lockfile_json(std::string text) {
    const auto replace_json_string_field = [](std::string input,
                                              const std::string& key,
                                              const std::string& replacement) {
        const std::string token = "\"" + key + "\": \"";
        std::size_t start = 0;
        while ((start = input.find(token, start)) != std::string::npos) {
            const std::size_t value_start = start + token.size();
            const std::size_t value_end = input.find('"', value_start);
            if (value_end == std::string::npos) {
                break;
            }
            input.replace(value_start, value_end - value_start, replacement);
            start = value_start + replacement.size();
        }
        return input;
    };

    text = replace_json_string_field(std::move(text), "root", "<template-root>");
    text = replace_json_string_field(std::move(text), "build_root", "<build-root>");
    text = replace_json_string_field(std::move(text), "generated_at", "<generated-at>");
    return text;
}

namespace {

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

std::size_t snapshot_artifact_count(const TemplateFixtureResult& fixture) {
    return fixture.snapshot_file_count + (fixture.includes_lockfile_snapshot ? 1U : 0U);
}

std::size_t passed_fixture_count(const TemplateTestReport& report) {
    std::size_t count = 0;
    for (const auto& fixture : report.fixtures) {
        if (!fixture.failure_message.has_value()) {
            ++count;
        }
    }
    return count;
}

std::size_t failed_fixture_count(const TemplateTestReport& report) {
    return report.fixtures.size() - passed_fixture_count(report);
}

class ScopedDirectoryCleanup {
public:
    explicit ScopedDirectoryCleanup(std::filesystem::path path)
        : path_(std::move(path)) {}

    ~ScopedDirectoryCleanup() {
        std::filesystem::remove_all(path_);
    }

private:
    std::filesystem::path path_;
};

class SilentFrontend final : public IQuestionFrontend {
public:
    std::optional<PromptResult> prompt(const std::string& text, bool sensitive = false) override {
        static_cast<void>(text);
        static_cast<void>(sensitive);
        return std::nullopt;
    }

    void write_line(const std::string& text) override {
        static_cast<void>(text);
    }

    void begin_group(const std::string& name, std::size_t index, std::size_t total) override {
        static_cast<void>(name);
        static_cast<void>(index);
        static_cast<void>(total);
    }

    void end_group() override {}
};

struct FixtureDefinition {
    std::string name;
    std::filesystem::path answers_path;
    std::filesystem::path snapshot_root;
    std::filesystem::path lockfile_snapshot_path;
};

struct SnapshotComparisonResult {
    std::size_t file_count = 0;
    bool includes_lockfile_snapshot = false;
};

struct FixtureFailure {
    std::string code;
    std::string kind;
    std::string message;
};

std::filesystem::path create_temp_directory(const std::string& prefix) {
    std::error_code error;
    const std::filesystem::path temp_root = std::filesystem::temp_directory_path(error);
    if (error) {
        throw TempifyError("Could not resolve temp directory for fixture '" + prefix + "': " + error.message());
    }

    const std::uint64_t seed = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    for (std::uint64_t attempt = 0; attempt < 128; ++attempt) {
        const std::filesystem::path candidate = temp_root / (prefix + "-" + std::to_string(seed + attempt));
        error.clear();
        if (std::filesystem::create_directory(candidate, error)) {
            return candidate;
        }
        if (error && error != std::errc::file_exists) {
            throw TempifyError("Could not create temp directory for fixture '" + prefix + "': " + error.message());
        }
    }

    throw TempifyError("Could not create temp directory for fixture '" + prefix + "'.");
}

std::vector<FixtureDefinition> discover_fixtures(const TemplateManifest& manifest,
                                                 const std::optional<std::string>& selected_fixture_name) {
    const std::filesystem::path tests_root = manifest.root / "tests";
    if (!std::filesystem::is_directory(tests_root)) {
        throw TempifyError("No test fixtures found under " + tests_root.string());
    }

    std::vector<std::filesystem::path> fixture_roots;
    for (const auto& entry : std::filesystem::directory_iterator(tests_root)) {
        if (entry.is_directory()) {
            fixture_roots.push_back(entry.path());
        }
    }
    std::ranges::sort(fixture_roots);

    std::vector<FixtureDefinition> fixtures;
    fixtures.reserve(fixture_roots.size());
    for (const auto& root : fixture_roots) {
        FixtureDefinition fixture{
            .name = root.filename().string(),
            .answers_path = root / "answers.json",
            .snapshot_root = root / "snapshot",
            .lockfile_snapshot_path = root / "lock.json",
        };
        if (!std::filesystem::is_directory(fixture.snapshot_root)) {
            throw TempifyError("Fixture '" + fixture.name + "' missing snapshot directory: " + fixture.snapshot_root.string());
        }
        if (std::filesystem::exists(fixture.answers_path) && !std::filesystem::is_regular_file(fixture.answers_path)) {
            throw TempifyError("Fixture '" + fixture.name + "' answers path is not file: " + fixture.answers_path.string());
        }
        if (std::filesystem::exists(fixture.lockfile_snapshot_path)
            && !std::filesystem::is_regular_file(fixture.lockfile_snapshot_path)) {
            throw TempifyError("Fixture '" + fixture.name + "' lock snapshot path is not file: "
                               + fixture.lockfile_snapshot_path.string());
        }
        fixtures.push_back(std::move(fixture));
    }

    if (fixtures.empty()) {
        throw TempifyError("No test fixtures found under " + tests_root.string());
    }

    if (selected_fixture_name.has_value()) {
        std::vector<FixtureDefinition> filtered;
        for (const auto& fixture : fixtures) {
            if (fixture.name == *selected_fixture_name) {
                filtered.push_back(fixture);
            }
        }
        if (filtered.empty()) {
            throw TempifyError("Fixture not found for template '" + manifest.info.id + "': " + *selected_fixture_name);
        }
        return filtered;
    }

    return fixtures;
}

std::vector<std::string> collect_relative_files(const std::filesystem::path& root) {
    std::vector<std::string> files;
    if (!std::filesystem::is_directory(root)) {
        return files;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        files.push_back(std::filesystem::relative(entry.path(), root).generic_string());
    }
    std::ranges::sort(files);
    return files;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw TempifyError("Could not read file: " + path.string());
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw TempifyError("Could not write file: " + path.string());
    }
    output << content;
}

void remove_file_if_exists(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

std::vector<std::string> split_lines_preserve(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            std::string line = text.substr(start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(std::move(line));
            return lines;
        }

        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
        start = end + 1;
    }

    if (text.empty() || text.ends_with('\n')) {
        lines.emplace_back();
    }
    return lines;
}

std::string preview_line(const std::string& line) {
    std::string preview;
    preview.reserve(line.size());
    for (const char ch : line) {
        switch (ch) {
        case '\t':
            preview += "\\t";
            break;
        case '\r':
            preview += "\\r";
            break;
        default:
            preview.push_back(ch);
            break;
        }
    }

    constexpr std::size_t max_length = 80;
    if (preview.size() > max_length) {
        preview.resize(max_length);
        preview += "...";
    }
    if (preview.empty()) {
        return "<empty>";
    }
    return preview;
}

std::string describe_content_mismatch(const std::string& relative_path,
                                      const std::string& expected,
                                      const std::string& actual) {
    const auto expected_lines = split_lines_preserve(expected);
    const auto actual_lines = split_lines_preserve(actual);
    const std::size_t line_count = std::max(expected_lines.size(), actual_lines.size());
    for (std::size_t index = 0; index < line_count; ++index) {
        const std::string expected_line = index < expected_lines.size() ? expected_lines[index] : "<eof>";
        const std::string actual_line = index < actual_lines.size() ? actual_lines[index] : "<eof>";
        if (expected_line != actual_line) {
            std::ostringstream stream;
            stream << "content mismatch in " << relative_path
                   << " at line " << (index + 1)
                   << ": expected `" << preview_line(expected_line)
                   << "`, got `" << preview_line(actual_line) << '`';
            return stream.str();
        }
    }

    return "content mismatch in " + relative_path;
}

FixtureFailure classify_failure(const std::string& message) {
    if (message.find("snapshot mismatch") != std::string::npos) {
        return {.code = "SNAPSHOT_MISMATCH", .kind = "snapshot_mismatch", .message = message};
    }
    if (message.find("lockfile mismatch") != std::string::npos) {
        return {.code = "LOCKFILE_MISMATCH", .kind = "lockfile_mismatch", .message = message};
    }
    if (message.find("Fixture not found") != std::string::npos) {
        return {.code = "FIXTURE_NOT_FOUND", .kind = "fixture_not_found", .message = message};
    }
    if (message.find("missing snapshot directory") != std::string::npos) {
        return {.code = "FIXTURE_INVALID", .kind = "fixture_invalid", .message = message};
    }
    if (message.find("missing [") != std::string::npos || message.find("unexpected [") != std::string::npos) {
        return {.code = "SNAPSHOT_FILE_SET_MISMATCH", .kind = "snapshot_file_set_mismatch", .message = message};
    }
    return {.code = "TEST_ERROR", .kind = "test_error", .message = message};
}

std::string join_paths(const std::vector<std::string>& paths) {
    std::ostringstream stream;
    constexpr std::size_t max_paths = 5;
    for (std::size_t index = 0; index < paths.size() && index < max_paths; ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << paths[index];
    }
    if (paths.size() > max_paths) {
        stream << ", ...";
    }
    return stream.str();
}

void compare_expected_file_set(const std::vector<std::string>& expected_files,
                               const std::vector<std::string>& actual_files,
                               const FixtureDefinition& fixture) {
    std::vector<std::string> missing_files;
    std::vector<std::string> unexpected_files;
    std::set_difference(expected_files.begin(),
                        expected_files.end(),
                        actual_files.begin(),
                        actual_files.end(),
                        std::back_inserter(missing_files));
    std::set_difference(actual_files.begin(),
                        actual_files.end(),
                        expected_files.begin(),
                        expected_files.end(),
                        std::back_inserter(unexpected_files));

    if (!missing_files.empty() || !unexpected_files.empty()) {
        std::ostringstream stream;
        stream << "Fixture '" << fixture.name << "' snapshot mismatch:";
        if (!missing_files.empty()) {
            stream << " missing [" << join_paths(missing_files) << ']';
        }
        if (!unexpected_files.empty()) {
            stream << " unexpected [" << join_paths(unexpected_files) << ']';
        }
        throw TempifyError(stream.str());
    }
}

bool compare_lockfile_snapshot(const FixtureDefinition& fixture,
                               const TemplateManifest& manifest,
                               const BuildPlan& plan,
                               const std::map<std::string, std::string>& values) {
    if (!std::filesystem::is_regular_file(fixture.lockfile_snapshot_path)) {
        return false;
    }

    const std::string expected = canonicalize_template_test_lockfile_json(read_file(fixture.lockfile_snapshot_path));
    const std::string actual = canonicalize_template_test_lockfile_json(format_generation_lock_json(manifest,
                                                                                                     plan,
                                                                                                     values,
                                                                                                     HookAcceptance::Yes,
                                                                                                     false));
    if (expected != actual) {
        throw TempifyError("Fixture '" + fixture.name + "' lockfile mismatch: "
                           + describe_content_mismatch("lock.json", expected, actual));
    }
    return true;
}

void write_fixture_snapshot(const FixtureDefinition& fixture,
                           const TemplateManifest& manifest,
                           const BuildPlan& plan,
                           const std::map<std::string, std::string>& values) {
    std::filesystem::remove_all(fixture.snapshot_root);
    std::filesystem::create_directories(fixture.snapshot_root);

    for (const auto& relative_path : collect_relative_files(plan.build_root)) {
        write_file(fixture.snapshot_root / relative_path, read_file(plan.build_root / relative_path));
    }

    if (std::filesystem::is_regular_file(fixture.lockfile_snapshot_path)) {
        write_file(fixture.lockfile_snapshot_path,
                   canonicalize_template_test_lockfile_json(format_generation_lock_json(manifest,
                                                                                        plan,
                                                                                        values,
                                                                                        HookAcceptance::Yes,
                                                                                        false)));
    }
}

SnapshotComparisonResult compare_snapshot_tree(const FixtureDefinition& fixture,
                                               const TemplateManifest& manifest,
                                               const BuildPlan& plan,
                                               const std::map<std::string, std::string>& values) {
    const auto expected_files = collect_relative_files(fixture.snapshot_root);
    const auto actual_files = collect_relative_files(plan.build_root);
    compare_expected_file_set(expected_files, actual_files, fixture);

    for (const auto& relative_path : expected_files) {
        const std::string expected = read_file(fixture.snapshot_root / relative_path);
        const std::string actual = read_file(plan.build_root / relative_path);
        if (expected != actual) {
            throw TempifyError("Fixture '" + fixture.name + "' snapshot mismatch: "
                               + describe_content_mismatch(relative_path, expected, actual));
        }
    }

    return {
        .file_count = expected_files.size(),
        .includes_lockfile_snapshot = compare_lockfile_snapshot(fixture, manifest, plan, values),
    };
}

std::map<std::string, std::string> load_fixture_answers(const FixtureDefinition& fixture) {
    if (!std::filesystem::exists(fixture.answers_path)) {
        return {};
    }
    return load_answer_file(fixture.answers_path, true);
}

}

TemplateTestRunner::TemplateTestRunner(const LuaEngine& lua_engine, const PrebyteRenderer& renderer)
    : lua_engine_(lua_engine),
      renderer_(renderer) {}

std::vector<TemplateFixtureListing> TemplateTestRunner::list_fixtures(const TemplateManifest& manifest,
                                                                      const std::optional<std::string>& fixture_name) const {
    std::vector<TemplateFixtureListing> fixtures;
    for (const auto& fixture : discover_fixtures(manifest, fixture_name)) {
        fixtures.push_back({
            .name = fixture.name,
            .has_answers_file = std::filesystem::is_regular_file(fixture.answers_path),
            .has_lockfile_snapshot = std::filesystem::is_regular_file(fixture.lockfile_snapshot_path),
            .snapshot_root = fixture.snapshot_root.string(),
            .answers_file = std::filesystem::is_regular_file(fixture.answers_path)
                ? std::optional<std::string>(fixture.answers_path.string())
                : std::nullopt,
            .lockfile_snapshot = std::filesystem::is_regular_file(fixture.lockfile_snapshot_path)
                ? std::optional<std::string>(fixture.lockfile_snapshot_path.string())
                : std::nullopt,
        });
    }
    return fixtures;
}

std::vector<std::string> TemplateTestRunner::list_fixture_names(const TemplateManifest& manifest,
                                                                const std::optional<std::string>& fixture_name) const {
    std::vector<std::string> names;
    for (const auto& fixture : discover_fixtures(manifest, fixture_name)) {
        names.push_back(fixture.name);
    }
    return names;
}

TemplateTestReport TemplateTestRunner::run(const TemplateManifest& manifest,
                                           const std::optional<std::string>& fixture_name) const {
    TemplateTestReport report{.template_id = manifest.info.id};
    const auto all_fixtures = discover_fixtures(manifest, std::nullopt);
    std::vector<FixtureDefinition> fixtures;
    if (fixture_name.has_value()) {
        for (const auto& fixture : all_fixtures) {
            if (fixture.name == *fixture_name) {
                fixtures.push_back(fixture);
            }
        }
        if (fixtures.empty()) {
            report.fixtures.push_back({
                .name = *fixture_name,
                .failure_message = "Fixture not found for template '" + manifest.info.id + "': " + *fixture_name,
            });
            return report;
        }
    } else {
        fixtures = all_fixtures;
    }

    report.fixtures.reserve(fixtures.size());

    SilentFrontend frontend;
    QuestionProcessor question_processor(lua_engine_, frontend);
    const BuildPlanner planner(renderer_);
    const BuildExecutor executor(renderer_, lua_engine_);
    const auto start_time = std::chrono::steady_clock::now();

    for (const auto& fixture : fixtures) {
        TemplateFixtureResult result{.name = fixture.name};
        const auto fixture_start = std::chrono::steady_clock::now();
        try {
            const auto values = question_processor.collect(
                manifest,
                {},
                {},
                load_fixture_answers(fixture),
                true,
                true);

            const std::filesystem::path scratch_root = create_temp_directory("tempify-test-" + fixture.name);
            const ScopedDirectoryCleanup cleanup(scratch_root);
            BuildPlan plan = planner.plan(manifest, values, scratch_root / "output");
            executor.execute(plan, manifest, values, false);
            const SnapshotComparisonResult comparison = compare_snapshot_tree(fixture, manifest, plan, values);

            result.snapshot_file_count = comparison.file_count;
            result.includes_lockfile_snapshot = comparison.includes_lockfile_snapshot;
            report.total_snapshot_artifact_count += comparison.file_count + (comparison.includes_lockfile_snapshot ? 1U : 0U);
        } catch (const TempifyError& error) {
            const FixtureFailure failure = classify_failure(error.what());
            result.failure_code = failure.code;
            result.failure_kind = failure.kind;
            result.failure_message = failure.message;
        }
        result.elapsed_ms = static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - fixture_start).count());

        report.fixtures.push_back(std::move(result));
    }

    report.elapsed_ms = static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count());
    return report;
}

TemplateTestReport TemplateTestRunner::update_snapshots(const TemplateManifest& manifest,
                                                        const std::optional<std::string>& fixture_name) const {
    TemplateTestReport report{.template_id = manifest.info.id};
    const auto fixtures = discover_fixtures(manifest, fixture_name);
    report.fixtures.reserve(fixtures.size());

    SilentFrontend frontend;
    QuestionProcessor question_processor(lua_engine_, frontend);
    const BuildPlanner planner(renderer_);
    const BuildExecutor executor(renderer_, lua_engine_);
    const auto start_time = std::chrono::steady_clock::now();

    for (const auto& fixture : fixtures) {
        TemplateFixtureResult result{.name = fixture.name};
        const auto fixture_start = std::chrono::steady_clock::now();
        try {
            const auto values = question_processor.collect(
                manifest,
                {},
                {},
                load_fixture_answers(fixture),
                true,
                true);

            const std::filesystem::path scratch_root = create_temp_directory("tempify-test-update-" + fixture.name);
            const ScopedDirectoryCleanup cleanup(scratch_root);
            BuildPlan plan = planner.plan(manifest, values, scratch_root / "output");
            executor.execute(plan, manifest, values, false);
            write_fixture_snapshot(fixture, manifest, plan, values);

            result.snapshot_file_count = collect_relative_files(fixture.snapshot_root).size();
            result.includes_lockfile_snapshot = std::filesystem::is_regular_file(fixture.lockfile_snapshot_path);
            report.total_snapshot_artifact_count += result.snapshot_file_count + (result.includes_lockfile_snapshot ? 1U : 0U);
        } catch (const TempifyError& error) {
            const FixtureFailure failure = classify_failure(error.what());
            result.failure_code = failure.code;
            result.failure_kind = failure.kind;
            result.failure_message = failure.message;
        }
        result.elapsed_ms = static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - fixture_start).count());
        report.fixtures.push_back(std::move(result));
    }

    report.elapsed_ms = static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count());
    return report;
}

std::string format_template_test_report(const TemplateTestReport& report) {
    std::ostringstream stream;
    stream << "Test " << report.template_id << '\n';
    const std::size_t passed = passed_fixture_count(report);
    const std::size_t failed = failed_fixture_count(report);
    for (const auto& fixture : report.fixtures) {
        if (fixture.failure_message.has_value()) {
            stream << "FAIL " << fixture.name << ": " << *fixture.failure_message << '\n';
            continue;
        }

        stream << "PASS " << fixture.name << " (" << fixture.snapshot_file_count << " files";
        if (fixture.includes_lockfile_snapshot) {
            stream << ", lock";
        }
        stream << ")\n";
    }
    if (failed == 0) {
        stream << passed << '/' << report.fixtures.size() << " fixtures passed";
    } else {
        stream << passed << '/' << report.fixtures.size() << " fixtures passed, " << failed << " failed";
    }
    if (!report.fixtures.empty()) {
        stream << " (" << report.total_snapshot_artifact_count << " snapshot artifacts, "
               << report.elapsed_ms << " ms)";
    }
    stream << '\n';
    return stream.str();
}

std::string format_template_test_report_json(const TemplateTestReport& report) {
    std::ostringstream stream;
    const std::size_t passed = passed_fixture_count(report);
    const std::size_t failed = failed_fixture_count(report);

    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(report.template_id) << "\",\n";
    stream << "  \"passed\": " << passed << ",\n";
    stream << "  \"failed\": " << failed << ",\n";
    stream << "  \"total\": " << report.fixtures.size() << ",\n";
    stream << "  \"snapshot_artifacts\": " << report.total_snapshot_artifact_count << ",\n";
    stream << "  \"elapsed_ms\": " << report.elapsed_ms << ",\n";
    stream << "  \"fixtures\": [\n";
    for (std::size_t index = 0; index < report.fixtures.size(); ++index) {
        const auto& fixture = report.fixtures[index];
        stream << "    {\n";
        stream << "      \"name\": \"" << json_escape(fixture.name) << "\",\n";
        stream << "      \"status\": \"" << (fixture.failure_message.has_value() ? "failed" : "passed") << "\",\n";
        stream << "      \"snapshot_files\": " << fixture.snapshot_file_count << ",\n";
        stream << "      \"snapshot_artifacts\": " << snapshot_artifact_count(fixture) << ",\n";
        stream << "      \"lockfile_snapshot\": " << (fixture.includes_lockfile_snapshot ? "true" : "false") << ",\n";
        stream << "      \"elapsed_ms\": " << fixture.elapsed_ms;
        if (fixture.failure_message.has_value()) {
            stream << ",\n";
            stream << "      \"code\": \"" << json_escape(fixture.failure_code.value_or("TEST_ERROR")) << "\",\n";
            stream << "      \"kind\": \"" << json_escape(fixture.failure_kind.value_or("test_error")) << "\",\n";
            stream << "      \"message\": \"" << json_escape(*fixture.failure_message) << "\"\n";
        } else {
            stream << '\n';
        }
        stream << "    }";
        if (index + 1 < report.fixtures.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n";
    stream << "}\n";
    return stream.str();
}

std::string format_template_fixture_listing_json(const std::string& template_id,
                                                 const std::vector<TemplateFixtureListing>& fixtures) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(template_id) << "\",\n";
    stream << "  \"total\": " << fixtures.size() << ",\n";
    stream << "  \"fixtures\": [\n";
    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        const auto& fixture = fixtures[index];
        stream << "    {\n";
        stream << "      \"name\": \"" << json_escape(fixture.name) << "\",\n";
        stream << "      \"has_answers_file\": " << (fixture.has_answers_file ? "true" : "false") << ",\n";
        stream << "      \"has_lockfile_snapshot\": " << (fixture.has_lockfile_snapshot ? "true" : "false") << ",\n";
        stream << "      \"snapshot_root\": \"" << json_escape(fixture.snapshot_root) << "\"";
        if (fixture.answers_file.has_value() || fixture.lockfile_snapshot.has_value()) {
            stream << ",\n";
            stream << "      \"answers_file\": ";
            if (fixture.answers_file.has_value()) {
                stream << "\"" << json_escape(*fixture.answers_file) << "\"";
            } else {
                stream << "null";
            }
            stream << ",\n";
            stream << "      \"lockfile_snapshot\": ";
            if (fixture.lockfile_snapshot.has_value()) {
                stream << "\"" << json_escape(*fixture.lockfile_snapshot) << "\"\n";
            } else {
                stream << "null\n";
            }
        } else {
            stream << '\n';
        }
        stream << "    }";
        if (index + 1 < fixtures.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n";
    stream << "}\n";
    return stream.str();
}

}
