#pragma once

#include "ReflectionModel.h"

#include <map>

#include <string>

namespace Reflect {

    // The C++ source with the property tables for everything in `model`, as the body of the
    // static member ComponentRegistry::<registerFunction>, which registers the components. The
    // member must be declared in scene/ComponentRegistry.h. Throws ReflectError for properties of
    // types the engine can't save.
    std::string WriteCode(const ReflectionModel& model, const std::string& registerFunction);

    // The file a game's DLL is built with: the tables of its scripts and what the engine loads it by.
    std::string WriteGameCode(const ReflectionModel& model);

    // The files that connect the SDK's marked functions to the engine, by file name.
    std::map<std::string, std::string> WriteApiFiles(const ReflectionModel& model);

} // namespace Reflect
