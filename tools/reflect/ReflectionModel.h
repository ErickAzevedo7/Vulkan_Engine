#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Reflect {

    // Where something was found, for error messages.
    struct SourceLocation {
        std::string File;
        int Line = 0;
    };

    struct EnumDef {
        // As C++ names it, from the global namespace: "VKE::Renderer::LightType".
        std::string QualifiedName;
        std::string Name;
        std::vector<std::pair<std::string, std::uint32_t>> Values;
        SourceLocation Location;
    };

    struct PropertyDef {
        std::string Member;
        // What files and editors call it.
        std::string SavedName;
        // The declared type, with spaces removed.
        std::string TypeText;
        // Number literals as written, or empty.
        std::string Min;
        std::string Max;
        SourceLocation Location;
    };

    // A marked struct, or a component.
    struct StructDef {
        std::string QualifiedName;
        std::string Name;
        bool IsComponent = false;
        // Components only: can be added by name, as a scene file does.
        bool Addable = true;
        std::vector<PropertyDef> Properties;
        // Include path of the header, as code includes it: "scene/components/LightComponent.h".
        std::string Include;
        SourceLocation Location;
    };

    struct ReflectionModel {
        std::vector<std::string> Includes;
        std::vector<EnumDef> Enums;
        std::vector<StructDef> Structs;
    };

    // Thrown for anything the generator refuses, and printed as "file(line): error: message".
    struct ReflectError {
        SourceLocation Location;
        std::string Message;
    };

} // namespace Reflect
