#include "tempify/question/AnswerFile.h"
#include "tempify/support/Errors.h"

#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    FuzzTempDir temp_dir;
    const std::filesystem::path path = temp_dir.path() / "answers.json";
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
    }

    for (const bool strict : {false, true}) {
        try {
            (void)tempify::load_answer_file(path, strict);
        } catch (const tempify::TempifyError &) {
        } catch (const std::exception &) {
        }
    }

    return 0;
}
