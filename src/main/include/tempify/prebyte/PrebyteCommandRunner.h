#pragma once

#include <string>
#include <vector>

namespace tempify {

class PrebyteCommandRunner {
public:
    int run(const std::vector<std::string>& args) const;
};

}
