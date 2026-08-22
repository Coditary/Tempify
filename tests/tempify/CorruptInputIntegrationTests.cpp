#include "TempifyAppTestSupport.h"
#include "tempify/build/GenerationLock.h"
#include "tempify/question/AnswerFile.h"
#include "tempify/support/Errors.h"

#include <filesystem>
#include <string>

namespace {

using tempify::test_support::read_text_file;
using tempify::test_support::ScopedDirectoryCleanup;
using tempify::test_support::ScopedStdoutCapture;
using tempify::test_support::ScopedTempifyDataHome;
using tempify::test_support::write_text_file;

std::filesystem::path answers_path(const char *label) {
    return std::filesystem::temp_directory_path() / ("tempify-corrupt-answers-" + std::string(label) + ".json");
}

std::filesystem::path lock_path(const std::filesystem::path &target) {
    return target / ".tempify-lock.json";
}

void render_basic_cpp_target(tempify::TempifyApp &app, const std::filesystem::path &target) {
    REQUIRE_EQ(app.run({
                   "basic_cpp",
                   target.string(),
                   "--set",
                   "project_name=Corrupt Input App",
                   "--set",
                   "name_slug=corrupt-input-app",
                   "--set",
                   "namespace=corrupt_input_ns",
                   "--set",
                   "include_ci=false",
                   "--set",
                   "author=Corrupt Input Tester",
               }),
               0);
}

std::vector<std::string> basic_cpp_render_args(const std::filesystem::path &target) {
    return {
        "basic_cpp", target.string(),
        "--set",     "project_name=Corrupt Input App",
        "--set",     "name_slug=corrupt-input-app",
        "--set",     "namespace=corrupt_input_ns",
        "--set",     "include_ci=false",
        "--set",     "author=Corrupt Input Tester",
    };
}

} // namespace

TEST_CASE(load_answer_file_rejects_malformed_json) {
    const std::filesystem::path path = answers_path("malformed-json");
    write_text_file(path, "{ not valid json\n");

    try {
        static_cast<void>(tempify::load_answer_file(path, false));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Could not parse answer file") != std::string::npos);
    }
}

TEST_CASE(load_answer_file_rejects_non_object_root) {
    const std::filesystem::path path = answers_path("non-object-root");
    write_text_file(path, "[\"project_name\"]\n");

    try {
        static_cast<void>(tempify::load_answer_file(path, false));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Answer file must be JSON object") != std::string::npos);
    }
}

TEST_CASE(load_answer_file_rejects_non_scalar_values) {
    const std::filesystem::path path = answers_path("non-scalar-value");
    write_text_file(path, "{\n  \"project_name\": { \"nested\": true }\n}\n");

    try {
        static_cast<void>(tempify::load_answer_file(path, false));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("must be scalar") != std::string::npos);
    }
}

TEST_CASE(load_answer_file_reports_missing_file) {
    const std::filesystem::path path = answers_path("missing-file");
    std::filesystem::remove(path);

    try {
        static_cast<void>(tempify::load_answer_file(path, false));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Answer file not found") != std::string::npos);
    }
}

TEST_CASE(load_generation_lock_rejects_malformed_json) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-corrupt-lock-malformed.json";
    write_text_file(path, "{ broken lock\n");

    try {
        static_cast<void>(tempify::load_generation_lock(path));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Could not parse lock file") != std::string::npos);
    }
}

TEST_CASE(load_generation_lock_rejects_non_object_root) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tempify-corrupt-lock-array.json";
    write_text_file(path, "[]\n");

    try {
        static_cast<void>(tempify::load_generation_lock(path));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Lock file must be JSON object") != std::string::npos);
    }
}

TEST_CASE(load_generation_lock_rejects_invalid_managed_files_type) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-corrupt-lock-managed-files.json";
    write_text_file(path, "{\n  \"managed_files\": \"not-an-array\"\n}\n");

    try {
        static_cast<void>(tempify::load_generation_lock(path));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("managed_files") != std::string::npos);
        REQUIRE(std::string(error.what()).find("must be array") != std::string::npos);
    }
}

TEST_CASE(load_generation_lock_rejects_invalid_template_field_type) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-corrupt-lock-template-field.json";
    write_text_file(path, "{\n  \"template\": \"not-an-object\"\n}\n");

    try {
        static_cast<void>(tempify::load_generation_lock(path));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("template") != std::string::npos);
        REQUIRE(std::string(error.what()).find("must be object") != std::string::npos);
    }
}

TEST_CASE(load_generation_lock_rejects_invalid_managed_file_hashes_value_type) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tempify-corrupt-lock-managed-hashes.json";
    write_text_file(path, "{\n  \"managed_file_hashes\": { \"README.md\": 123 }\n}\n");

    try {
        static_cast<void>(tempify::load_generation_lock(path));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("managed_file_hashes") != std::string::npos);
        REQUIRE(std::string(error.what()).find("must contain only string values") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_diff_rejects_corrupt_lock_file) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-diff-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-diff-data-home");
    tempify::TempifyApp app;
    render_basic_cpp_target(app, target.path());
    write_text_file(lock_path(target.path()), "{ broken lock file\n");

    try {
        auto args = basic_cpp_render_args(target.path());
        args.push_back("--diff");
        static_cast<void>(app.run(args));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Could not parse lock file") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_reapply_rejects_corrupt_lock_file) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-reapply-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-lock-reapply-data-home");
    tempify::TempifyApp app;
    render_basic_cpp_target(app, target.path());
    write_text_file(lock_path(target.path()), "{ broken lock file\n");

    try {
        auto args = basic_cpp_render_args(target.path());
        args.push_back("--reapply");
        static_cast<void>(app.run(args));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Could not parse lock file") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_render_rejects_corrupt_answers_file) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-render-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-corrupt-answers-render-data-home");
    const std::filesystem::path answers_file = answers_path("render-corrupt");
    write_text_file(answers_file, "{ broken answers\n");

    tempify::TempifyApp app;
    try {
        static_cast<void>(app.run({
            "basic_cpp",
            target.path().string(),
            "--answers",
            answers_file.string(),
            "--set",
            "author=Corrupt Input Tester",
            "--non-interactive",
        }));
        REQUIRE(false);
    } catch (const tempify::TempifyError &error) {
        REQUIRE(std::string(error.what()).find("Could not parse answer file") != std::string::npos);
    }
}

TEST_CASE(TempifyApp_render_rejects_answers_file_with_nested_object_value) {
    ScopedDirectoryCleanup target(std::filesystem::temp_directory_path() / "tempify-corrupt-answers-nested-target");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() /
                                    "tempify-corrupt-answers-nested-data-home");
    const std::filesystem::path answers_file = answers_path("render-nested");
    write_text_file(answers_file, "{\n  \"project_name\": { \"nested\": true }\n}\n");

    tempify::TempifyApp app;
    REQUIRE_THROWS_AS(app.run({
                          "basic_cpp",
                          target.path().string(),
                          "--answers",
                          answers_file.string(),
                          "--set",
                          "author=Corrupt Input Tester",
                          "--non-interactive",
                      }),
                      tempify::TempifyError);
}

TEST_CASE(TempifyApp_list_fails_when_available_cache_json_is_corrupt) {
    ScopedDirectoryCleanup workspace(std::filesystem::temp_directory_path() / "tempify-corrupt-cache-list-workspace");
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-corrupt-cache-list-data-home");
    tempify::test_support::link_test_templates_into_workspace(workspace.path());
    const std::filesystem::path cache_file = data_home.shared_root() / "index" / "reqpack-available.json";
    std::filesystem::create_directories(cache_file.parent_path());
    write_text_file(cache_file, "{ broken cache\n");

    tempify::TempifyApp app;
    try {
        static_cast<void>(app.run({"list"}));
        REQUIRE(false);
    } catch (const std::exception &error) {
        REQUIRE(std::string(error.what()).find("JSON") != std::string::npos);
    }
}
