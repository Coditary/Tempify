#include "TestHarness.h"
#include "tempify/prebyte/PrebyteCommandRunner.h"
#include "tempify/support/Errors.h"

TEST_CASE(PrebyteCommandRunner_runs_embedded_prebyte_version_command) {
    tempify::PrebyteCommandRunner runner;
    REQUIRE_EQ(runner.run({"--version"}), 0);
}

TEST_CASE(PrebyteCommandRunner_invalid_command_throws) {
    tempify::PrebyteCommandRunner runner;
    try {
        static_cast<void>(runner.run({"definitely-not-a-prebyte-input-file"}));
        REQUIRE(false);
    } catch (const std::exception &) {
        REQUIRE(true);
    }
}
