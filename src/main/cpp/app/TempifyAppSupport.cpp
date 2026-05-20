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

bool visible_record_beats(const tempify::app_internal::VisibleTemplateRecord& candidate,
                          const tempify::app_internal::VisibleTemplateRecord& current) {
    using tempify::app_internal::VisibleTemplateStatus;

    auto rank = [](const VisibleTemplateStatus status) {
        switch (status) {
        case VisibleTemplateStatus::Workspace:
            return 3;
        case VisibleTemplateStatus::Installed:
            return 2;
        case VisibleTemplateStatus::Available:
            return 1;
        }
        return 0;
    };

    return rank(candidate.status) > rank(current.status);
}

std::vector<std::filesystem::path> scan_template_roots(const std::filesystem::path& templates_root) {
    std::vector<std::filesystem::path> roots;
    std::error_code error;
    if (!std::filesystem::is_directory(templates_root, error)) {
        return roots;
    }

    for (const auto& entry : std::filesystem::directory_iterator(templates_root, error)) {
        if (error) {
            break;
        }
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
                              const AvailableTemplateCache& available_cache,
                              const TemplateLoader& loader) {
    TemplateCatalog catalog;
    std::map<std::string, VisibleTemplateRecord> visible_by_id;

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
            const TemplateInfo& stored = catalog.infos.back();
            visible_by_id[stored.id] = VisibleTemplateRecord{
                .info = stored,
                .status = VisibleTemplateStatus::Workspace,
                .installed = true,
            };
        }
    }

    std::set<std::string> shared_ids;
    for (const auto& entry : store.list_templates()) {
        if (!shared_ids.insert(entry.id).second) {
            throw TempifyError("Duplicate shared template id found in index: " + entry.id);
        }

        std::error_code error;
        if (!std::filesystem::is_directory(entry.path, error) || !std::filesystem::is_regular_file(entry.path / "template.lua", error)) {
            continue;
        }

        if (catalog.index.contains(entry.id)) {
            continue;
        }

        TemplateInfo info{
            .id = entry.id,
            .name = entry.name,
            .description = entry.description,
            .version = entry.version,
            .root = entry.path,
        };
        catalog.index[entry.id] = entry.path;
        catalog.infos.push_back(info);
        visible_by_id[entry.id] = VisibleTemplateRecord{
            .info = info,
            .status = VisibleTemplateStatus::Installed,
            .installed = true,
        };
    }

    for (const auto& entry : available_cache.list_templates()) {
        catalog.available_index[entry.id] = entry;
        VisibleTemplateRecord candidate{
            .info = TemplateInfo{
                .id = entry.id,
                .name = entry.name,
                .description = entry.description,
                .version = entry.version,
                .root = {},
            },
            .status = VisibleTemplateStatus::Available,
            .installed = false,
            .available = entry,
        };

        const auto existing = visible_by_id.find(entry.id);
        if (existing == visible_by_id.end() || visible_record_beats(candidate, existing->second)) {
            visible_by_id[entry.id] = std::move(candidate);
        } else if (!existing->second.available.has_value()) {
            existing->second.available = entry;
        }
    }

    std::ranges::sort(catalog.infos, {}, &TemplateInfo::id);
    for (const auto& [id, record] : visible_by_id) {
        static_cast<void>(id);
        catalog.visible.push_back(record);
    }
    std::ranges::sort(catalog.visible, {}, [](const VisibleTemplateRecord& record) {
        return record.info.id;
    });
    return catalog;
}

std::filesystem::path resolve_template_root(const CliRequest& request,
                                            const TemplateCatalog& catalog,
                                            const LocalTemplateStore& store) {
    const std::filesystem::path candidate = request.template_ref;
    std::error_code error;
    if (std::filesystem::is_directory(candidate, error) && std::filesystem::is_regular_file(candidate / "template.lua", error)) {
        return std::filesystem::absolute(candidate);
    }

    const auto direct = catalog.index.find(request.template_ref);
    if (direct != catalog.index.end()) {
        return direct->second;
    }

    if (const auto shared = store.find_template(request.template_ref)) {
        std::error_code shared_error;
        if (std::filesystem::is_directory(shared->path, shared_error)
            && std::filesystem::is_regular_file(shared->path / "template.lua", shared_error)) {
            return std::filesystem::absolute(shared->path);
        }
        throw TempifyError("Shared template '" + request.template_ref + "' is missing on disk. Run `tempify refresh`.");
    }

    throw TempifyError("Template not found: " + request.template_ref);
}

void print_catalog(const TemplateCatalog& catalog) {
    if (catalog.visible.empty()) {
        std::cout << "No compatible Tempify templates found\n";
        return;
    }

    for (const auto& record : catalog.visible) {
        const auto& info = record.info;
        std::cout << info.id;
        if (!info.name.empty()) {
            std::cout << "\t" << info.name;
        }
        if (!info.description.empty()) {
            std::cout << "\t" << info.description;
        }
        std::cout << "\t";
        switch (record.status) {
        case VisibleTemplateStatus::Workspace:
            std::cout << "workspace";
            break;
        case VisibleTemplateStatus::Installed:
            std::cout << "installed";
            break;
        case VisibleTemplateStatus::Available:
            std::cout << "available";
            break;
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
