#include "tempify/template/TemplateInspector.h"

#include <sstream>

namespace tempify {

namespace {

void append_hook(std::ostringstream& stream,
                 const std::string& name,
                 const std::optional<std::filesystem::path>& path) {
    if (!path.has_value()) {
        return;
    }
    stream << "- " << name << ": " << path->string() << '\n';
}

}

std::string inspect_template_text(const TemplateManifest& manifest) {
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

    stream << "\nSource Roots:\n";
    for (const auto& source_root : manifest.source_roots) {
        stream << "- " << source_root.template_id << ": " << source_root.path.string() << '\n';
    }

    stream << "\nIncludes:\n";
    if (manifest.include_ids.empty()) {
        stream << "- <none>\n";
    } else {
        for (const auto& include_id : manifest.include_ids) {
            stream << "- " << include_id << '\n';
        }
    }

    stream << "\nFiles:\n";
    if (manifest.files.empty()) {
        stream << "- <none>\n";
    } else {
        for (const auto& file : manifest.files) {
            stream << "- " << file.relative_path << " <- " << file.source_template_id;
            if (file.render_with_prebyte) {
                stream << " [render]";
            }
            if (file.excluded) {
                stream << " [excluded]";
            }
            stream << '\n';
        }
    }

    stream << "\nQuestions:\n";
    if (manifest.questions.empty()) {
        stream << "- <none>\n";
    } else {
        for (const auto& question : manifest.questions) {
            stream << "- " << question.key << " [" << question.type << "]";
            if (!question.group.empty()) {
                stream << " group=" << question.group;
            }
            stream << " <- " << question.source_path.string() << "#" << question.source_index;
            if (!question.aliases.empty()) {
                stream << " aliases=";
                for (std::size_t index = 0; index < question.aliases.size(); ++index) {
                    if (index > 0) {
                        stream << ',';
                    }
                    stream << question.aliases[index];
                }
            }
            stream << '\n';
        }
    }

    stream << "\nLayout Rules:\n";
    if (manifest.layout_rules.empty()) {
        stream << "- <none>\n";
    } else {
        for (const auto& rule : manifest.layout_rules) {
            stream << "- source=" << rule.source;
            if (rule.target.has_value()) {
                stream << " target=" << *rule.target;
            }
            if (rule.exclude) {
                stream << " exclude=yes";
            }
            if (rule.render.has_value()) {
                stream << " render=" << (*rule.render ? "yes" : "no");
            }
            stream << " <- " << rule.source_template_id;
            if (!rule.source_path.empty()) {
                stream << " (" << rule.source_path.string() << ")";
            }
            stream << '\n';
        }
    }

    stream << "\nScripts:\n";
    if (manifest.scripts.empty()) {
        stream << "- <none>\n";
    } else {
        for (const auto& script : manifest.scripts) {
            stream << "- " << script.name << " <- " << script.path.string() << '\n';
        }
    }

    stream << "\nHooks:\n";
    if (!manifest.pre_hook_path.has_value()
        && !manifest.before_render_hook_path.has_value()
        && !manifest.after_render_hook_path.has_value()
        && !manifest.post_hook_path.has_value()) {
        stream << "- <none>\n";
    } else {
        append_hook(stream, "pre", manifest.pre_hook_path);
        append_hook(stream, "before_render", manifest.before_render_hook_path);
        append_hook(stream, "after_render", manifest.after_render_hook_path);
        append_hook(stream, "post", manifest.post_hook_path);
    }

    return stream.str();
}

}
