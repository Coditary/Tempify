#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/question/AnswerFile.h"

#include <map>
#include <string>

namespace {

using tempify::test_support::read_text_file;
using tempify::test_support::write_text_file;

} // namespace

TEST_CASE(AnswerFile_roundtrip_preserves_scalar_values) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-answer-file-roundtrip.json";
    const std::map<std::string, std::string> original = {
        {"project_name", "Roundtrip App"},
        {"include_ci", "true"},
        {"count", "3"},
    };
    tempify::write_answer_file(path, original);

    const std::map<std::string, std::string> loaded = tempify::load_answer_file(path, false);
    REQUIRE_EQ(loaded.at("project_name"), std::string("Roundtrip App"));
    REQUIRE_EQ(loaded.at("include_ci"), std::string("true"));
    REQUIRE_EQ(loaded.at("count"), std::string("3"));
    REQUIRE(read_text_file(path).find("\"project_name\": \"Roundtrip App\"") != std::string::npos);
}

TEST_CASE(AnswerFile_loads_bool_and_number_scalars) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-answer-file-scalars.json";
    write_text_file(path,
                    "{\n"
                    "  \"enabled\": false,\n"
                    "  \"retries\": 2\n"
                    "}\n");

    const std::map<std::string, std::string> loaded = tempify::load_answer_file(path, false);
    REQUIRE_EQ(loaded.at("enabled"), std::string("false"));
    REQUIRE_EQ(loaded.at("retries"), std::string("2"));
}
