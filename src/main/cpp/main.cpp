#include "support/Diagnostic.h"
#include "tempify/app/TempifyApp.h"
#include "tempify/build/ReapplySerialization.h"
#include "tempify/support/Errors.h"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool wants_json_errors(const std::vector<std::string> &args) {
    if (args.empty()) {
        return false;
    }

    if (args[0] == "list" || args[0] == "info" || args[0] == "doctor" || args[0] == "validate" ||
        args[0] == "inspect" || args[0] == "lint" || args[0] == "refresh" || args[0] == "reapply" ||
        args[0] == "test") {
        for (std::size_t index = 1; index < args.size(); ++index) {
            if (args[index] == "--json") {
                return true;
            }
        }
    }

    if (args[0] != "list" && args[0] != "info" && args[0] != "doctor" && args[0] != "validate" &&
        args[0] != "inspect" && args[0] != "lint" && args[0] != "refresh" && args[0] != "test" &&
        args[0] != "completion" && args[0] != "process" && args[0] != "reapply" && args[0] != "-p" &&
        args[0] != "--prebyte" && args[0] != "help") {
        for (std::size_t index = 1; index < args.size(); ++index) {
            if (args[index] == "--json") {
                return true;
            }
        }
    }

    return false;
}

std::string json_escape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

void write_json_error(const std::string &code, const std::string &message) {
    std::cerr << "{\n"
              << "  \"status\": \"error\",\n"
              << "  \"code\": \"" << code << "\",\n"
              << "  \"message\": \"" << json_escape(message) << "\"\n"
              << "}\n";
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    try {
        tempify::TempifyApp app;
        return app.run(args);
    } catch (const tempify::ReapplyBlockedError &error) {
        if (wants_json_errors(args)) {
            std::cerr << tempify::format_reapply_blocked_error_json(error);
        } else {
            std::cerr << error.what() << '\n';
        }
        return 1;
    } catch (const prebyte::DiagnosticError &error) {
        if (wants_json_errors(args)) {
            write_json_error("CLI_ERROR", error.what());
        } else {
            std::cerr << error.what() << '\n';
        }
        return 1;
    } catch (const std::exception &error) {
        if (wants_json_errors(args)) {
            write_json_error("CLI_ERROR", error.what());
        } else {
            std::cerr << error.what() << '\n';
        }
        return 1;
    }
}
