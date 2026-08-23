#include "tempify/prebyte/PrebyteCommandRunner.h"

#include "app/AppRunner.h"
#include "cli/CommandParser.h"

namespace tempify {

int PrebyteCommandRunner::run(const std::vector<std::string> &args) const {
    prebyte::CommandParser parser;
    prebyte::AppRunner runner;
    const prebyte::Command command = parser.parse(args);
    runner.run(command);
    return 0;
}

} // namespace tempify
