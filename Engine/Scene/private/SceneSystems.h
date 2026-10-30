#pragma once

#include "MeshShapeCache.h"

#include <core/Handle.h>
#include <physics/PhysicsWorld.h>
#include <renderer/RenderScene.h>

namespace VKE::Scene {

    class GameObject;

    // What a scene's components feed. Only the scene module sees its definition.
    struct SceneSystems {
        Renderer::RenderScene& Render;
        Physics::PhysicsWorld& Physics;
        MeshShapeCache& MeshShapes;
    };

    // A physics body's UserData and a mesh instance's Owner hold the handle of the object they
    // belong to.
    inline u64 PackObjectHandle(Handle<GameObject> handle) {
        return (static_cast<u64>(handle.Index) << 32) | handle.Generation;
    }

    inline Handle<GameObject> UnpackObjectHandle(u64 userData) {
        Handle<GameObject> handle;
        handle.Index = static_cast<u32>(userData >> 32);
        handle.Generation = static_cast<u32>(userData);
        return handle;
    }

} // namespace VKE::Scene
