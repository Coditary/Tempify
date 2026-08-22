#include "tempify/support/Slug.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    const std::string input(reinterpret_cast<const char *>(data), size);
    (void)tempify::slugify(input);
    return 0;
}
