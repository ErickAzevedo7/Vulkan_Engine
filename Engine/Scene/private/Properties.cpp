#include "scene/Properties.h"

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <math/MathTypes.h>

#include <vector>

namespace VKE::Scene {

    namespace {
        using MaterialList = std::vector<Handle<Assets::MaterialData>>;

        template<typename T>
        const T& As(const void* value) {
            return *static_cast<const T*>(value);
        }

        template<typename T>
        T& As(void* value) {
            return *static_cast<T*>(value);
        }
    } // namespace

    bool AreEqual(const PropertyInfo& property, const void* a, const void* b) {
        switch (property.Type) {
        case PropertyType::Bool: return As<bool>(a) == As<bool>(b);
        case PropertyType::F32: return As<f32>(a) == As<f32>(b);
        case PropertyType::Vec3: return As<Vec3>(a) == As<Vec3>(b);
        case PropertyType::Quat: return As<Quat>(a) == As<Quat>(b);
        case PropertyType::Enum: return property.Enum->Read(a) == property.Enum->Read(b);
        case PropertyType::Struct: return AreEqual(*property.Struct, a, b);
        case PropertyType::Mesh: return As<Handle<Assets::MeshData>>(a) == As<Handle<Assets::MeshData>>(b);
        case PropertyType::Materials: return As<MaterialList>(a) == As<MaterialList>(b);
        }
        return false;
    }

    bool AreEqual(const StructInfo& type, const void* a, const void* b) {
        for (const PropertyInfo& property : type) {
            if (!AreEqual(property, property.Get(a), property.Get(b))) return false;
        }
        return true;
    }

    void CopyValue(const PropertyInfo& property, void* to, const void* from) {
        switch (property.Type) {
        case PropertyType::Bool: As<bool>(to) = As<bool>(from); break;
        case PropertyType::F32: As<f32>(to) = As<f32>(from); break;
        case PropertyType::Vec3: As<Vec3>(to) = As<Vec3>(from); break;
        case PropertyType::Quat: As<Quat>(to) = As<Quat>(from); break;
        case PropertyType::Enum: property.Enum->Write(to, property.Enum->Read(from)); break;
        case PropertyType::Struct: CopyValues(*property.Struct, to, from); break;
        case PropertyType::Mesh: As<Handle<Assets::MeshData>>(to) = As<Handle<Assets::MeshData>>(from); break;
        case PropertyType::Materials: As<MaterialList>(to) = As<MaterialList>(from); break;
        }
    }

    void CopyValues(const StructInfo& type, void* to, const void* from) {
        for (const PropertyInfo& property : type) {
            CopyValue(property, property.Get(to), property.Get(from));
        }
    }

} // namespace VKE::Scene
