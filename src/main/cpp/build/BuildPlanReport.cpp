#include "tempify/build/BuildPlanReport.h"

#include "tempify/build/GenerationLock.h"
#include "tempify/support/Version.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>

namespace tempify {

namespace {

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

std::string existing_path_behavior_name(const ExistingPathBehavior behavior) {
    switch (behavior) {
    case ExistingPathBehavior::Error:
        return "error";
    case ExistingPathBehavior::Overwrite:
        return "overwrite";
    case ExistingPathBehavior::Skip:
        return "skip";
    }
    return "error";
}

std::string hook_acceptance_name(const HookAcceptance acceptance) {
    switch (acceptance) {
    case HookAcceptance::Yes:
        return "yes";
    case HookAcceptance::Ask:
        return "ask";
    case HookAcceptance::No:
        return "no";
    }
    return "yes";
}

std::string current_timestamp_utc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::set<std::string> sensitive_question_keys(const TemplateManifest &manifest) {
    std::set<std::string> keys;
    for (const auto &question : manifest.questions) {
        if (!question.sensitive) {
            continue;
        }
        keys.insert(question.key);
        for (const auto &alias : question.aliases) {
            keys.insert(alias);
        }
    }
    return keys;
}

void append_hook(std::vector<std::string> &hooks, const std::optional<std::filesystem::path> &path,
                 const std::string &name) {
    if (path.has_value()) {
        hooks.push_back(name);
    }
}

} // namespace

BuildPlanReport build_plan_report(const BuildPlan &plan, const TemplateManifest &manifest) {
    BuildPlanReport report;
    report.build_root = plan.build_root;
    report.existing_path_behavior = plan.existing_path_behavior;
    report.directories = plan.directories;
    report.files = plan.files;
    append_hook(report.hooks, manifest.pre_hook_path, "pre");
    append_hook(report.hooks, manifest.before_render_hook_path, "before_render");
    append_hook(report.hooks, manifest.after_render_hook_path, "after_render");
    append_hook(report.hooks, manifest.post_hook_path, "post");
    return report;
}

std::string format_build_plan_text(const BuildPlanReport &report) {
    std::ostringstream stream;
    stream << "Build root: " << report.build_root.string() << '\n';
    stream << "Existing path behavior: " << existing_path_behavior_name(report.existing_path_behavior) << '\n';
    stream << "Directories: " << report.directories.size() << '\n';
    for (const auto &directory : report.directories) {
        stream << "  dir  " << directory.string() << '\n';
    }
    stream << "Files: " << report.files.size() << '\n';
    for (const auto &file : report.files) {
        stream << "  file " << file.output_path.string();
        if (file.render_with_prebyte) {
            stream << " [render]";
        }
        stream << '\n';
    }
    stream << "Hooks: " << report.hooks.size() << '\n';
    for (const auto &hook : report.hooks) {
        stream << "  hook " << hook << '\n';
    }
    return stream.str();
}

std::string format_build_plan_json(const BuildPlanReport &report) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"build_root\": \"" << json_escape(report.build_root.string()) << "\",\n";
    stream << "  \"existing_path_behavior\": \"" << existing_path_behavior_name(report.existing_path_behavior)
           << "\",\n";
    stream << "  \"directories\": [\n";
    for (std::size_t index = 0; index < report.directories.size(); ++index) {
        stream << "    \"" << json_escape(report.directories[index].string()) << "\"";
        if (index + 1 < report.directories.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"files\": [\n";
    for (std::size_t index = 0; index < report.files.size(); ++index) {
        const auto &file = report.files[index];
        stream << "    {\"source\": \"" << json_escape(file.source_path.string()) << "\", \"output\": \""
               << json_escape(file.output_path.string())
               << "\", \"render\": " << (file.render_with_prebyte ? "true" : "false") << '}';
        if (index + 1 < report.files.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"hooks\": [\n";
    for (std::size_t index = 0; index < report.hooks.size(); ++index) {
        stream << "    \"" << json_escape(report.hooks[index]) << "\"";
        if (index + 1 < report.hooks.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n";
    stream << "}\n";
    return stream.str();
}

std::string format_generation_lock_json(const TemplateManifest &manifest, const BuildPlan &plan,
                                        const std::map<std::string, std::string> &values,
                                        const HookAcceptance hook_acceptance, const bool hooks_disabled) {
    return format_generation_lock_json(manifest, plan, values, hook_acceptance, hooks_disabled,
                                       build_generation_lock_managed_file_hashes(plan));
}

std::string format_generation_lock_json(const TemplateManifest &manifest, const BuildPlan &plan,
                                        const std::map<std::string, std::string> &values,
                                        const HookAcceptance hook_acceptance, const bool hooks_disabled,
                                        const std::map<std::string, std::string> &managed_file_hashes) {
    const std::set<std::string> redacted_keys = sensitive_question_keys(manifest);
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"tempify_version\": \"" << json_escape(std::string(VERSION)) << "\",\n";
    stream << "  \"template\": {\n";
    stream << "    \"id\": \"" << json_escape(manifest.info.id) << "\",\n";
    stream << "    \"name\": \"" << json_escape(manifest.info.name) << "\",\n";
    stream << "    \"version\": \"" << json_escape(manifest.info.version) << "\",\n";
    stream << "    \"root\": \"" << json_escape(manifest.root.string()) << "\"\n";
    stream << "  },\n";
    stream << "  \"build_root\": \"" << json_escape(plan.build_root.string()) << "\",\n";
    stream << "  \"generated_at\": \"" << current_timestamp_utc() << "\",\n";
    stream << "  \"existing_path_behavior\": \"" << existing_path_behavior_name(plan.existing_path_behavior) << "\",\n";
    stream << "  \"hook_acceptance\": \"" << hook_acceptance_name(hook_acceptance) << "\",\n";
    stream << "  \"hooks_disabled\": " << (hooks_disabled ? "true" : "false") << ",\n";
    stream << "  \"managed_files\": [\n";
    for (std::size_t index = 0; index < plan.files.size(); ++index) {
        const auto relative_path =
            std::filesystem::relative(plan.files[index].output_path, plan.build_root).generic_string();
        stream << "    \"" << json_escape(relative_path) << "\"";
        if (index + 1 < plan.files.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"managed_file_hashes\": {\n";
    std::size_t hash_index = 0;
    for (const auto &[relative_path, hash] : managed_file_hashes) {
        stream << "    \"" << json_escape(relative_path) << "\": \"" << json_escape(hash) << "\"";
        if (hash_index + 1 < managed_file_hashes.size()) {
            stream << ',';
        }
        stream << '\n';
        ++hash_index;
    }
    stream << "  },\n";
    stream << "  \"values\": {\n";
    std::size_t index = 0;
    for (const auto &[key, value] : values) {
        const std::string lock_value = redacted_keys.contains(key) ? std::string{"<redacted>"} : value;
        stream << "    \"" << json_escape(key) << "\": \"" << json_escape(lock_value) << "\"";
        if (index + 1 < values.size()) {
            stream << ',';
        }
        stream << '\n';
        ++index;
    }
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

} // namespace tempify
