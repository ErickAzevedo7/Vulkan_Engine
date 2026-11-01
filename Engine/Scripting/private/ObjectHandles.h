#pragma once

#include <core/Handle.h>
#include <core/Types.h>

#include <vke/Object.h>

namespace VKE::Scene {
    class GameObject;
} // namespace VKE::Scene

namespace VKE::Scripting {

    // A script's Object is the handle of a GameObject, in one number.
    inline vke::Object ToObject(Handle<Scene::GameObject> handle) {
        return vke::detail::ObjectBits::Make((static_cast<u64>(handle.Index) << 32) | handle.Generation);
    }

    inline Handle<Scene::GameObject> ToHandle(vke::Object object) {
        u64 bits = vke::detail::ObjectBits::Get(object);
        Handle<Scene::GameObject> handle;
        handle.Index = static_cast<u32>(bits >> 32);
        handle.Generation = static_cast<u32>(bits);
        return handle;
    }

} // namespace VKE::Scripting
