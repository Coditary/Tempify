#pragma once

#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"

#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace tempify {

class LuaEngine;
class PrebyteRenderer;

class BuildExecutor {
  public:
    BuildExecutor(const PrebyteRenderer &renderer, const LuaEngine &lua_engine);

    void execute(const BuildPlan &plan, const TemplateManifest &manifest,
                 const std::map<std::string, std::string> &values, bool disable_hooks,
                 std::optional<std::chrono::milliseconds> hook_timeout = std::nullopt) const;

  private:
    const PrebyteRenderer &renderer_;
    const LuaEngine &lua_engine_;
};

} // namespace tempify
