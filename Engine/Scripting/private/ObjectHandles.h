#pragma once

#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>

#include <vke/Math.h>
#include <vke/Object.h>

namespace VKE::Scene {
    class GameObject;
} // namespace VKE::Scene

namespace VKE::Scripting {

    // A script's Object is the handle of a GameObject, in one number.
    inline vke::Object ToObject(Handle<Scene::GameObject> handle) {
        return vke::detail::Bits::MakeObject((static_cast<u64>(handle.Index) << 32) | handle.Generation);
    }

    inline Handle<Scene::GameObject> ToHandle(vke::Object object) {
        u64 bits = vke::detail::Bits::GetHandle(object);
        Handle<Scene::GameObject> handle;
        handle.Index = static_cast<u32>(bits >> 32);
        handle.Generation = static_cast<u32>(bits);
        return handle;
    }

    inline Vec3 ToEngine(vke::Vec3 vector) {
        return Vec3(vector.x, vector.y, vector.z);
    }

    inline vke::Vec3 ToSDK(const Vec3& vector) {
        return {vector.x, vector.y, vector.z};
    }

    inline Quat ToEngine(vke::Quat rotation) {
        return Quat(rotation.w, rotation.x, rotation.y, rotation.z);
    }

    inline vke::Quat ToSDK(const Quat& rotation) {
        vke::Quat result;
        result.x = rotation.x;
        result.y = rotation.y;
        result.z = rotation.z;
        result.w = rotation.w;
        return result;
    }

} // namespace VKE::Scripting
