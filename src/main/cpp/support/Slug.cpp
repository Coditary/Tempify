#include "tempify/support/Slug.h"

#include <algorithm>
#include <cctype>

namespace tempify {

std::string slugify(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    bool last_was_dash = false;
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            result.push_back(static_cast<char>(std::tolower(ch)));
            last_was_dash = false;
            continue;
        }

        if (!result.empty() && !last_was_dash) {
            result.push_back('-');
            last_was_dash = true;
        }
    }

    while (!result.empty() && result.front() == '-') {
        result.erase(result.begin());
    }
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }

    if (result.empty()) {
        return "app";
    }

    return result;
}

} // namespace tempify
