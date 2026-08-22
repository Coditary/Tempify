#include "TestHarness.h"
#include "tempify/prebyte/PrebyteCommandRunner.h"

TEST_CASE(PrebyteCommandRunner_runs_embedded_prebyte_version_command) {
    tempify::PrebyteCommandRunner runner;
    REQUIRE_EQ(runner.run({"--version"}), 0);
}
