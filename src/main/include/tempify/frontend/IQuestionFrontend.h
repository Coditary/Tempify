#pragma once

#include <optional>
#include <string>
#include <vector>

namespace tempify {

enum class FrontendAction {
    Submit,
    Back,
    Quit,
};

struct PromptResult {
    FrontendAction action = FrontendAction::Submit;
    std::string value;
};

class IQuestionFrontend {
public:
    virtual ~IQuestionFrontend() = default;

    virtual std::optional<PromptResult> prompt(const std::string& text, bool sensitive = false) = 0;
    virtual void write_line(const std::string& text) = 0;
    virtual void begin_group(const std::string& name, std::size_t index, std::size_t total) = 0;
    virtual void end_group() = 0;
};

}
