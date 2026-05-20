#pragma once

#include "tempify/build/BuildDiffReport.h"
#include "tempify/support/Errors.h"

#include <filesystem>
#include <string>

namespace tempify {

std::string format_reapply_result_text(const std::string& template_id,
                                       const std::filesystem::path& build_root,
                                       const BuildDiffReport& report);

std::string format_reapply_result_json(const BuildDiffReport& report);

ReapplyBlockedError build_reapply_blocked_error(const BuildDiffReport& report);
std::string format_reapply_blocked_error_json(const ReapplyBlockedError& error);

}
