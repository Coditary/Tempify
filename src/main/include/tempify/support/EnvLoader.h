#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace tempify {

std::map<std::string, std::string> load_env_file(const std::filesystem::path& path);

}
