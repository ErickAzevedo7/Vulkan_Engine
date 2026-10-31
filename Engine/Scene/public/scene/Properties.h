#pragma once

#include <core/Types.h>

#include <cfloat>

namespace VKE::Scene {

    // Descriptions of the fields of components and marked structs, written by the reflection
    // generator from the marks in reflection/Marks.h. Savers and editors go through them instead
    // of knowing each type.

    enum class PropertyType : u8 {
        Bool,
        // Only game scripts have these three.
        I32,
        // The ObjectID of an object. In a prefab's instance, that of the part in the prefab
        // when it points inside the instance: see PrefabLibrary::FindReferenced.
        Object,
        // The AssetID of a prefab.
        Prefab,
        F32,
        Vec3,
        Quat,
        // A marked enum, described by PropertyInfo::Enum.
        Enum,
        // A marked struct, described by PropertyInfo::Struct.
        Struct,
        // Handle<Assets::MeshData>.
        Mesh,
        // std::vector<Handle<Assets::MaterialData>>.
        Materials,
    };

    struct EnumValue {
        const char* Name;
        u32 Value;
    };

    struct EnumInfo {
        const char* Name;
        const EnumValue* Values;
        u32 Count;
        // Bytes the enum takes, which its underlying type decides.
        u32 Size;

        u32 Read(const void* value) const {
            switch (Size) {
            case 1: return *static_cast<const u8*>(value);
            case 2: return *static_cast<const u16*>(value);
            default: return *static_cast<const u32*>(value);
            }
        }

        void Write(void* value, u32 number) const {
            switch (Size) {
            case 1: *static_cast<u8*>(value) = static_cast<u8>(number); break;
            case 2: *static_cast<u16*>(value) = static_cast<u16>(number); break;
            default: *static_cast<u32*>(value) = number; break;
            }
        }
    };

    struct PropertyInfo;

    // The properties of a component type or a marked struct.
    struct StructInfo {
        const char* Name;
        const PropertyInfo* Properties;
        u32 Count;

        const PropertyInfo* begin() const;
        const PropertyInfo* end() const;
    };

    // How editors show a property, beyond its type.
    enum class PropertyDisplay : u8 {
        Default,
        // A Vec3 holding a linear RGB color.
        Color,
        // An f32 holding an angle in radians, shown in degrees.
        Angle,
    };

    constexpr f32 kNoMinimum = -FLT_MAX;
    constexpr f32 kNoMaximum = FLT_MAX;

    struct PropertyInfo {
        // What files and editors call it.
        const char* Name;
        PropertyType Type;
        // The field, inside an object of the type the StructInfo describes.
        void* (*Access)(void* object);
        const EnumInfo* Enum;
        const StructInfo* Struct;
        // For editors: f32 properties are kept between these. Loading doesn't clamp.
        f32 Min;
        f32 Max;
        PropertyDisplay Display;

        void* Get(void* object) const { return Access(object); }
        // Access only takes writable objects; reading through it leaves them as they are.
        const void* Get(const void* object) const { return Access(const_cast<void*>(object)); }
    };

    inline const PropertyInfo* StructInfo::begin() const {
        return Properties;
    }

    inline const PropertyInfo* StructInfo::end() const {
        return Properties + Count;
    }

    // Compare and copy the values of properties, given as what PropertyInfo::Get returns for
    // two objects of the same type, or whole objects of a type.
    bool AreEqual(const PropertyInfo& property, const void* a, const void* b);
    bool AreEqual(const StructInfo& type, const void* a, const void* b);
    void CopyValue(const PropertyInfo& property, void* to, const void* from);
    void CopyValues(const StructInfo& type, void* to, const void* from);

} // namespace VKE::Scene
