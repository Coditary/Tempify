#pragma once

#include <optional>

namespace tempify {

enum class ExistingPathBehavior {
    Error,
    Overwrite,
    Skip,
};

enum class HookAcceptance {
    Yes,
    Ask,
    No,
};

} // namespace tempify
