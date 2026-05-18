#pragma once

#include "tempify/frontend/IQuestionFrontend.h"

namespace tempify {

class WizardFrontend final : public IQuestionFrontend {
public:
    std::optional<PromptResult> prompt(const std::string& text, bool sensitive = false) override;
    void write_line(const std::string& text) override;
    void begin_group(const std::string& name, std::size_t index, std::size_t total) override;
    void end_group() override;
};

}
