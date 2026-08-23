#include "tempify/build/BuildPlanner.h"

#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

namespace tempify {

namespace {

std::filesystem::path normalize_relative_output_path(const std::filesystem::path &path) {
    if (path.empty() || path.is_absolute()) {
        throw TempifyError("Rendered output path must be relative and non-empty");
    }

    const std::filesystem::path normalized = path.lexically_normal();
    for (const auto &part : normalized) {
        if (part == "..") {
            throw TempifyError("Rendered output path escapes build root: " + normalized.string());
        }
    }

    return normalized;
}

void collect_parent_directories(std::set<std::filesystem::path> &directories, const std::filesystem::path &root,
                                const std::filesystem::path &output_path) {
    std::filesystem::path current = output_path.parent_path();
    while (!current.empty() && current != root && current.native().size() >= root.native().size()) {
        directories.insert(current);
        current = current.parent_path();
    }
}

int path_depth(const std::filesystem::path &path) {
    int depth = 0;
    for (const auto &part : path) {
        static_cast<void>(part);
        ++depth;
    }
    return depth;
}

const LayoutRule *layout_rule_for(const TemplateManifest &manifest, const std::string &relative_path) {
    for (auto it = manifest.layout_rules.rbegin(); it != manifest.layout_rules.rend(); ++it) {
        if (it->source == relative_path) {
            return &*it;
        }
    }
    return nullptr;
}

} // namespace

BuildPlanner::BuildPlanner(const PrebyteRenderer &renderer) : renderer_(renderer) {}

BuildPlan BuildPlanner::plan(const TemplateManifest &manifest, const std::map<std::string, std::string> &values,
                             const std::optional<std::filesystem::path> &explicit_target) const {
    prebyte::Prebyte engine;
    renderer_.configure(engine, values, manifest);

    BuildPlan plan;
    plan.existing_path_behavior = manifest.overwrite ? ExistingPathBehavior::Overwrite : ExistingPathBehavior::Error;
    plan.pre_hook_path = manifest.pre_hook_path;
    plan.before_render_hook_path = manifest.before_render_hook_path;
    plan.after_render_hook_path = manifest.after_render_hook_path;
    plan.post_hook_path = manifest.post_hook_path;

    if (explicit_target.has_value()) {
        plan.build_root =
            explicit_target->is_absolute() ? *explicit_target : std::filesystem::current_path() / *explicit_target;
    } else {
        const std::string rendered = renderer_.render_string(
            engine, manifest.output_path_template.empty() ? manifest.info.id : manifest.output_path_template);
        if (rendered.empty()) {
            throw TempifyError("Rendered build root is empty for template " + manifest.info.id);
        }
        plan.build_root = std::filesystem::path(rendered);
        if (plan.build_root.is_relative()) {
            plan.build_root = std::filesystem::current_path() / plan.build_root;
        }
    }
    plan.build_root = plan.build_root.lexically_normal();

    std::set<std::filesystem::path> directories;
    directories.insert(plan.build_root);

    for (const auto &raw_directory : manifest.directories) {
        if (const LayoutRule *rule = layout_rule_for(manifest, raw_directory.relative_path);
            rule != nullptr && rule->exclude) {
            continue;
        }

        const std::string layout_source = [&]() {
            if (const LayoutRule *rule = layout_rule_for(manifest, raw_directory.relative_path);
                rule != nullptr && rule->target.has_value()) {
                return *rule->target;
            }
            return raw_directory.relative_path;
        }();

        const std::string rendered = renderer_.render_string(engine, layout_source);
        if (rendered.empty()) {
            continue;
        }
        const std::filesystem::path normalized = normalize_relative_output_path(std::filesystem::path(rendered));
        directories.insert((plan.build_root / normalized).lexically_normal());
    }

    std::map<std::string, std::filesystem::path> output_collision_guard;
    for (const auto &source : manifest.files) {
        if (source.excluded) {
            continue;
        }
        const LayoutRule *rule = layout_rule_for(manifest, source.relative_path);
        if (rule != nullptr && rule->exclude) {
            continue;
        }

        const std::string logical_source =
            (rule != nullptr && rule->target.has_value()) ? *rule->target : source.relative_path;
        std::filesystem::path output_relative =
            normalize_relative_output_path(std::filesystem::path(renderer_.render_string(engine, logical_source)));
        const bool render_with_prebyte =
            (rule != nullptr && rule->render.has_value()) ? *rule->render : source.render_with_prebyte;
        if (render_with_prebyte) {
            output_relative.replace_extension();
        }

        const std::filesystem::path output_path = (plan.build_root / output_relative).lexically_normal();
        const std::string collision_key = output_path.generic_string();
        const auto collision = output_collision_guard.find(collision_key);
        if (collision != output_collision_guard.end() && collision->second != source.source_path) {
            throw TempifyError("Multiple template files render to same output path: " + collision_key);
        }
        output_collision_guard[collision_key] = source.source_path;

        collect_parent_directories(directories, plan.build_root, output_path);
        plan.files.push_back({
            .source_path = source.source_path,
            .output_path = output_path,
            .render_with_prebyte = render_with_prebyte,
        });
    }

    plan.directories.assign(directories.begin(), directories.end());
    std::sort(plan.directories.begin(), plan.directories.end(), [](const auto &left, const auto &right) {
        if (path_depth(left) == path_depth(right)) {
            return left.generic_string() < right.generic_string();
        }
        return path_depth(left) < path_depth(right);
    });

    std::sort(plan.files.begin(), plan.files.end(), [](const PlannedFile &left, const PlannedFile &right) {
        return left.output_path.generic_string() < right.output_path.generic_string();
    });

    return plan;
}

} // namespace tempify
