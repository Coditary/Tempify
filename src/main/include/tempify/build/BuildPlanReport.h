#pragma once

#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"

#include <map>
#include <string>

namespace tempify {

BuildPlanReport build_plan_report(const BuildPlan &plan, const TemplateManifest &manifest);

std::string format_build_plan_text(const BuildPlanReport &report);
std::string format_build_plan_json(const BuildPlanReport &report);

std::string format_generation_lock_json(const TemplateManifest &manifest, const BuildPlan &plan,
                                        const std::map<std::string, std::string> &values,
                                        HookAcceptance hook_acceptance, bool hooks_disabled);

std::string format_generation_lock_json(const TemplateManifest &manifest, const BuildPlan &plan,
                                        const std::map<std::string, std::string> &values,
                                        HookAcceptance hook_acceptance, bool hooks_disabled,
                                        const std::map<std::string, std::string> &managed_file_hashes);

} // namespace tempify
