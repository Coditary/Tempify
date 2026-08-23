#include "tempify/template/TemplateLoader.h"

#include "tempify/lua/LuaEngine.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace tempify {

namespace {

std::string format_stack(const std::vector<std::filesystem::path> &stack) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < stack.size(); ++index) {
        if (index > 0) {
            stream << " -> ";
        }
        stream << stack[index].string();
    }
    return stream.str();
}

} // namespace

TemplateLoader::TemplateLoader(const LuaEngine &lua_engine) : lua_engine_(lua_engine) {}

TemplateInfo TemplateLoader::summarize(const std::filesystem::path &template_root) const {
    return lua_engine_.load_template_info(template_root);
}

TemplateManifest TemplateLoader::load(const std::filesystem::path &template_root,
                                      const std::map<std::string, std::filesystem::path> &template_index) const {
    std::vector<std::filesystem::path> stack;
    return load_recursive(template_root, template_index, stack);
}

TemplateManifest TemplateLoader::load_recursive(const std::filesystem::path &template_root,
                                                const std::map<std::string, std::filesystem::path> &template_index,
                                                std::vector<std::filesystem::path> &stack) const {
    if (std::find(stack.begin(), stack.end(), template_root) != stack.end()) {
        std::vector<std::filesystem::path> cycle = stack;
        cycle.push_back(template_root);
        throw TempifyError("Template include cycle detected: " + format_stack(cycle));
    }

    stack.push_back(template_root);
    TemplateManifest current = lua_engine_.load_partial_manifest(template_root);
    TemplateManifest merged;

    for (const auto &include_id : current.include_ids) {
        const auto it = template_index.find(include_id);
        if (it == template_index.end()) {
            throw TempifyError("Unknown included template id '" + include_id + "' while loading " + current.info.id);
        }
        merged = merge(merged, load_recursive(it->second, template_index, stack), current.merge_config, false);
    }

    merged = merge(merged, current, current.merge_config, true);
    stack.pop_back();
    return merged;
}

} // namespace tempify
