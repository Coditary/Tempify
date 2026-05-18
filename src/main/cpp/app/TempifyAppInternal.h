#pragma once

#include "tempify/domain/CliRequest.h"
#include "tempify/store/LocalTemplateStore.h"
#include "tempify/template/TemplateLoader.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tempify {

class IQuestionFrontend;

namespace app_internal {

struct TemplateCatalog {
    std::vector<TemplateInfo> infos;
    std::map<std::string, std::filesystem::path> index;
};

TemplateCatalog build_catalog(const std::optional<std::filesystem::path>& workspace_templates_root,
                              const LocalTemplateStore& store,
                              const TemplateLoader& loader);
std::filesystem::path resolve_template_root(const CliRequest& request,
                                            const TemplateCatalog& catalog,
                                            const LocalTemplateStore& store);
void print_catalog(const TemplateCatalog& catalog);
std::unique_ptr<IQuestionFrontend> make_frontend(bool use_tui);

}

}
