#pragma once

#include "tempify/domain/CliRequest.h"

#include <string>
#include <vector>

namespace tempify {

class CliParser {
public:
    CliRequest parse(const std::vector<std::string>& args) const;
    std::string help_text(const CliRequest& request) const;
};

}
