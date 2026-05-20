#include "tempify/app/TempifyApp.h"

#include "TempifyAppInternal.h"

#include "tempify/build/BuildExecutor.h"
#include "tempify/build/BuildDiffReport.h"
#include "tempify/build/BuildPlanner.h"
#include "tempify/build/BuildPlanReport.h"
#include "tempify/build/GenerationLock.h"
#include "tempify/build/ReapplySerialization.h"
#include "tempify/cli/CliParser.h"
#include "tempify/cli/ShellCompletion.h"
#include "tempify/config/TempifyConfig.h"
#include "tempify/frontend/IQuestionFrontend.h"
#include "tempify/hook/HookTrustStore.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteCommandRunner.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/question/AnswerFile.h"
#include "tempify/question/QuestionProcessor.h"
#include "tempify/store/AvailableTemplateCache.h"
#include "tempify/store/LocalTemplateStore.h"
#include "tempify/support/Errors.h"
#include "tempify/support/Paths.h"
#include "tempify/support/Version.h"
#include "tempify/template/TemplateLoader.h"
#include "tempify/template/TemplateInspector.h"
#include "tempify/template/TemplateLinter.h"
#include "tempify/template/TemplateTestRunner.h"
#include "tempify/template/TemplateValidator.h"

#include <filesystem>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <ranges>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <optional>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

bool stdin_is_tty() {
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return ::isatty(STDIN_FILENO) == 1;
#endif
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

std::string join_values(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << values[index];
    }
    return stream.str();
}

bool has_hooks(const tempify::BuildPlan& plan) {
    return plan.pre_hook_path.has_value()
        || plan.before_render_hook_path.has_value()
        || plan.after_render_hook_path.has_value()
        || plan.post_hook_path.has_value();
}

std::string normalize_prompt_value(std::string value) {
    const auto first = std::ranges::find_if(value, [](const unsigned char ch) {
        return !std::isspace(ch);
    });
    const auto last = std::ranges::find_if(value | std::views::reverse, [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    if (first >= last) {
        return {};
    }

    value = std::string(first, last);
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool hooks_disabled_for(const tempify::CliRequest& request,
                        const tempify::BuildPlan& plan,
                        const tempify::TemplateManifest& manifest,
                        const tempify::HookTrustStore& trust_store,
                        tempify::IQuestionFrontend& frontend) {
    if (!has_hooks(plan)) {
        return false;
    }

    const bool trusted = trust_store.is_trusted(manifest.root);

    switch (request.hook_acceptance) {
    case tempify::HookAcceptance::Yes:
        return false;
    case tempify::HookAcceptance::No:
        return true;
    case tempify::HookAcceptance::Ask:
        const bool force_prompt = [] {
            if (const char* value = std::getenv("TEMPIFY_FORCE_HOOK_PROMPT")) {
                return std::string_view(value) == "1";
            }
            return false;
        }();
        if (!force_prompt && !stdin_is_tty()) {
            return false;
        }

        if (trusted) {
            frontend.write_line("Template hooks trusted from previous approval.");
            return false;
        }

        frontend.write_line("Template defines hooks:");
        if (plan.pre_hook_path.has_value()) {
            frontend.write_line("- pre: " + plan.pre_hook_path->string());
        }
        if (plan.before_render_hook_path.has_value()) {
            frontend.write_line("- before_render: " + plan.before_render_hook_path->string());
        }
        if (plan.after_render_hook_path.has_value()) {
            frontend.write_line("- after_render: " + plan.after_render_hook_path->string());
        }
        if (plan.post_hook_path.has_value()) {
            frontend.write_line("- post: " + plan.post_hook_path->string());
        }
        frontend.write_line("Template root: " + manifest.root.string());
        while (true) {
            const auto input = frontend.prompt("Run hooks and trust this template next time? [Y/n]: ");
            if (!input.has_value()) {
                return false;
            }

            if (input->action == tempify::FrontendAction::Quit) {
                throw tempify::TempifyError("Hook confirmation aborted.");
            }

            if (input->action == tempify::FrontendAction::Back) {
                frontend.write_line("Enter yes or no.");
                continue;
            }

            const std::string value = normalize_prompt_value(input->value);
            if (value.empty() || value == "y" || value == "yes") {
                trust_store.trust(manifest.root);
                return false;
            }
            if (value == "n" || value == "no") {
                return true;
            }

            frontend.write_line("Enter yes or no.");
        }
    }

    return false;
}

bool entry_has_reapply_action(const tempify::BuildDiffEntry& entry,
                              const tempify::BuildReapplyAction action) {
    return entry.reapply_action == action;
}

tempify::BuildPlan build_reapply_write_plan(const tempify::BuildPlan& plan,
                                            const tempify::BuildDiffReport& report) {
    std::set<std::string> writable_paths;
    for (const auto& entry : report.entries) {
        if (entry.reapply_action == tempify::BuildReapplyAction::Create
            || entry.reapply_action == tempify::BuildReapplyAction::Update) {
            writable_paths.insert(entry.relative_path.generic_string());
        }
    }

    tempify::BuildPlan writable_plan = plan;
    writable_plan.existing_path_behavior = tempify::ExistingPathBehavior::Overwrite;
    writable_plan.pre_hook_path.reset();
    writable_plan.before_render_hook_path.reset();
    writable_plan.after_render_hook_path.reset();
    writable_plan.post_hook_path.reset();
    std::erase_if(writable_plan.files, [&](const tempify::PlannedFile& file) {
        const auto relative_path = std::filesystem::relative(file.output_path, plan.build_root).generic_string();
        return !writable_paths.contains(relative_path);
    });
    return writable_plan;
}

void validate_reapply_directory_paths(const tempify::BuildPlan& plan) {
    if (std::filesystem::exists(plan.build_root) && !std::filesystem::is_directory(plan.build_root)) {
        throw tempify::TempifyError("Reapply target must be existing directory: " + plan.build_root.string());
    }

    for (const auto& directory : plan.directories) {
        if (std::filesystem::exists(directory) && !std::filesystem::is_directory(directory)) {
            throw tempify::TempifyError("Reapply blocked: required directory path exists as file: " + directory.string());
        }
    }
}

void apply_reapply_deletes(const std::filesystem::path& build_root,
                           const tempify::BuildDiffReport& report) {
    for (const auto& entry : report.entries) {
        if (!entry_has_reapply_action(entry, tempify::BuildReapplyAction::Delete)) {
            continue;
        }
        const std::filesystem::path target = build_root / entry.relative_path;
        if (std::filesystem::exists(target)) {
            std::filesystem::remove(target);
        }
    }
}

std::map<std::string, std::string> build_reapply_lock_hashes(const tempify::BuildPlan& plan,
                                                             const tempify::BuildDiffReport& report,
                                                             const tempify::GenerationLockRecord& previous_lock) {
    std::map<std::string, std::string> hashes = tempify::build_generation_lock_managed_file_hashes(plan);
    for (const auto& entry : report.entries) {
        if (!entry_has_reapply_action(entry, tempify::BuildReapplyAction::Keep)) {
            continue;
        }
        const std::string relative_path = entry.relative_path.generic_string();
        const auto baseline_it = previous_lock.managed_file_hashes.find(relative_path);
        if (baseline_it == previous_lock.managed_file_hashes.end()) {
            throw tempify::TempifyError("Reapply baseline missing managed file hash for kept file: " + relative_path);
        }
        hashes[relative_path] = baseline_it->second;
    }
    return hashes;
}

void write_resolved_answers_file(const std::optional<std::filesystem::path>& answers_path,
                                 const tempify::TemplateManifest& manifest,
                                 const std::map<std::string, std::string>& values) {
    if (!answers_path.has_value()) {
        return;
    }

    std::map<std::string, std::string> answer_values;
    for (const auto& question : manifest.questions) {
        if (const auto it = values.find(question.key); it != values.end()) {
            answer_values[question.key] = it->second;
        }
    }
    tempify::write_answer_file(*answers_path, answer_values);
}

void ensure_reapply_ready(const tempify::BuildDiffReport& report) {
    if (!report.origin.detected) {
        throw tempify::TempifyError("`--reapply` requires existing .tempify-lock.json in target directory.");
    }

    if (report.reapply.status == tempify::BuildReapplyStatus::Ready) {
        return;
    }

    throw tempify::build_reapply_blocked_error(report);
}

void append_text_field(std::ostringstream& stream,
                       const std::string& label,
                       const std::string& value,
                       const bool full) {
    if (!full && value.empty()) {
        return;
    }

    stream << "  " << label << ": " << (value.empty() ? "<none>" : value) << '\n';
}

void append_text_bool(std::ostringstream& stream,
                      const std::string& label,
                      const bool value,
                      const bool full) {
    if (!full && !value) {
        return;
    }

    stream << "  " << label << ": " << (value ? "yes" : "no") << '\n';
}

void append_text_string_list(std::ostringstream& stream,
                             const std::string& label,
                             const std::vector<std::string>& values,
                             const bool full) {
    if (!full && values.empty()) {
        return;
    }

    stream << "  " << label << ": " << (values.empty() ? "[]" : join_values(values)) << '\n';
}

std::string format_questions_text(const tempify::TemplateManifest& manifest,
                                  const bool full) {
    std::ostringstream stream;
    std::vector<std::string> group_order = manifest.question_group_order;
    if (group_order.empty()) {
        for (const auto& question : manifest.questions) {
            if (std::ranges::find(group_order, question.group) == group_order.end()) {
                group_order.push_back(question.group);
            }
        }
    }
    stream << manifest.info.id;
    if (!manifest.info.name.empty()) {
        stream << " (" << manifest.info.name << ')';
    }
    stream << '\n';
    stream << "Groups: " << group_order.size() << "\n";
    stream << "Questions: " << manifest.questions.size() << "\n";

    if (manifest.questions.empty()) {
        return stream.str();
    }

    for (const auto& group_name : group_order) {
        stream << "\n[" << group_name << "]\n";
        for (const auto& question : manifest.questions) {
            if (question.group != group_name) {
                continue;
            }

            stream << "- " << question.key << " [" << question.type << "]\n";
            append_text_field(stream, "prompt", question.prompt, full);
            append_text_bool(stream, "optional", question.optional, full);
            append_text_bool(stream, "sensitive", question.sensitive, full);
            append_text_field(stream, "help", question.help, full);
            append_text_bool(stream,
                             "condition",
                             question.condition_is_function || question.condition_value.has_value(),
                             full);
            append_text_bool(stream, "validate", question.validate_is_function, full);
            append_text_string_list(stream, "aliases", question.aliases, full);
            append_text_string_list(stream, "choices", question.choices, full);
            stream << '\n';
        }
    }

    return stream.str();
}

std::size_t count_hook_phases(const tempify::TemplateManifest& manifest) {
    std::size_t count = 0;
    if (manifest.pre_hook_path.has_value()) {
        ++count;
    }
    if (manifest.before_render_hook_path.has_value()) {
        ++count;
    }
    if (manifest.after_render_hook_path.has_value()) {
        ++count;
    }
    if (manifest.post_hook_path.has_value()) {
        ++count;
    }
    return count;
}

std::string format_template_info_text(const tempify::TemplateManifest& manifest) {
    std::ostringstream stream;
    stream << manifest.info.id;
    if (!manifest.info.name.empty()) {
        stream << " (" << manifest.info.name << ')';
    }
    stream << '\n';
    stream << "Root: " << manifest.root.string() << '\n';
    stream << "Version: " << (manifest.info.version.empty() ? "<none>" : manifest.info.version) << '\n';
    stream << "Description: " << (manifest.info.description.empty() ? "<none>" : manifest.info.description) << '\n';
    stream << "Output: " << manifest.output_path_template << '\n';
    stream << "Source dir: " << manifest.source_dir.string() << '\n';
    stream << "Includes: " << manifest.include_ids.size();
    if (!manifest.include_ids.empty()) {
        stream << " (" << join_values(manifest.include_ids) << ')';
    }
    stream << '\n';
    stream << "Question groups: " << manifest.question_group_order.size() << '\n';
    stream << "Questions: " << manifest.questions.size() << '\n';
    stream << "Files: " << manifest.files.size() << '\n';
    stream << "Directories: " << manifest.directories.size() << '\n';
    stream << "Scripts: " << manifest.scripts.size() << '\n';
    stream << "Hooks: " << count_hook_phases(manifest) << '\n';
    return stream.str();
}

std::string format_template_info_json(const tempify::TemplateManifest& manifest) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(manifest.info.id) << "\",\n";
    stream << "  \"name\": \"" << json_escape(manifest.info.name) << "\",\n";
    stream << "  \"description\": \"" << json_escape(manifest.info.description) << "\",\n";
    stream << "  \"version\": \"" << json_escape(manifest.info.version) << "\",\n";
    stream << "  \"root\": \"" << json_escape(manifest.root.string()) << "\",\n";
    stream << "  \"output\": \"" << json_escape(manifest.output_path_template) << "\",\n";
    stream << "  \"source_dir\": \"" << json_escape(manifest.source_dir.string()) << "\",\n";
    stream << "  \"include_ids\": [\n";
    for (std::size_t index = 0; index < manifest.include_ids.size(); ++index) {
        stream << "    \"" << json_escape(manifest.include_ids[index]) << "\"";
        if (index + 1 < manifest.include_ids.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"question_group_count\": " << manifest.question_group_order.size() << ",\n";
    stream << "  \"question_count\": " << manifest.questions.size() << ",\n";
    stream << "  \"file_count\": " << manifest.files.size() << ",\n";
    stream << "  \"directory_count\": " << manifest.directories.size() << ",\n";
    stream << "  \"script_count\": " << manifest.scripts.size() << ",\n";
    stream << "  \"hook_phase_count\": " << count_hook_phases(manifest) << ",\n";
    stream << "  \"hooks\": {\n";
    stream << "    \"pre\": " << (manifest.pre_hook_path.has_value() ? "true" : "false") << ",\n";
    stream << "    \"before_render\": " << (manifest.before_render_hook_path.has_value() ? "true" : "false") << ",\n";
    stream << "    \"after_render\": " << (manifest.after_render_hook_path.has_value() ? "true" : "false") << ",\n";
    stream << "    \"post\": " << (manifest.post_hook_path.has_value() ? "true" : "false") << "\n";
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

std::size_t count_direct_template_dirs(const std::optional<std::filesystem::path>& workspace_templates_root) {
    std::error_code error;
    if (!workspace_templates_root.has_value() || !std::filesystem::is_directory(*workspace_templates_root, error)) {
        return 0;
    }

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(*workspace_templates_root, error)) {
        if (error) {
            break;
        }
        if (entry.is_directory()) {
            ++count;
        }
    }
    return count;
}

std::string format_doctor_text(const std::filesystem::path& data_root,
                               const std::optional<std::filesystem::path>& workspace_templates_root,
                               const std::filesystem::path& global_config_path,
                               const std::optional<std::filesystem::path>& workspace_config_path,
                               const tempify::LocalTemplateStore& store,
                               const std::string& shared_index_status,
                               const std::string& catalog_status) {
    std::ostringstream stream;
    stream << "Tempify Doctor\n";
    stream << "Data root: " << data_root.string() << '\n';
    stream << "Global config: " << global_config_path.string() << '\n';
    stream << "Global config exists: " << (std::filesystem::is_regular_file(global_config_path) ? "yes" : "no") << '\n';
    stream << "Shared templates root: " << store.templates_root().string() << '\n';
    stream << "Shared index: " << store.index_file().string() << '\n';
    stream << "Shared index exists: " << (std::filesystem::is_regular_file(store.index_file()) ? "yes" : "no") << '\n';
    stream << "Shared index status: " << shared_index_status << '\n';
    stream << "Workspace config: ";
    if (workspace_config_path.has_value()) {
        stream << workspace_config_path->string();
    } else {
        stream << "<none>";
    }
    stream << '\n';
    stream << "Workspace templates: ";
    if (workspace_templates_root.has_value()) {
        stream << workspace_templates_root->string();
    } else {
        stream << "<none>";
    }
    stream << '\n';
    stream << "Workspace template dirs: " << count_direct_template_dirs(workspace_templates_root) << '\n';
    stream << "Catalog status: " << catalog_status << '\n';
    return stream.str();
}

std::string format_doctor_json(const std::filesystem::path& data_root,
                               const std::optional<std::filesystem::path>& workspace_templates_root,
                               const std::filesystem::path& global_config_path,
                               const std::optional<std::filesystem::path>& workspace_config_path,
                               const tempify::LocalTemplateStore& store,
                               const std::string& shared_index_status,
                               const std::string& catalog_status) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"data_root\": \"" << json_escape(data_root.string()) << "\",\n";
    stream << "  \"global_config\": \"" << json_escape(global_config_path.string()) << "\",\n";
    stream << "  \"global_config_exists\": " << (std::filesystem::is_regular_file(global_config_path) ? "true" : "false") << ",\n";
    stream << "  \"shared_templates_root\": \"" << json_escape(store.templates_root().string()) << "\",\n";
    stream << "  \"shared_index\": \"" << json_escape(store.index_file().string()) << "\",\n";
    stream << "  \"shared_index_exists\": " << (std::filesystem::is_regular_file(store.index_file()) ? "true" : "false") << ",\n";
    stream << "  \"shared_index_status\": \"" << json_escape(shared_index_status) << "\",\n";
    stream << "  \"workspace_config\": ";
    if (workspace_config_path.has_value()) {
        stream << "\"" << json_escape(workspace_config_path->string()) << "\"";
    } else {
        stream << "null";
    }
    stream << ",\n";
    stream << "  \"workspace_templates_root\": ";
    if (workspace_templates_root.has_value()) {
        stream << "\"" << json_escape(workspace_templates_root->string()) << "\"";
    } else {
        stream << "null";
    }
    stream << ",\n";
    stream << "  \"workspace_template_dir_count\": " << count_direct_template_dirs(workspace_templates_root) << ",\n";
    stream << "  \"catalog_status\": \"" << json_escape(catalog_status) << "\"\n";
    stream << "}\n";
    return stream.str();
}

std::string format_catalog_json(const tempify::app_internal::TemplateCatalog& catalog) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"total\": " << catalog.visible.size() << ",\n";
    stream << "  \"templates\": [\n";
    for (std::size_t index = 0; index < catalog.visible.size(); ++index) {
        const auto& record = catalog.visible[index];
        const auto& info = record.info;
        const char* status = "available";
        switch (record.status) {
        case tempify::app_internal::VisibleTemplateStatus::Workspace:
            status = "workspace";
            break;
        case tempify::app_internal::VisibleTemplateStatus::Installed:
            status = "installed";
            break;
        case tempify::app_internal::VisibleTemplateStatus::Available:
            status = "available";
            break;
        }
        stream << "    {\"id\": \"" << json_escape(info.id)
               << "\", \"name\": \"" << json_escape(info.name)
               << "\", \"description\": \"" << json_escape(info.description)
               << "\", \"version\": \"" << json_escape(info.version)
               << "\", \"installed\": " << (record.installed ? "true" : "false")
               << ", \"status\": \"" << status << "\", \"root\": ";
        if (info.root.empty()) {
            stream << "null";
        } else {
            stream << "\"" << json_escape(info.root.string()) << "\"";
        }
        stream << '}';
        if (index + 1 < catalog.visible.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n";
    stream << "}\n";
    return stream.str();
}

std::string format_available_template_info_text(const tempify::AvailableTemplateRecord& record) {
    std::ostringstream stream;
    stream << record.id;
    if (!record.name.empty()) {
        stream << " (" << record.name << ')';
    }
    stream << '\n';
    stream << "Availability: available (registry cache)\n";
    stream << "Version: " << (record.version.empty() ? "<none>" : record.version) << '\n';
    stream << "Description: " << (record.description.empty() ? "<none>" : record.description) << '\n';
    stream << "Repository: " << (record.repository_id.empty() ? "<none>" : record.repository_id) << '\n';
    stream << "Source URL: " << (record.source_url.empty() ? "<none>" : record.source_url) << '\n';
    stream << "Source ref: " << (record.source_ref.empty() ? "<none>" : record.source_ref) << '\n';
    stream << "Source subdir: " << (record.source_subdir.empty() ? "<none>" : record.source_subdir) << '\n';
    return stream.str();
}

std::string format_available_template_info_json(const tempify::AvailableTemplateRecord& record) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(record.id) << "\",\n";
    stream << "  \"name\": \"" << json_escape(record.name) << "\",\n";
    stream << "  \"description\": \"" << json_escape(record.description) << "\",\n";
    stream << "  \"version\": \"" << json_escape(record.version) << "\",\n";
    stream << "  \"availability\": \"available (registry cache)\",\n";
    stream << "  \"repository\": \"" << json_escape(record.repository_id) << "\",\n";
    stream << "  \"source_url\": \"" << json_escape(record.source_url) << "\",\n";
    stream << "  \"source_ref\": \"" << json_escape(record.source_ref) << "\",\n";
    stream << "  \"source_subdir\": \"" << json_escape(record.source_subdir) << "\"\n";
    stream << "}\n";
    return stream.str();
}

std::string format_refresh_json(const std::size_t count,
                                const std::filesystem::path& index_file) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"refreshed\": " << count << ",\n";
    stream << "  \"index_file\": \"" << json_escape(index_file.string()) << "\"\n";
    stream << "}\n";
    return stream.str();
}

std::string format_validate_json(const tempify::TemplateManifest& manifest) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(manifest.info.id) << "\",\n";
    stream << "  \"status\": \"ok\"\n";
    stream << "}\n";
    return stream.str();
}

std::string format_inspect_json(const tempify::TemplateManifest& manifest) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(manifest.info.id) << "\",\n";
    stream << "  \"name\": \"" << json_escape(manifest.info.name) << "\",\n";
    stream << "  \"root\": \"" << json_escape(manifest.root.string()) << "\",\n";
    stream << "  \"output\": \"" << json_escape(manifest.output_path_template) << "\",\n";
    stream << "  \"source_roots\": [\n";
    for (std::size_t index = 0; index < manifest.source_roots.size(); ++index) {
        const auto& source_root = manifest.source_roots[index];
        stream << "    {\"template_id\": \"" << json_escape(source_root.template_id)
               << "\", \"path\": \"" << json_escape(source_root.path.string()) << "\"}";
        if (index + 1 < manifest.source_roots.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"include_ids\": [\n";
    for (std::size_t index = 0; index < manifest.include_ids.size(); ++index) {
        stream << "    \"" << json_escape(manifest.include_ids[index]) << "\"";
        if (index + 1 < manifest.include_ids.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"files\": [\n";
    for (std::size_t index = 0; index < manifest.files.size(); ++index) {
        const auto& file = manifest.files[index];
        stream << "    {\"relative_path\": \"" << json_escape(file.relative_path)
               << "\", \"source_template_id\": \"" << json_escape(file.source_template_id)
               << "\", \"source_path\": \"" << json_escape(file.source_path.string())
               << "\", \"render\": " << (file.render_with_prebyte ? "true" : "false")
               << ", \"excluded\": " << (file.excluded ? "true" : "false") << '}';
        if (index + 1 < manifest.files.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"questions\": [\n";
    for (std::size_t index = 0; index < manifest.questions.size(); ++index) {
        const auto& question = manifest.questions[index];
        stream << "    {\"key\": \"" << json_escape(question.key)
                << "\", \"type\": \"" << json_escape(question.type)
                << "\", \"group\": \"" << json_escape(question.group)
                << "\", \"sensitive\": " << (question.sensitive ? "true" : "false")
                << "\", \"source_path\": \"" << json_escape(question.source_path.string())
                << "\", \"source_index\": " << question.source_index << '}';
        if (index + 1 < manifest.questions.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"layout_rules\": [\n";
    for (std::size_t index = 0; index < manifest.layout_rules.size(); ++index) {
        const auto& rule = manifest.layout_rules[index];
        stream << "    {\"source\": \"" << json_escape(rule.source)
               << "\", \"target\": ";
        if (rule.target.has_value()) {
            stream << "\"" << json_escape(*rule.target) << "\"";
        } else {
            stream << "null";
        }
        stream << ", \"exclude\": " << (rule.exclude ? "true" : "false")
               << ", \"render\": ";
        if (rule.render.has_value()) {
            stream << (*rule.render ? "true" : "false");
        } else {
            stream << "null";
        }
        stream << ", \"source_template_id\": \"" << json_escape(rule.source_template_id)
               << "\", \"source_path\": \"" << json_escape(rule.source_path.string()) << "\"}";
        if (index + 1 < manifest.layout_rules.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"scripts\": [\n";
    for (std::size_t index = 0; index < manifest.scripts.size(); ++index) {
        const auto& script = manifest.scripts[index];
        stream << "    {\"name\": \"" << json_escape(script.name)
               << "\", \"path\": \"" << json_escape(script.path.string()) << "\"}";
        if (index + 1 < manifest.scripts.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
    stream << "  \"hooks\": {\n";
    auto append_hook_path = [&](const std::string& name, const std::optional<std::filesystem::path>& path, const bool trailing_comma) {
        stream << "    \"" << name << "\": ";
        if (path.has_value()) {
            stream << "\"" << json_escape(path->string()) << "\"";
        } else {
            stream << "null";
        }
        if (trailing_comma) {
            stream << ',';
        }
        stream << '\n';
    };
    append_hook_path("pre", manifest.pre_hook_path, true);
    append_hook_path("before_render", manifest.before_render_hook_path, true);
    append_hook_path("after_render", manifest.after_render_hook_path, true);
    append_hook_path("post", manifest.post_hook_path, false);
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

std::string format_lint_json(const std::string& template_id,
                             const std::vector<std::string>& warnings) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"template_id\": \"" << json_escape(template_id) << "\",\n";
    stream << "  \"warning_count\": " << warnings.size() << ",\n";
    stream << "  \"warnings\": [\n";
    for (std::size_t index = 0; index < warnings.size(); ++index) {
        stream << "    \"" << json_escape(warnings[index]) << "\"";
        if (index + 1 < warnings.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ]\n";
    stream << "}\n";
    return stream.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& content) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw tempify::TempifyError("Could not write file: " + path.string());
    }
    output << content;
}

bool has_cli_assignment(const std::vector<std::string>& args,
                        const std::vector<std::string_view>& flags) {
    for (std::size_t index = 0; index < args.size(); ++index) {
        for (const auto flag : flags) {
            if (args[index] == flag) {
                return true;
            }
            if (args[index].starts_with(std::string(flag) + "=")) {
                return true;
            }
        }
    }
    return false;
}

tempify::CliRequest apply_config_defaults(const tempify::CliRequest& request,
                                          const std::vector<std::string>& raw_args,
                                          const tempify::TempifyConfig& config) {
    tempify::CliRequest effective = request;

    if (config.hook_acceptance.has_value() &&
        !has_cli_assignment(raw_args, {"--accept-hooks", "--no-hooks"})) {
        effective.hook_acceptance = *config.hook_acceptance;
    }

    if (config.hook_timeout_ms.has_value() && !has_cli_assignment(raw_args, {"--hook-timeout-ms"})) {
        effective.hook_timeout_ms = *config.hook_timeout_ms;
    }

    if (config.existing_path_behavior.has_value() &&
        !effective.existing_path_behavior_override.has_value() &&
        !has_cli_assignment(raw_args, {"-f", "--overwrite-if-exists", "-s", "--skip-if-file-exists"})) {
        effective.existing_path_behavior_override = config.existing_path_behavior;
    }

    return effective;
}

}

namespace tempify {

int TempifyApp::run(const std::vector<std::string>& args) const {
    CliParser parser;
    CliRequest request = parser.parse(args);

    if (request.mode == CliMode::Help) {
        std::cout << parser.help_text(request);
        return 0;
    }

    if (request.mode == CliMode::Version) {
        std::cout << VERSION << '\n';
        return 0;
    }

    if (request.mode == CliMode::Completion) {
        std::cout << render_shell_completion(request.completion_shell.value_or("bash"));
        return 0;
    }

    if (request.mode == CliMode::PrebytePassthrough) {
        PrebyteCommandRunner runner;
        return runner.run(request.raw_prebyte_args);
    }

    const LuaEngine lua_engine;
    const TemplateLoader loader(lua_engine);
    const std::filesystem::path data_root = resolve_tempify_data_root();
    const LocalTemplateStore store(data_root);
    const AvailableTemplateCache available_cache(data_root);

    if (request.mode == CliMode::Refresh) {
        const std::size_t count = store.refresh(loader);
        if (request.refresh_json) {
            std::cout << format_refresh_json(count, store.index_file());
        } else {
            std::cout << "Refreshed " << count << " shared templates -> " << store.index_file().string() << '\n';
        }
        return 0;
    }

    const std::filesystem::path current_path = std::filesystem::current_path();
    const auto workspace_templates_root = find_workspace_templates_root(current_path);
    TempifyConfig config;
    const std::filesystem::path global_config_path = default_global_config_file_path();
    if (std::filesystem::is_regular_file(global_config_path)) {
        config = merge_tempify_config(config, load_tempify_config_file(global_config_path));
    }
    const auto workspace_config_path = find_workspace_config_file(current_path);
    if (workspace_config_path.has_value()) {
        config = merge_tempify_config(config, load_tempify_config_file(*workspace_config_path));
    }
    request = apply_config_defaults(request, args, config);

    if (request.mode == CliMode::Doctor) {
        std::string shared_index_status = "ok";
        try {
            static_cast<void>(store.list_templates());
        } catch (const TempifyError& error) {
            shared_index_status = error.what();
        }

        std::string catalog_status;
        try {
            const app_internal::TemplateCatalog catalog = app_internal::build_catalog(workspace_templates_root, store, available_cache, loader);
            catalog_status = std::to_string(catalog.infos.size()) + " templates visible";
        } catch (const TempifyError& error) {
            catalog_status = error.what();
        }

        if (request.doctor_json) {
            std::cout << format_doctor_json(data_root,
                                            workspace_templates_root,
                                            global_config_path,
                                            workspace_config_path,
                                            store,
                                            shared_index_status,
                                            catalog_status);
        } else {
            std::cout << format_doctor_text(data_root,
                                            workspace_templates_root,
                                            global_config_path,
                                            workspace_config_path,
                                            store,
                                            shared_index_status,
                                            catalog_status);
        }
        return 0;
    }

    const app_internal::TemplateCatalog catalog = app_internal::build_catalog(workspace_templates_root, store, available_cache, loader);

    if (request.mode == CliMode::TemplateList) {
        if (request.list_json) {
            std::cout << format_catalog_json(catalog);
        } else {
            app_internal::print_catalog(catalog);
        }
        return 0;
    }

    if (request.mode == CliMode::TemplateInfo) {
        std::optional<std::filesystem::path> template_root;
        try {
            template_root = app_internal::resolve_template_root(request, catalog, store);
        } catch (const TempifyError& error) {
            if (std::string_view(error.what()).find("Template not found: ") != 0) {
                throw;
            }
            const auto available = available_cache.find_template(request.template_ref);
            if (!available.has_value()) {
                throw;
            }
            if (request.info_json) {
                std::cout << format_available_template_info_json(*available);
            } else {
                std::cout << format_available_template_info_text(*available);
            }
            return 0;
        }

        const TemplateManifest manifest = loader.load(*template_root, catalog.index);
        if (request.info_json) {
            std::cout << format_template_info_json(manifest);
        } else {
            std::cout << format_template_info_text(manifest);
        }
        return 0;
    }

    if (request.mode == CliMode::TemplateValidate) {
        const std::filesystem::path template_root = app_internal::resolve_template_root(request, catalog, store);
        const TemplateManifest manifest = loader.load(template_root, catalog.index);
        const TemplateValidator validator;
        validator.validate(manifest);
        if (request.validate_json) {
            std::cout << format_validate_json(manifest);
        } else {
            std::cout << "Validated " << manifest.info.id << " -> OK\n";
        }
        return 0;
    }

    if (request.mode == CliMode::TemplateInspect) {
        const std::filesystem::path template_root = app_internal::resolve_template_root(request, catalog, store);
        const TemplateManifest manifest = loader.load(template_root, catalog.index);
        if (request.inspect_json) {
            std::cout << format_inspect_json(manifest);
        } else {
            std::cout << inspect_template_text(manifest);
        }
        return 0;
    }

    if (request.mode == CliMode::TemplateLint) {
        const std::filesystem::path template_root = app_internal::resolve_template_root(request, catalog, store);
        const TemplateManifest manifest = loader.load(template_root, catalog.index);
        const TemplateValidator validator;
        validator.validate(manifest);
        const TemplateLinter linter;
        const std::vector<std::string> warnings = linter.lint(manifest);
        if (request.lint_json) {
            std::cout << format_lint_json(manifest.info.id, warnings);
        } else {
            std::cout << format_template_lint_text(manifest.info.id, warnings);
        }
        return 0;
    }

    if (request.mode == CliMode::TemplateTest) {
        const std::filesystem::path template_root = app_internal::resolve_template_root(request, catalog, store);
        const TemplateManifest manifest = loader.load(template_root, catalog.index);
        const TemplateValidator validator;
        validator.validate(manifest);
        const PrebyteRenderer renderer;
        const TemplateTestRunner tester(lua_engine, renderer);
        if (request.test_list_fixtures) {
            if (request.test_json) {
                std::cout << format_template_fixture_listing_json(manifest.info.id,
                                                                  tester.list_fixtures(manifest, request.test_fixture_name));
            } else {
                for (const auto& name : tester.list_fixture_names(manifest, request.test_fixture_name)) {
                    std::cout << name << '\n';
                }
            }
            return 0;
        }
        const TemplateTestReport report = request.test_update_snapshots
            ? tester.update_snapshots(manifest, request.test_fixture_name)
            : tester.run(manifest, request.test_fixture_name);
        if (request.test_json) {
            std::cout << format_template_test_report_json(report);
        } else {
            std::cout << format_template_test_report(report);
        }
        for (const auto& fixture : report.fixtures) {
            if (fixture.failure_message.has_value()) {
                return 1;
            }
        }
        return 0;
    }

    if (request.mode == CliMode::QuestionsShow) {
        const std::filesystem::path template_root = app_internal::resolve_template_root(request, catalog, store);
        const TemplateManifest manifest = loader.load(template_root, catalog.index);
        if (request.questions_output_format == QuestionsOutputFormat::Json) {
            std::cout << lua_engine.export_questions_json(manifest, request.questions_full);
        } else {
            std::cout << format_questions_text(manifest, request.questions_full) << '\n';
        }
        return 0;
    }

    const std::filesystem::path template_root = app_internal::resolve_template_root(request, catalog, store);
    const TemplateManifest manifest = loader.load(template_root, catalog.index);
    std::unique_ptr<IQuestionFrontend> frontend = app_internal::make_frontend(request.use_tui);

    std::map<std::string, std::string> imported_values;
    if (request.answers_file.has_value()) {
        imported_values = load_answer_file(*request.answers_file, request.strict);
    }

    QuestionProcessor question_processor(lua_engine, *frontend);
    const std::map<std::string, std::string> values = question_processor.collect(
        manifest,
        request.variables,
        config.defaults,
        imported_values,
        request.non_interactive,
        request.strict,
        request.use_tui && !request.non_interactive);

    const PrebyteRenderer renderer;
    const BuildPlanner planner(renderer);
    BuildPlan plan = planner.plan(manifest, values, request.target_dir);
    if (request.existing_path_behavior_override.has_value()) {
        plan.existing_path_behavior = *request.existing_path_behavior_override;
    }

    if (request.plan_json || request.dry_run) {
        const BuildPlanReport report = build_plan_report(plan, manifest);
        if (request.plan_json) {
            std::cout << format_build_plan_json(report);
        } else {
            std::cout << format_build_plan_text(report);
        }
        return 0;
    }

    if (request.diff_only) {
        const BuildDiffReport report = build_diff_report(plan, manifest, values, renderer);
        if (request.diff_json) {
            std::cout << format_build_diff_json(report);
        } else {
            std::cout << format_build_diff_text(report);
        }
        return 0;
    }

    if (request.reapply) {
        const std::optional<GenerationLockRecord> previous_lock = load_generation_lock(plan.build_root / ".tempify-lock.json");
        if (!previous_lock.has_value()) {
            throw TempifyError("`--reapply` requires existing .tempify-lock.json in target directory: "
                               + (plan.build_root / ".tempify-lock.json").string());
        }

        const BuildDiffReport report = build_diff_report(plan, manifest, values, renderer);
        if (request.reapply_report) {
            if (request.diff_json) {
                std::cout << format_build_diff_json(report);
            } else {
                std::cout << format_build_diff_text(report);
            }
            return 0;
        }
        ensure_reapply_ready(report);
        validate_reapply_directory_paths(plan);

        const BuildExecutor executor(renderer, lua_engine);
        const BuildPlan reapply_plan = build_reapply_write_plan(plan, report);
        if (!reapply_plan.files.empty()) {
            executor.execute(reapply_plan, manifest, values, true, std::nullopt);
        }
        apply_reapply_deletes(plan.build_root, report);

        write_resolved_answers_file(request.write_answers_file, manifest, values);

        write_text_file(plan.build_root / ".tempify-lock.json",
                        format_generation_lock_json(manifest,
                                                    plan,
                                                    values,
                                                    request.hook_acceptance,
                                                    true,
                                                    build_reapply_lock_hashes(plan, report, *previous_lock)));

        if (request.diff_json) {
            std::cout << format_reapply_result_json(report);
        } else {
            std::cout << format_reapply_result_text(manifest.info.id, plan.build_root, report);
        }
        return 0;
    }

    const HookTrustStore trust_store(default_hook_trust_store_path(data_root));
    const bool disable_hooks = hooks_disabled_for(request, plan, manifest, trust_store, *frontend);
    const std::optional<std::chrono::milliseconds> hook_timeout = request.hook_timeout_ms == 0
        ? std::nullopt
        : std::optional<std::chrono::milliseconds>(std::chrono::milliseconds(request.hook_timeout_ms));

    const BuildExecutor executor(renderer, lua_engine);
    executor.execute(plan, manifest, values, disable_hooks, hook_timeout);

    write_resolved_answers_file(request.write_answers_file, manifest, values);

    write_text_file(plan.build_root / ".tempify-lock.json",
                    format_generation_lock_json(manifest,
                                                plan,
                                                values,
                                                request.hook_acceptance,
                                                disable_hooks));

    std::cout << "Generated " << manifest.info.id << " -> " << plan.build_root.string() << '\n';
    return 0;
}

}
