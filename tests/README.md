# Tempify Test Layout

Tests are grouped by non-functional quality attributes (NFR). Each folder targets a specific concern while sharing the same harness in `tests/support/`.

## Structure

| Folder | Purpose |
|--------|---------|
| `tests/tempify/` | Core unit and in-process integration tests |
| `tests/correctness/e2e/` | End-to-end workflows (render, validate, diff, reapply) |
| `tests/portability/cli/` | Real CLI subprocess tests (binary invocation) |
| `tests/security/e2e/` | Hook sandboxing, sensitive data, path safety |
| `tests/concurrency/e2e/` | Parallel CLI execution |
| `tests/fault_tolerance/fuzz/` | LibFuzzer targets and regression seeds |

## Running

```bash
make test
```

Individual tests are discovered from `TEST_CASE(...)` declarations and can be run via CTest:

```bash
ctest --test-dir build-cmake/dev -R TempifyCliBinary_help
```

## E2E subprocess tests

Subprocess tests use `tests/support/CliProcess.{h,cpp}` and resolve the binary from:

- `TEMPIFY_TEST_BINARY` (set automatically by CMake)
- `TEMPIFY_CLI_BINARY` compile-time fallback

Working directory and isolated `XDG_DATA_HOME` / `XDG_CONFIG_HOME` are configured per test via `tests/support/E2ETestSupport.h`.
