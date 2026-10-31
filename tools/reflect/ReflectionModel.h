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
        // "Color", "Angle" or empty.
        std::string Display;
        SourceLocation Location;
    };

    // A marked struct, a component, or a game's script.
    struct StructDef {
        std::string QualifiedName;
        std::string Name;
        bool IsComponent = false;
        // Components only: can be added by name, as a scene file does.
        bool Addable = true;
        std::vector<PropertyDef> Properties;
        // Include path of the header, as code includes it: "scene/components/LightComponent.h".
        // A game's headers are relative to its scripts folder.
        std::string Include;
        SourceLocation Location;
    };

    struct ParameterDef {
        // The declaration up to the name, with tokens separated by spaces: "const char *".
        std::string Type;
        std::string Name;
    };

    // A function of the SDK marked VKE_API, which a script calls through the engine's table.
    struct ApiFunctionDef {
        // The namespaces around it, from the global one: "vke::Log".
        std::string Namespace;
        std::string Name;
        std::string ReturnType;
        std::vector<ParameterDef> Parameters;
        SourceLocation Location;
    };

    struct ReflectionModel {
        std::vector<std::string> Includes;
        std::vector<EnumDef> Enums;
        std::vector<StructDef> Structs;
        std::vector<ApiFunctionDef> Functions;
        // Follows every token read, so it changes when a header changes in any way but comments.
        std::uint32_t TokenHash = 2166136261u;
    };

    // Thrown for anything the generator refuses, and printed as "file(line): error: message".
    struct ReflectError {
        SourceLocation Location;
        std::string Message;
    };

} // namespace Reflect
