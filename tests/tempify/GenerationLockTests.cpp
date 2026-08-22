#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/build/GenerationLock.h"

#include <string>

namespace {

using tempify::test_support::write_text_file;

} // namespace

TEST_CASE(GenerationLock_content_fingerprint_is_stable_for_same_input) {
    REQUIRE_EQ(tempify::content_fingerprint_hex("hello"), tempify::content_fingerprint_hex("hello"));
    REQUIRE(tempify::content_fingerprint_hex("hello") != tempify::content_fingerprint_hex("hello\n"));
}

TEST_CASE(GenerationLock_missing_file_returns_nullopt) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-generation-lock-missing.json";
    std::filesystem::remove(path);
    REQUIRE(!tempify::load_generation_lock(path).has_value());
}

TEST_CASE(GenerationLock_loads_managed_files_and_template_metadata) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-generation-lock-valid.json";
    write_text_file(path,
                    "{\n"
                    "  \"tempify_version\": \"0.1.0\",\n"
                    "  \"build_root\": \"/tmp/out\",\n"
                    "  \"generated_at\": \"2026-01-01T00:00:00Z\",\n"
                    "  \"existing_path_behavior\": \"error\",\n"
                    "  \"hook_acceptance\": \"yes\",\n"
                    "  \"hooks_disabled\": false,\n"
                    "  \"managed_files\": [\n"
                    "    \"README.md\"\n"
                    "  ],\n"
                    "  \"managed_file_hashes\": {\n"
                    "    \"README.md\": \"abc123\"\n"
                    "  },\n"
                    "  \"values\": {\n"
                    "    \"project_name\": \"Demo\"\n"
                    "  },\n"
                    "  \"template\": {\n"
                    "    \"id\": \"basic_cpp\",\n"
                    "    \"name\": \"Basic C++\",\n"
                    "    \"version\": \"1.0.0\",\n"
                    "    \"root\": \"/tmp/templates/basic_cpp\"\n"
                    "  }\n"
                    "}\n");

    const std::optional<tempify::GenerationLockRecord> record = tempify::load_generation_lock(path);
    REQUIRE(record.has_value());
    REQUIRE_EQ(record->template_info.id, std::string("basic_cpp"));
    REQUIRE_EQ(record->template_info.version, std::string("1.0.0"));
    REQUIRE_EQ(record->build_root, std::string("/tmp/out"));
    REQUIRE(record->managed_files.contains("README.md"));
    REQUIRE_EQ(record->managed_file_hashes.at("README.md"), std::string("abc123"));
    REQUIRE_EQ(record->values.at("project_name"), std::string("Demo"));
}
