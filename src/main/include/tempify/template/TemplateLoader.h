#pragma once

#include "tempify/domain/TemplateManifest.h"

#include <filesystem>
#include <map>

namespace tempify {

class LuaEngine;

class TemplateLoader {
  public:
    explicit TemplateLoader(const LuaEngine &lua_engine);

    TemplateInfo summarize(const std::filesystem::path &template_root) const;
    TemplateManifest load(const std::filesystem::path &template_root,
                          const std::map<std::string, std::filesystem::path> &template_index) const;

  private:
    TemplateManifest load_recursive(const std::filesystem::path &template_root,
                                    const std::map<std::string, std::filesystem::path> &template_index,
                                    std::vector<std::filesystem::path> &stack) const;
    TemplateManifest merge(const TemplateManifest &base, const TemplateManifest &overlay,
                           const TemplateMergeConfig &policy, bool overlay_is_local) const;

    const LuaEngine &lua_engine_;
};

} // namespace tempify
