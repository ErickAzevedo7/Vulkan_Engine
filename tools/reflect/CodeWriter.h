#pragma once

#include "ReflectionModel.h"

#include <string>

namespace Reflect {

    // The C++ source with the property tables for everything in `model`, as the body of the
    // static member ComponentRegistry::<registerFunction>, which registers the components. The
    // member must be declared in scene/ComponentRegistry.h. Throws ReflectError for properties of
    // types the engine can't save.
    std::string WriteCode(const ReflectionModel& model, const std::string& registerFunction);

} // namespace Reflect
