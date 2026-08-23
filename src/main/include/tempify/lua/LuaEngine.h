#pragma once

#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace tempify {

class PrebyteRenderer;

class LuaEngine {
  public:
    TemplateInfo load_template_info(const std::filesystem::path &template_root) const;
    TemplateManifest load_partial_manifest(const std::filesystem::path &template_root) const;
    std::optional<std::string> evaluate_default(const QuestionDefinition &question,
                                                const std::map<std::string, std::string> &values) const;
    bool evaluate_condition(const QuestionDefinition &question, const std::map<std::string, std::string> &values) const;
    std::optional<std::string> validate_answer(const QuestionDefinition &question, const std::string &candidate,
                                               const std::map<std::string, std::string> &values) const;
    std::string export_questions_json(const TemplateManifest &manifest, bool full = false) const;
    void run_hook(const std::filesystem::path &script_path, const TemplateManifest &manifest,
                  const BuildContext &context, const PrebyteRenderer &renderer,
                  std::optional<std::chrono::milliseconds> timeout = std::nullopt) const;
};

} // namespace tempify
