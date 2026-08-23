#include "TestHarness.h"
#include "tempify/frontend/PlainCliFrontend.h"
#include "tempify/frontend/WizardFrontend.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

class ScopedStreamRedirect {
  public:
    ScopedStreamRedirect(std::ios &stream, std::streambuf *buffer) : stream_(stream), previous_(stream.rdbuf(buffer)) {}
    ~ScopedStreamRedirect() {
        stream_.rdbuf(previous_);
    }

  private:
    std::ios &stream_;
    std::streambuf *previous_;
};

} // namespace

TEST_CASE(PlainCliFrontend_prompt_handles_submit_back_quit_and_eof) {
    tempify::PlainCliFrontend frontend;

    {
        std::istringstream input("alpha\n");
        std::ostringstream output;
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        ScopedStreamRedirect cout_redirect(std::cout, output.rdbuf());

        const auto result = frontend.prompt("Name: ", false);
        const auto &prompt_result = REQUIRE_VALUE(result);
        REQUIRE_EQ(static_cast<int>(prompt_result.action), static_cast<int>(tempify::FrontendAction::Submit));
        REQUIRE_EQ(prompt_result.value, std::string("alpha"));
        REQUIRE(output.str().find("Name: ") != std::string::npos);
    }

    {
        std::istringstream input(":back\n");
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        const auto result = frontend.prompt("Choice: ", false);
        const auto &prompt_result = REQUIRE_VALUE(result);
        REQUIRE_EQ(static_cast<int>(prompt_result.action), static_cast<int>(tempify::FrontendAction::Back));
    }

    {
        std::istringstream input(":quit\n");
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        const auto result = frontend.prompt("Choice: ", false);
        const auto &prompt_result = REQUIRE_VALUE(result);
        REQUIRE_EQ(static_cast<int>(prompt_result.action), static_cast<int>(tempify::FrontendAction::Quit));
    }

    {
        std::istringstream input;
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        const auto result = frontend.prompt("Name: ", false);
        REQUIRE(!result.has_value());
    }

    {
        std::ostringstream output;
        ScopedStreamRedirect cout_redirect(std::cout, output.rdbuf());
        frontend.write_line("status line");
        frontend.begin_group("Project", 1, 2);
        frontend.begin_group("", 0, 0);
        frontend.end_group();
        REQUIRE(output.str().find("status line") != std::string::npos);
        REQUIRE(output.str().find("[Project]") != std::string::npos);
    }

    {
        std::istringstream input("secret\n");
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        const auto result = frontend.prompt("Password: ", true);
        const auto &prompt_result = REQUIRE_VALUE(result);
        REQUIRE_EQ(prompt_result.value, std::string("secret"));
    }
}

TEST_CASE(WizardFrontend_renders_group_headers_and_prompt_actions) {
    tempify::WizardFrontend frontend;

    {
        std::ostringstream output;
        ScopedStreamRedirect cout_redirect(std::cout, output.rdbuf());
        frontend.begin_group("Setup", 2, 5);
        frontend.end_group();
        REQUIRE(output.str().find("Page 2/5") != std::string::npos);
        REQUIRE(output.str().find("Setup") != std::string::npos);
        REQUIRE(output.str().find("----------------------------------------") != std::string::npos);
    }

    {
        std::istringstream input("value\n");
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        const auto result = frontend.prompt("Field: ", false);
        const auto &prompt_result = REQUIRE_VALUE(result);
        REQUIRE_EQ(static_cast<int>(prompt_result.action), static_cast<int>(tempify::FrontendAction::Submit));
        REQUIRE_EQ(prompt_result.value, std::string("value"));
    }

    {
        std::istringstream input(":back\n");
        ScopedStreamRedirect cin_redirect(std::cin, input.rdbuf());
        const auto result = frontend.prompt("Field: ", false);
        const auto &prompt_result = REQUIRE_VALUE(result);
        REQUIRE_EQ(static_cast<int>(prompt_result.action), static_cast<int>(tempify::FrontendAction::Back));
    }

    {
        std::ostringstream output;
        ScopedStreamRedirect cout_redirect(std::cout, output.rdbuf());
        frontend.begin_group("", 1, 1);
        frontend.write_line("wizard line");
        REQUIRE(output.str().find("Page 1/1") != std::string::npos);
        REQUIRE(output.str().find("wizard line") != std::string::npos);
    }
}
