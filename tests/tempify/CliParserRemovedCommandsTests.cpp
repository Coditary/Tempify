#include "TestHarness.h"
#include "tempify/cli/CliParser.h"
#include "tempify/support/Errors.h"

#include <string>

namespace {

void require_removed_command_error(tempify::CliParser &parser, const std::vector<std::string> &args,
                                   const std::string &command_name, const std::string &migration_hint) {
    try {
        static_cast<void>(parser.parse(args));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        const std::string message = error.what();
        REQUIRE(message.find("Command `" + command_name + "` removed") != std::string::npos);
        REQUIRE(message.find(migration_hint) != std::string::npos);
    }
}

} // namespace

TEST_CASE(CliParser_reports_migration_errors_for_removed_commands) {
    tempify::CliParser parser;

    require_removed_command_error(parser, {"template", "list"}, "template", "tempify list");
    require_removed_command_error(parser, {"prebyte", "render"}, "prebyte", "tempify -p");
    require_removed_command_error(parser, {"schema", "basic_cpp"}, "schema", "-q|--questions");
    require_removed_command_error(parser, {"questions", "basic_cpp"}, "questions", "-q|--questions");
    require_removed_command_error(parser, {"registry", "refresh"}, "registry", "tempify refresh");
}

TEST_CASE(CliParser_parses_prebyte_passthrough_aliases) {
    tempify::CliParser parser;

    const tempify::CliRequest short_form = parser.parse({"-p", "--version"});
    REQUIRE(short_form.mode == tempify::CliMode::PrebytePassthrough);
    REQUIRE_EQ(short_form.raw_prebyte_args.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(short_form.raw_prebyte_args[0], std::string("--version"));

    const tempify::CliRequest long_form = parser.parse({"--prebyte", "render", "--flag"});
    REQUIRE(long_form.mode == tempify::CliMode::PrebytePassthrough);
    REQUIRE_EQ(long_form.raw_prebyte_args.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(long_form.raw_prebyte_args[0], std::string("render"));
    REQUIRE_EQ(long_form.raw_prebyte_args[1], std::string("--flag"));
}
