#include "TestHarness.h"

#include "tempify/app/TempifyApp.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace {

class ScopedDirectoryCleanup {
public:
    explicit ScopedDirectoryCleanup(std::filesystem::path path)
        : path_(std::move(path)) {
        std::filesystem::remove_all(path_);
    }

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

class ScopedTempifyDataHome {
public:
    explicit ScopedTempifyDataHome(std::filesystem::path path)
        : cleanup_(std::move(path)),
          env_("XDG_DATA_HOME", cleanup_.path().string()) {}

    std::filesystem::path shared_root() const {
        return cleanup_.path() / "tempify";
    }

private:
    ScopedDirectoryCleanup cleanup_;
    prebyte::test::ScopedEnvironmentVariable env_;
};

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

void create_shared_template(const std::filesystem::path& shared_root,
                            const std::string& template_id,
                            const std::string& template_name,
                            const std::string& message) {
    const std::filesystem::path template_root = shared_root / "templates" / template_id;
    std::filesystem::create_directories(template_root / "files");
    write_text_file(template_root / "template.lua",
                    "return {\n"
                    "  id = \"" + template_id + "\",\n"
                    "  name = \"" + template_name + "\",\n"
                    "  version = \"1.0.0\",\n"
                    "  description = \"From shared store\",\n"
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
    write_text_file(template_root / "files" / "README.md.pbt", "# {{ project_name }}\n\n" + message + "\n");
}

}

TEST_CASE(TempifyApp_refresh_indexes_shared_templates_and_renders_them) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-store-render-data-home");
    ScopedDirectoryCleanup output_root(std::filesystem::temp_directory_path() / "tempify-shared-store-render-output");
    create_shared_template(data_home.shared_root(), "shared_cpp", "Shared Template", "Installed from shared store.");

    tempify::TempifyApp app;

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"refresh"}), 0);
        REQUIRE(capture.str().find("Refreshed 1 shared templates") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"list"}), 0);
        REQUIRE(capture.str().find("shared_cpp") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"shared_cpp", "-q", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"shared_cpp\"") != std::string::npos);
        REQUIRE(output.find("\"Project\": [") != std::string::npos);
    }

    {
        ScopedStdoutCapture capture;
        REQUIRE_EQ(app.run({"shared_cpp", "--questions", "--json"}), 0);
        const std::string output = capture.str();
        REQUIRE(output.find("\"shared_cpp\"") != std::string::npos);
        REQUIRE(output.find("\"Project\": [") != std::string::npos);
    }

    const std::filesystem::path generated = output_root.path() / "from-shared";
    REQUIRE_EQ(app.run({
        "shared_cpp",
        generated.string(),
        "--set", "project_name=Shared App",
        "--set", "project_slug=shared-app",
    }), 0);
    REQUIRE(read_text_file(generated / "README.md").find("Installed from shared store.") != std::string::npos);
}

TEST_CASE(TempifyApp_workspace_template_overrides_shared_template_with_same_id) {
    ScopedTempifyDataHome data_home(std::filesystem::temp_directory_path() / "tempify-shared-store-override-data-home");
    ScopedDirectoryCleanup output_root(std::filesystem::temp_directory_path() / "tempify-shared-store-override-output");
    create_shared_template(data_home.shared_root(), "basic_cpp", "Shared Basic", "FROM SHARED STORE");

    tempify::TempifyApp app;
    REQUIRE_EQ(app.run({"refresh"}), 0);

    const std::filesystem::path generated = output_root.path() / "workspace-wins";
    REQUIRE_EQ(app.run({
        "basic_cpp",
        generated.string(),
        "--set", "project_name=Workspace App",
        "--set", "name_slug=workspace-app",
        "--set", "namespace=workspace_ns",
        "--set", "include_ci=false",
        "--set", "author=Tester",
    }), 0);

    const std::string readme = read_text_file(generated / "README.md");
    REQUIRE(readme.find("FROM SHARED STORE") == std::string::npos);
    REQUIRE(readme.find("# Workspace App") != std::string::npos);
}
