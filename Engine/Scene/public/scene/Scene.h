#pragma once

#include <core/Handle.h>
#include <core/SlotMap.h>
#include <core/Types.h>
#include <renderer/RenderScene.h>
#include <renderer/RenderTypes.h>

#include <string>
#include <typeindex>
#include <vector>

namespace VKE::Scene {

    class CameraComponent;
    class Component;
    class GameObject;

    // Owns a hierarchy of GameObjects and the render scene their components fill.
    //
    // Components connect to the render scene when added and disconnect when removed. Changes
    // in between, to a component or to its object's transform or active state, are sent once
    // per frame by Update.
    class Scene {
    public:
        Scene();
        ~Scene();

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        GameObject& CreateObject(std::string name = "GameObject", GameObject* parent = nullptr);
        // Destroys the object and its children at once; their handles stop resolving.
        void Destroy(GameObject& object);

        // The object behind a handle, or null once it has been destroyed.
        GameObject* Get(Handle<GameObject> handle) const;
        const std::vector<GameObject*>& GetRootObjects() const { return m_Roots; }

        // Sends the changes made since the last update to the render scene. Call it before
        // rendering.
        void Update();

        const Renderer::RenderScene& GetRenderScene() const { return m_RenderScene; }

        // The camera the scene is seen from. The first camera added is used until another is set.
        CameraComponent* GetMainCamera() const { return m_MainCamera; }
        void SetMainCamera(CameraComponent* camera) { m_MainCamera = camera; }
        // The main camera's view, or a view from the origin if there is none.
        Renderer::RenderView GetView() const;

    private:
        friend class GameObject;

        void OnRootChanged(GameObject& object, bool isRoot);
        // Returns false if the object was already queued.
        bool QueueSync(GameObject& object);
        void ConnectComponent(Component& component, std::type_index type);
        void DisconnectComponent(Component& component);

        // Declared before the objects, so it outlives the components that fill it.
        Renderer::RenderScene m_RenderScene;
        CameraComponent* m_MainCamera = nullptr;

        SlotMap<Iunique<GameObject>, GameObject> m_Objects;
        std::vector<GameObject*> m_Roots;
        // Objects whose components are synced at the next update.
        std::vector<GameObject*> m_SyncQueue;
    };

} // namespace VKE::Scene
