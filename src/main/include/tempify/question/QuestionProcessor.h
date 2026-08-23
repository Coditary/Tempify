#pragma once

#include "tempify/domain/TemplateManifest.h"

#include <map>
#include <string>

namespace tempify {

class IQuestionFrontend;
class LuaEngine;

class QuestionProcessor {
  public:
    QuestionProcessor(const LuaEngine &lua_engine, IQuestionFrontend &frontend);

    std::map<std::string, std::string> collect(const TemplateManifest &manifest,
                                               const std::map<std::string, std::string> &cli_values,
                                               const std::map<std::string, std::string> &config_values = {},
                                               const std::map<std::string, std::string> &imported_values = {},
                                               bool non_interactive = false, bool strict = false,
                                               bool review_before_finish = false) const;

  private:
    const LuaEngine &lua_engine_;
    IQuestionFrontend &frontend_;
};

} // namespace tempify
