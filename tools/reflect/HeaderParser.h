#pragma once

#include "ReflectionModel.h"

#include <filesystem>

namespace Reflect {

    // Reads the marked enums, structs and components of one header into `model`. Throws
    // ReflectError for marks it can't read.
    //
    // It doesn't parse C++: it splits the file into tokens, follows namespaces and classes by their
    // braces, and only reads closely what comes right after a mark.
    void ParseHeader(const std::filesystem::path& file, const std::filesystem::path& includeRoot,
                     ReflectionModel& model);

} // namespace Reflect
