#pragma once

#include "TestHarness.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tempify::test_support {

inline const std::filesystem::path& project_root() {
    static const std::filesystem::path value = std::filesystem::path(TEMPIFY_SOURCE_DIR);
    return value;
}

inline const std::filesystem::path& tests_root() {
    static const std::filesystem::path value = project_root() / "tests";
    return value;
}

inline const std::filesystem::path& test_workspace_root() {
    static const std::filesystem::path value = tests_root() / "test_workspace";
    return value;
}

inline const std::filesystem::path& test_templates_root() {
    static const std::filesystem::path value = tests_root() / "test_templates";
    return value;
}

inline void ensure_test_workspace_layout() {
    std::filesystem::create_directories(test_workspace_root());
    std::error_code error;
    std::filesystem::remove(test_workspace_root() / "templates", error);
    error.clear();
    std::filesystem::create_directory_symlink(test_templates_root(), test_workspace_root() / "templates");
}

inline std::filesystem::path test_template_path(const std::string_view template_id) {
    return test_templates_root() / std::string(template_id);
}

class ScopedDirectoryCleanup {
public:
    explicit ScopedDirectoryCleanup(std::filesystem::path path)
        : path_(std::move(path)) {
        std::filesystem::remove_all(path_);
    }

    ScopedDirectoryCleanup(const ScopedDirectoryCleanup&) = delete;
    ScopedDirectoryCleanup& operator=(const ScopedDirectoryCleanup&) = delete;

    ~ScopedDirectoryCleanup() {
        std::filesystem::remove_all(path_);
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

class ScopedStdoutCapture {
public:
    ScopedStdoutCapture()
        : previous_(std::cout.rdbuf(buffer_.rdbuf())) {}

    ScopedStdoutCapture(const ScopedStdoutCapture&) = delete;
    ScopedStdoutCapture& operator=(const ScopedStdoutCapture&) = delete;

    ~ScopedStdoutCapture() {
        std::cout.rdbuf(previous_);
    }

    std::string str() const {
        return buffer_.str();
    }

private:
    std::ostringstream buffer_;
    std::streambuf* previous_ = nullptr;
};

class ScopedStdinCapture {
public:
    explicit ScopedStdinCapture(std::string text)
        : buffer_(std::move(text)),
          previous_(std::cin.rdbuf(buffer_.rdbuf())) {}

    ScopedStdinCapture(const ScopedStdinCapture&) = delete;
    ScopedStdinCapture& operator=(const ScopedStdinCapture&) = delete;

    ~ScopedStdinCapture() {
        std::cin.rdbuf(previous_);
    }

private:
    std::istringstream buffer_;
    std::streambuf* previous_ = nullptr;
};

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& path)
        : previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(path);
    }

    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;

    ~ScopedCurrentPath() {
        std::filesystem::current_path(previous_);
    }

private:
    std::filesystem::path previous_;
};

class ScopedTestWorkspace final : public ScopedCurrentPath {
public:
    ScopedTestWorkspace()
        : ScopedCurrentPath((ensure_test_workspace_layout(), test_workspace_root())) {}
};

inline std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

inline void write_text_file(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary);
    output << text;
}

inline std::filesystem::path tempify_binary_path() {
    if (const char* binary = std::getenv("TEMPIFY_TEST_BINARY"); binary != nullptr && binary[0] != '\0') {
        return binary;
    }

    std::filesystem::path fallback = project_root() / "build" / "tempify";
#ifdef _WIN32
    fallback += ".exe";
#endif
    return fallback;
}

class ScopedTempifyDataHome {
public:
    explicit ScopedTempifyDataHome(std::filesystem::path path)
        : cleanup_(std::move(path)),
          env_("XDG_DATA_HOME", cleanup_.path().string()) {}

    ScopedTempifyDataHome(const ScopedTempifyDataHome&) = delete;
    ScopedTempifyDataHome& operator=(const ScopedTempifyDataHome&) = delete;

    const std::filesystem::path& path() const noexcept {
        return cleanup_.path();
    }

    std::filesystem::path shared_root() const {
        return cleanup_.path() / "tempify";
    }

private:
    ScopedDirectoryCleanup cleanup_;
    prebyte::test::ScopedEnvironmentVariable env_;
};

class ScopedTempifyConfigHome {
public:
    explicit ScopedTempifyConfigHome(std::filesystem::path path)
        : cleanup_(std::move(path)),
          env_("XDG_CONFIG_HOME", cleanup_.path().string()) {}

    ScopedTempifyConfigHome(const ScopedTempifyConfigHome&) = delete;
    ScopedTempifyConfigHome& operator=(const ScopedTempifyConfigHome&) = delete;

    const std::filesystem::path& path() const noexcept {
        return cleanup_.path();
    }

private:
    ScopedDirectoryCleanup cleanup_;
    prebyte::test::ScopedEnvironmentVariable env_;
};

inline void link_test_templates_into_workspace(const std::filesystem::path& workspace_root) {
    std::filesystem::create_directories(workspace_root);
    std::error_code error;
    std::filesystem::remove(workspace_root / "templates", error);
    error.clear();
    std::filesystem::create_directory_symlink(test_templates_root(), workspace_root / "templates");
}

inline std::filesystem::path create_required_only_template_with_version(const std::filesystem::path& root,
                                                                        const std::string& version) {
    const std::filesystem::path template_root = root / "required_only";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"required_only\",\n"
                    "  name = \"Required Only\",\n"
                    "  version = \"" + version + "\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ required_name }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"required_name\", type = \"string\", prompt = \"Required name\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ required_name }}\n");
    return template_root;
}

inline std::filesystem::path create_required_only_template(const std::filesystem::path& root) {
    return create_required_only_template_with_version(root, "1.0.0");
}

inline std::filesystem::path create_duplicate_alias_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "duplicate_alias";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"duplicate_alias\",\n"
                    "  name = \"Duplicate Alias\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"out\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"project_name\", type = \"string\", alias = { \"slug\" } },\n"
                    "      { key = \"slug\", type = \"string\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# hi\n");
    return template_root;
}

inline std::filesystem::path create_bad_layout_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "bad_layout";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"bad_layout\",\n"
                    "  name = \"Bad Layout\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"out\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "layout.lua",
                    "return {\n"
                    "  { source = \"missing.txt\", target = \"renamed.txt\" },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# hi\n");
    return template_root;
}

inline std::filesystem::path create_sensitive_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "sensitive_demo";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"sensitive_demo\",\n"
                    "  name = \"Sensitive Demo\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ project_name }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"Secrets\" },\n"
                    "  groups = {\n"
                    "    Secrets = {\n"
                    "      { key = \"project_name\", type = \"string\", prompt = \"Project name\" },\n"
                    "      { key = \"api_token\", type = \"string\", prompt = \"API token\", sensitive = true },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n");
    return template_root;
}

inline std::filesystem::path create_slow_hook_template(const std::filesystem::path& root) {
    const std::filesystem::path template_root = root / "slow_hook_demo";
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"slow_hook_demo\",\n"
                    "  name = \"Slow Hook Demo\",\n"
                    "  version = \"1.0.0\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ project_name }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"General\" },\n"
                    "  groups = {\n"
                    "    General = {\n"
                    "      { key = \"project_name\", type = \"string\", prompt = \"Project name\" },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n");
    write_text_file(template_root / "hooks" / "post.lua", "while true do end\n");
    return template_root;
}

inline std::filesystem::path create_basic_template_at(const std::filesystem::path& template_root,
                                                      const std::string& template_id,
                                                      const std::string& template_name,
                                                      const std::string& version,
                                                      const std::string& description,
                                                      const std::string& readme_message = "Installed from shared store.") {
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"" + template_id + "\",\n"
                    "  name = \"" + template_name + "\",\n"
                    "  version = \"" + version + "\",\n"
                    "  description = \"" + description + "\",\n"
                    "  source_dir = \"files\",\n"
                    "  output = { path = \"{{ project_slug }}\", overwrite = false },\n"
                    "}\n");
    write_text_file(template_root / "questions.lua",
                    "return {\n"
                    "  order = { \"Project\" },\n"
                    "  groups = {\n"
                    "    Project = {\n"
                    "      { key = \"project_name\", type = \"string\", prompt = \"Project name\", default = \"Shared App\" },\n"
                    "      { key = \"project_slug\", type = \"string\", default = function(ctx) return slugify(ctx.values.project_name or \"shared-app\") end },\n"
                    "    },\n"
                    "  },\n"
                    "}\n");
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n\n" + readme_message + "\n");
    return template_root;
}

inline void create_shared_template(const std::filesystem::path& shared_root,
                                   const std::string& template_id,
                                   const std::string& template_name,
                                   const std::string& version,
                                   const std::string& description,
                                   const std::string& readme_message = "Installed from shared store.") {
    static_cast<void>(create_basic_template_at(shared_root / "templates" / template_id,
                                               template_id,
                                               template_name,
                                               version,
                                               description,
                                               readme_message));
}

}
