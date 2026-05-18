#pragma once

#include "tempify/domain/TemplateManifest.h"

#include <string>
#include <vector>

namespace tempify {

class TemplateLinter {
public:
    std::vector<std::string> lint(const TemplateManifest& manifest) const;
};

std::string format_template_lint_text(const std::string& template_id,
                                      const std::vector<std::string>& warnings);

}
