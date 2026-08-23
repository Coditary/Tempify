#include "tempify/prebyte/PrebyteRenderer.h"

#include <set>

namespace tempify {

void PrebyteRenderer::configure(prebyte::Prebyte &engine, const std::map<std::string, std::string> &values,
                                const TemplateManifest &manifest) const {
    for (const auto &[key, value] : values) {
        engine.set_variable(key, value);
    }

    std::set<std::string> include_paths;
    for (const auto &root : manifest.source_roots) {
        if (include_paths.insert(root.path.string()).second) {
            engine.add_include_path(root.path.string());
        }
    }
    for (const auto &path : manifest.prebyte_config.include_paths) {
        if (include_paths.insert(path.string()).second) {
            engine.add_include_path(path.string());
        }
    }

    for (const auto &[name, value] : manifest.prebyte_config.rules) {
        engine.set_rule(name, value);
    }
}

std::string PrebyteRenderer::render_string(const prebyte::Prebyte &engine, std::string_view input) const {
    return engine.process(std::string(input));
}

void PrebyteRenderer::render_file(const prebyte::Prebyte &engine, const std::filesystem::path &input_path,
                                  const std::filesystem::path &output_path) const {
    engine.process_file(input_path.string(), output_path.string());
}

} // namespace tempify
