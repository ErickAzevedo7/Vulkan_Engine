#pragma once

#include <cfloat>

// What a game's DLL tells the engine about itself. Nothing here is for scripts: the tables are
// written by the tool that reads the marks. Plain types only, so the two sides don't have to be
// built by the same compiler or in the same configuration.

namespace vke {
    struct Collision;
    class Object;
    class Script;
    enum class StopReason;
} // namespace vke

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
        MeshAsset,
        MaterialAsset,
        SceneAsset,
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
        Script* (*ToScript)(void* script);
        // Whether the type overrides these. The ones that don't aren't called every frame.
        bool UsesUpdate;
        bool UsesLateUpdate;
    };

    enum class ContactPhase : unsigned char {
        Start,
        Stay,
        End,
    };

    // How the engine calls a script of any type.
    struct ScriptCalls {
        void (*SetOwner)(Script* script, Object owner);
        void (*Start)(Script* script);
        void (*Update)(Script* script, float deltaTime);
        void (*LateUpdate)(Script* script, float deltaTime);
        void (*Stop)(Script* script, StopReason reason);
        void (*Collision)(Script* script, ContactPhase phase, const vke::Collision& collision);
        void (*Trigger)(Script* script, ContactPhase phase, Object other);
    };

    struct GameInfo {
        // Stays the first member: it says whether the rest can be read.
        unsigned ApiVersion;
        const ScriptTypeInfo* Scripts;
        unsigned ScriptCount;
        const ScriptCalls* Calls;
    };

    // Reaches what scripts keep private. Partly written by the tool that reads the marks.
    struct ScriptTables {
        static const GameInfo& GetGame();
        static const ScriptCalls& GetCalls();
        template<typename T>
        static constexpr bool UsesUpdate();
        template<typename T>
        static constexpr bool UsesLateUpdate();
    };

} // namespace vke::detail
