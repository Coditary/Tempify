#pragma once

#include "tempify/domain/TemplateManifest.h"

namespace tempify {

class TemplateValidator {
public:
    void validate(const TemplateManifest& manifest) const;
};

}
