#include "tempify/template/TemplateLinter.h"

#include <set>
#include <sstream>

namespace tempify {

namespace {

std::string describe_question(const QuestionDefinition& question) {
    return question.source_path.empty()
        ? question.key
        : question.key + " (" + question.source_path.string() + "#" + std::to_string(question.source_index) + ")";
}

bool has_matching_source_root(const TemplateManifest& manifest,
                              const std::filesystem::path& path) {
    for (const auto& source_root : manifest.source_roots) {
        const auto root = source_root.path.lexically_normal().generic_string();
        const auto candidate = path.lexically_normal().generic_string();
        if (candidate == root || (candidate.size() > root.size() && candidate.starts_with(root) && candidate[root.size()] == '/')) {
            return true;
        }
    }
    return false;
}

}

std::vector<std::string> TemplateLinter::lint(const TemplateManifest& manifest) const {
    std::vector<std::string> warnings;

    if (manifest.info.description.empty()) {
        warnings.push_back("template description is empty");
    }
    if (manifest.info.version.empty()) {
        warnings.push_back("template version is empty");
    }

    std::set<std::string> layout_sources;
    for (const auto& rule : manifest.layout_rules) {
        layout_sources.insert(rule.source);
    }

    for (const auto& question : manifest.questions) {
        if (question.prompt.empty()) {
            warnings.push_back("question missing prompt: " + describe_question(question));
        }
        if (question.type == "choice" && question.choices.empty()) {
            warnings.push_back("choice question has no choices: " + describe_question(question));
        }
        if (question.optional && question.help.empty()) {
            warnings.push_back("optional question missing help text: " + describe_question(question));
        }
        if ((question.condition_is_function || question.condition_value.has_value()) && question.help.empty()) {
            warnings.push_back("conditional question missing help text: " + describe_question(question));
        }
    }

    for (const auto& file : manifest.files) {
        if (!has_matching_source_root(manifest, file.source_path)) {
            warnings.push_back("file source outside declared source roots: " + file.source_path.string());
        }
        if (file.render_with_prebyte && !layout_sources.contains(file.relative_path) && !file.relative_path.ends_with(".pbt")) {
            warnings.push_back("rendered file does not use .pbt extension: " + file.relative_path);
        }
    }

    for (const auto& rule : manifest.layout_rules) {
        if (!rule.exclude && !rule.target.has_value()) {
            warnings.push_back("layout rule relies on implicit target path for source: " + rule.source);
        }
    }

    return warnings;
}

std::string format_template_lint_text(const std::string& template_id,
                                      const std::vector<std::string>& warnings) {
    std::ostringstream stream;
    stream << "Lint " << template_id << '\n';
    stream << "Warnings: " << warnings.size() << '\n';
    for (const auto& warning : warnings) {
        stream << "- " << warning << '\n';
    }
    return stream.str();
}

}
