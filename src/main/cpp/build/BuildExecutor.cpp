#include "tempify/build/BuildExecutor.h"

#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteRenderer.h"
#include "tempify/support/Errors.h"

#include <chrono>
#include <filesystem>
#include <optional>

namespace tempify {

namespace {

void prepare_directory_path(const std::filesystem::path &path, const ExistingPathBehavior behavior) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    if (std::filesystem::is_directory(path)) {
        return;
    }

    if (behavior == ExistingPathBehavior::Overwrite) {
        std::filesystem::remove(path);
        return;
    }

    throw TempifyError("Directory path already exists as file: " + path.string());
}

bool should_skip_output_file(const std::filesystem::path &path, const ExistingPathBehavior behavior) {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    if (std::filesystem::is_directory(path)) {
        throw TempifyError("Output file path already exists as directory: " + path.string());
    }

    if (behavior == ExistingPathBehavior::Skip) {
        return true;
    }

    if (behavior == ExistingPathBehavior::Error) {
        throw TempifyError("Output file already exists: " + path.string());
    }

    return false;
}

void run_hook_phase(const LuaEngine &lua_engine, const std::optional<std::filesystem::path> &hook_path,
                    const char *phase_name, const TemplateManifest &manifest, const BuildContext &context,
                    const PrebyteRenderer &renderer, const std::optional<std::chrono::milliseconds> timeout) {
    if (!hook_path.has_value()) {
        return;
    }

    try {
        lua_engine.run_hook(*hook_path, manifest, context, renderer, timeout);
    } catch (const TempifyError &error) {
        throw TempifyError(std::string("Hook phase '") + phase_name + "' failed (" + hook_path->string() +
                           "): " + error.what());
    }
}

} // namespace

BuildExecutor::BuildExecutor(const PrebyteRenderer &renderer, const LuaEngine &lua_engine)
    : renderer_(renderer), lua_engine_(lua_engine) {}

void BuildExecutor::execute(const BuildPlan &plan, const TemplateManifest &manifest,
                            const std::map<std::string, std::string> &values, const bool disable_hooks,
                            const std::optional<std::chrono::milliseconds> hook_timeout) const {
    if (std::filesystem::exists(plan.build_root)) {
        if (std::filesystem::is_directory(plan.build_root)) {
            if (plan.existing_path_behavior == ExistingPathBehavior::Error) {
                throw TempifyError("Target path already exists and overwrite is disabled: " + plan.build_root.string());
            }
        } else if (plan.existing_path_behavior == ExistingPathBehavior::Overwrite) {
            std::filesystem::remove(plan.build_root);
        } else {
            throw TempifyError("Target path exists as file and cannot be used as directory: " +
                               plan.build_root.string());
        }
    }

    const BuildContext context{
        .template_root = manifest.root,
        .build_root = plan.build_root,
        .values = values,
    };

    if (!disable_hooks) {
        run_hook_phase(lua_engine_, plan.pre_hook_path, "pre", manifest, context, renderer_, hook_timeout);
    }

    for (const auto &directory : plan.directories) {
        prepare_directory_path(directory, plan.existing_path_behavior);
        std::filesystem::create_directories(directory);
    }

    prebyte::Prebyte engine;
    renderer_.configure(engine, values, manifest);

    if (!disable_hooks) {
        run_hook_phase(lua_engine_, plan.before_render_hook_path, "before_render", manifest, context, renderer_,
                       hook_timeout);
    }

    for (const auto &file : plan.files) {
        if (should_skip_output_file(file.output_path, plan.existing_path_behavior)) {
            continue;
        }

        std::filesystem::create_directories(file.output_path.parent_path());
        if (file.render_with_prebyte) {
            if (plan.existing_path_behavior == ExistingPathBehavior::Overwrite &&
                std::filesystem::exists(file.output_path)) {
                std::filesystem::remove(file.output_path);
            }
            renderer_.render_file(engine, file.source_path, file.output_path);
            continue;
        }
        std::filesystem::copy_file(file.source_path, file.output_path,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    if (!disable_hooks) {
        run_hook_phase(lua_engine_, plan.after_render_hook_path, "after_render", manifest, context, renderer_,
                       hook_timeout);
    }

    if (!disable_hooks) {
        run_hook_phase(lua_engine_, plan.post_hook_path, "post", manifest, context, renderer_, hook_timeout);
    }
}

} // namespace tempify
