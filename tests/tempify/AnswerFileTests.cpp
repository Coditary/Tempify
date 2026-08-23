#include "TempifyTestSupport.h"
#include "TestHarness.h"
#include "tempify/question/AnswerFile.h"
#include "tempify/support/Errors.h"

#include <map>
#include <string>

namespace {

using tempify::test_support::read_text_file;
using tempify::test_support::write_text_file;

} // namespace

TEST_CASE(AnswerFile_roundtrip_preserves_scalar_values) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-answer-file-roundtrip.json";
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
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-answer-file-scalars.json";
    write_text_file(path, "{\n"
                          "  \"enabled\": false,\n"
                          "  \"retries\": 2\n"
                          "}\n");

    const std::map<std::string, std::string> loaded = tempify::load_answer_file(path, false);
    REQUIRE_EQ(loaded.at("enabled"), std::string("false"));
    REQUIRE_EQ(loaded.at("retries"), std::string("2"));
}

TEST_CASE(AnswerFile_roundtrip_escapes_special_characters) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-answer-file-escape.json";
    const std::map<std::string, std::string> original = {
        {"quoted", "say \"hi\""},
        {"multiline", "line1\nline2"},
        {"tabbed", "a\tb"},
        {"backslash", "path\\to\\file"},
    };
    tempify::write_answer_file(path, original);

    const std::map<std::string, std::string> loaded = tempify::load_answer_file(path, false);
    REQUIRE_EQ(loaded.at("quoted"), std::string("say \"hi\""));
    REQUIRE_EQ(loaded.at("multiline"), std::string("line1\nline2"));
    REQUIRE_EQ(loaded.at("tabbed"), std::string("a\tb"));
    REQUIRE_EQ(loaded.at("backslash"), std::string("path\\to\\file"));

    const std::string serialized = read_text_file(path);
    REQUIRE(serialized.find("\\\"hi\\\"") != std::string::npos);
    REQUIRE(serialized.find("\\n") != std::string::npos);
    REQUIRE(serialized.find("\\t") != std::string::npos);
}

TEST_CASE(AnswerFile_loads_double_scalar_values) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-answer-file-double.json";
    write_text_file(path, "{\n"
                          "  \"ratio\": 1.5\n"
                          "}\n");

    const std::map<std::string, std::string> loaded = tempify::load_answer_file(path, false);
    REQUIRE_EQ(loaded.at("ratio"), std::string("1.500000"));
}

TEST_CASE(AnswerFile_missing_file_throws) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-answer-file-missing.json";
    REQUIRE_THROWS_AS(tempify::load_answer_file(path, false), tempify::TempifyError);
}

TEST_CASE(AnswerFile_rejects_invalid_json_and_non_object_payloads) {
    const std::filesystem::path invalid_json_path =
        std::filesystem::temp_directory_path() / "tempify-answer-file-invalid.json";
    write_text_file(invalid_json_path, "{not-json");

    const std::filesystem::path array_path = std::filesystem::temp_directory_path() / "tempify-answer-file-array.json";
    write_text_file(array_path, "[]\n");

    const std::filesystem::path nested_path =
        std::filesystem::temp_directory_path() / "tempify-answer-file-nested.json";
    write_text_file(nested_path, "{\n  \"nested\": {\"a\": 1}\n}\n");

    REQUIRE_THROWS_AS(tempify::load_answer_file(invalid_json_path, false), tempify::TempifyError);
    REQUIRE_THROWS_AS(tempify::load_answer_file(array_path, false), tempify::TempifyError);
    REQUIRE_THROWS_AS(tempify::load_answer_file(nested_path, false), tempify::TempifyError);
}

TEST_CASE(AnswerFile_writes_nested_parent_directories) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-answer-file-nested" / "dir" / "answers.json";
    std::filesystem::remove_all(path.parent_path().parent_path());

    tempify::write_answer_file(path, {{"project_name", "Nested"}});
    REQUIRE(std::filesystem::is_regular_file(path));

    const std::map<std::string, std::string> loaded = tempify::load_answer_file(path, false);
    REQUIRE_EQ(loaded.at("project_name"), std::string("Nested"));
}
