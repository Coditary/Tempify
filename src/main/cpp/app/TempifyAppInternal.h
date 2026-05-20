#pragma once

#include "tempify/domain/CliRequest.h"
#include "tempify/store/AvailableTemplateCache.h"
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

enum class VisibleTemplateStatus {
    Workspace,
    Installed,
    Available,
};

struct VisibleTemplateRecord {
    TemplateInfo info;
    VisibleTemplateStatus status = VisibleTemplateStatus::Available;
    bool installed = false;
    std::optional<AvailableTemplateRecord> available;
};

struct TemplateCatalog {
    std::vector<TemplateInfo> infos;
    std::map<std::string, std::filesystem::path> index;
    std::vector<VisibleTemplateRecord> visible;
    std::map<std::string, AvailableTemplateRecord> available_index;
};

TemplateCatalog build_catalog(const std::optional<std::filesystem::path>& workspace_templates_root,
                              const LocalTemplateStore& store,
                              const AvailableTemplateCache& available_cache,
                              const TemplateLoader& loader);
std::filesystem::path resolve_template_root(const CliRequest& request,
                                            const TemplateCatalog& catalog,
                                            const LocalTemplateStore& store);
void print_catalog(const TemplateCatalog& catalog);
std::unique_ptr<IQuestionFrontend> make_frontend(bool use_tui);

}

}
