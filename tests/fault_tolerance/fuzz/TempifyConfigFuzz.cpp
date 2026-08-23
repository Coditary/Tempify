#include "tempify/config/TempifyConfig.h"
#include "tempify/support/Errors.h"

#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    FuzzTempDir temp_dir;
    const std::filesystem::path path = temp_dir.path() / "config.json";
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
    }

    try {
        (void)tempify::load_tempify_config_file(path);
    } catch (const tempify::TempifyError &) {
    } catch (const std::exception &) {
    }

    return 0;
}
