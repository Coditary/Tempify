#pragma once

#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace tempify {

class PrebyteRenderer;

class BuildPlanner {
public:
    explicit BuildPlanner(const PrebyteRenderer& renderer);

    BuildPlan plan(const TemplateManifest& manifest,
                   const std::map<std::string, std::string>& values,
                   const std::optional<std::filesystem::path>& explicit_target = std::nullopt) const;

private:
    const PrebyteRenderer& renderer_;
};

}
