#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace tempify {

std::map<std::string, std::string> load_answer_file(const std::filesystem::path& path,
                                                    bool strict);
void write_answer_file(const std::filesystem::path& path,
                       const std::map<std::string, std::string>& values);

}
