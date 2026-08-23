#include "tempify/cli/CliParser.h"
#include "tempify/support/Errors.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::string> tokenize_args(const std::uint8_t *data, std::size_t size) {
    std::vector<std::string> args;
    std::string current;

    for (std::size_t index = 0; index < size; ++index) {
        const char ch = static_cast<char>(data[index]);
        if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
            if (!current.empty()) {
                args.push_back(std::move(current));
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        args.push_back(std::move(current));
    }

    if (args.empty()) {
        args.emplace_back("list");
    }

    return args;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    const tempify::CliParser parser;

    try {
        (void)parser.parse(tokenize_args(data, size));
    } catch (const tempify::TempifyError &) {
    } catch (const std::exception &) {
    }

    return 0;
}
