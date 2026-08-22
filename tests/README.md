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

## Fuzzing

Requires Clang with libFuzzer. Configure and run:

```bash
make fuzz
# or with a shorter smoke run:
TEMPIFY_FUZZ_MAX_TOTAL_TIME=10 make fuzz
```

Targets:

| Fuzzer | Input surface |
|--------|----------------|
| `fuzz_slugify` | `slugify()` |
| `fuzz_answer_json` | Prebyte JSON parser (answers/config baseline) |
| `fuzz_cli_parser` | `CliParser::parse()` |
| `fuzz_tempify_config` | `load_tempify_config_file()` |
| `fuzz_generation_lock` | `load_generation_lock()` + `content_fingerprint_hex()` |
| `fuzz_answer_file` | `load_answer_file()` (strict + non-strict) |

Seeds live under `tests/fault_tolerance/fuzz/seeds/`. Generated corpora are written to
`tests/fault_tolerance/fuzz/corpus/` (gitignored). Replay saved crash inputs from
`tests/fault_tolerance/fuzz/regression/<target>/` via `make fuzz-regression`.

Manual workflow:

```bash
cmake --preset fuzz
cmake --build --preset fuzz
ctest --test-dir build-cmake/fuzz -R '^fuzz_' --output-on-failure
```

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
