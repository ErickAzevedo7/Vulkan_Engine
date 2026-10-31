#pragma once

#include <cfloat>

// What a game's DLL tells the engine about itself. Nothing here is for scripts: the tables are
// written by the tool that reads the marks. Plain types only, so the two sides don't have to be
// built by the same compiler or in the same configuration.

namespace vke::detail {

    enum class PropertyType : unsigned char {
        Bool,
        I32,
        F32,
        Vec3,
        Quat,
        Enum,
        Struct,
        Object,
        Prefab,
    };

    enum class PropertyDisplay : unsigned char {
        Default,
        Color,
        Angle,
    };

    inline constexpr float kNoMinimum = -FLT_MAX;
    inline constexpr float kNoMaximum = FLT_MAX;

    struct EnumValue {
        const char* Name;
        unsigned Value;
    };

    struct EnumInfo {
        const char* Name;
        const EnumValue* Values;
        unsigned Count;
        unsigned Size;
    };

    struct PropertyInfo;

    struct StructInfo {
        const char* Name;
        const PropertyInfo* Properties;
        unsigned Count;
    };

    struct PropertyInfo {
        const char* Name;
        PropertyType Type;
        // The field, inside an object of the type the StructInfo describes.
        void* (*Access)(void* object);
        const EnumInfo* Enum;
        const StructInfo* Struct;
        float Min;
        float Max;
        PropertyDisplay Display;
    };

    struct ScriptTypeInfo {
        const char* Name;
        const StructInfo* Properties;
        void* (*Create)();
        void (*Destroy)(void* script);
    };

    struct GameInfo {
        // Stays the first member: it says whether the rest can be read.
        unsigned ApiVersion;
        const ScriptTypeInfo* Scripts;
        unsigned ScriptCount;
    };

    // Written by the tool, which is why every script befriends it.
    struct ScriptTables {
        static const GameInfo& GetGame();
    };

} // namespace vke::detail
