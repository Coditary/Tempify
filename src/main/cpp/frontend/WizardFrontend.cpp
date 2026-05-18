#include "tempify/frontend/WizardFrontend.h"

#include <iostream>
#include <optional>
#include <string>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif

namespace tempify {

namespace {

#if !defined(_WIN32)
using TerminalState = termios;

void restore_terminal_echo(const std::optional<TerminalState>& original) {
    if (original.has_value()) {
        tcsetattr(STDIN_FILENO, TCSANOW, &*original);
        std::cout << '\n';
        std::cout.flush();
    }
}

std::optional<TerminalState> disable_terminal_echo(const bool sensitive) {
    if (!sensitive || ::isatty(STDIN_FILENO) != 1) {
        return std::nullopt;
    }

    termios original{};
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        return std::nullopt;
    }

    termios hidden = original;
    hidden.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &hidden) != 0) {
        return std::nullopt;
    }
    return original;
}
#else
struct TerminalState {};

void restore_terminal_echo(const std::optional<TerminalState>& original) {
    static_cast<void>(original);
}

std::optional<TerminalState> disable_terminal_echo(const bool sensitive) {
    static_cast<void>(sensitive);
    return std::nullopt;
}
#endif

}

std::optional<PromptResult> WizardFrontend::prompt(const std::string& text, const bool sensitive) {
    std::cout << text;
    std::cout.flush();

    const std::optional<TerminalState> original = disable_terminal_echo(sensitive);
    std::string line;
    if (!std::getline(std::cin, line)) {
        restore_terminal_echo(original);
        return std::nullopt;
    }
    restore_terminal_echo(original);

    if (line == ":back") {
        return PromptResult{.action = FrontendAction::Back, .value = {}};
    }
    if (line == ":quit") {
        return PromptResult{.action = FrontendAction::Quit, .value = {}};
    }

    return PromptResult{.action = FrontendAction::Submit, .value = line};
}

void WizardFrontend::write_line(const std::string& text) {
    std::cout << text << '\n';
}

void WizardFrontend::begin_group(const std::string& name, const std::size_t index, const std::size_t total) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "Page " << index << '/' << total;
    if (!name.empty()) {
        std::cout << " | " << name;
    }
    std::cout << "\n";
    std::cout << "Commands: :back  :quit  ? for help\n";
    std::cout << "Press Enter to accept shown default value.\n";
    std::cout << "========================================\n";
}

void WizardFrontend::end_group() {
    std::cout << "----------------------------------------\n";
}

}
