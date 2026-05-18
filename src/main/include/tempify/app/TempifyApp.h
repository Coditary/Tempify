#pragma once

#include <string>
#include <vector>

namespace tempify {

class TempifyApp {
public:
    int run(const std::vector<std::string>& args) const;
};

}
