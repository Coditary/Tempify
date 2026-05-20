#include "TempifyAppInternal.h"

#include "tempify/frontend/IQuestionFrontend.h"
#include "tempify/frontend/PlainCliFrontend.h"
#include "tempify/frontend/WizardFrontend.h"
#include "tempify/support/Errors.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <utility>

namespace tempify::app_internal {

namespace {

std::vector<std::filesystem::path> scan_template_roots(const std::filesystem::path& templates_root) {
    std::vector<std::filesystem::path> roots;
    if (!std::filesystem::is_directory(templates_root)) {
        return roots;
    }

    for (const auto& entry : std::filesystem::directory_iterator(templates_root)) {
        if (entry.is_directory()) {
            roots.push_back(entry.path());
        }
    }

    std::ranges::sort(roots);
    return roots;
}

}

TemplateCatalog build_catalog(const std::optional<std::filesystem::path>& workspace_templates_root,
                              const LocalTemplateStore& store,
                              const TemplateLoader& loader) {
    TemplateCatalog catalog;

    if (workspace_templates_root.has_value()) {
        for (const auto& root : scan_template_roots(*workspace_templates_root)) {
            TemplateInfo info;
            try {
                info = loader.summarize(root);
            } catch (const TempifyError&) {
                continue;
            }
            if (catalog.index.contains(info.id)) {
                throw TempifyError("Duplicate workspace template id found: " + info.id);
            }
            catalog.index[info.id] = root;
            catalog.infos.push_back(std::move(info));
        }
    }

    std::set<std::string> shared_ids;
    for (const auto& entry : store.list_templates()) {
        if (!shared_ids.insert(entry.id).second) {
            throw TempifyError("Duplicate shared template id found in index: " + entry.id);
        }

        if (!std::filesystem::is_directory(entry.path) || !std::filesystem::is_regular_file(entry.path / "template.lua")) {
            continue;
        }

        if (catalog.index.contains(entry.id)) {
            continue;
        }

        catalog.index[entry.id] = entry.path;
        catalog.infos.push_back({
            .id = entry.id,
            .name = entry.name,
            .description = entry.description,
            .version = entry.version,
            .root = entry.path,
        });
    }

    std::ranges::sort(catalog.infos, {}, &TemplateInfo::id);
    return catalog;
}

std::filesystem::path resolve_template_root(const CliRequest& request,
                                            const TemplateCatalog& catalog,
                                            const LocalTemplateStore& store) {
    const std::filesystem::path candidate = request.template_ref;
    if (std::filesystem::is_directory(candidate) && std::filesystem::is_regular_file(candidate / "template.lua")) {
        return std::filesystem::absolute(candidate);
    }

    const auto direct = catalog.index.find(request.template_ref);
    if (direct != catalog.index.end()) {
        return direct->second;
    }

    if (const auto shared = store.find_template(request.template_ref)) {
        if (std::filesystem::is_directory(shared->path) && std::filesystem::is_regular_file(shared->path / "template.lua")) {
            return std::filesystem::absolute(shared->path);
        }
        throw TempifyError("Shared template '" + request.template_ref + "' is missing on disk. Run `tempify refresh`.");
    }

    throw TempifyError("Template not found: " + request.template_ref);
}

void print_catalog(const TemplateCatalog& catalog) {
    if (catalog.infos.empty()) {
        std::cout << "No compatible Tempify templates found\n";
        return;
    }

    for (const auto& info : catalog.infos) {
        std::cout << info.id;
        if (!info.name.empty()) {
            std::cout << "\t" << info.name;
        }
        if (!info.description.empty()) {
            std::cout << "\t" << info.description;
        }
        std::cout << '\n';
    }
}

std::unique_ptr<IQuestionFrontend> make_frontend(const bool use_tui) {
    if (use_tui) {
        return std::make_unique<WizardFrontend>();
    }
    return std::make_unique<PlainCliFrontend>();
}

}
