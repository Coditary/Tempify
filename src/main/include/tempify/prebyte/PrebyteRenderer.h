#pragma once

#include "PrebyteEngine.h"
#include "tempify/domain/TemplateManifest.h"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace tempify {

class PrebyteRenderer {
  public:
    void configure(prebyte::Prebyte &engine, const std::map<std::string, std::string> &values,
                   const TemplateManifest &manifest) const;

    std::string render_string(const prebyte::Prebyte &engine, std::string_view input) const;
    void render_file(const prebyte::Prebyte &engine, const std::filesystem::path &input_path,
                     const std::filesystem::path &output_path) const;
};

} // namespace tempify
