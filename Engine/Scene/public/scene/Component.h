#pragma once

#include <core/Types.h>

namespace VKE::Scene {

    class GameObject;
    class Scene;
    struct SceneSystems;
    class TransformComponent;

    // Data attached to a GameObject. The engine's components keep a copy of their data in the
    // systems the scene feeds, such as the render scene, through three steps the scene calls:
    // connect when the component is added, sync at the end of an update in which it changed,
    // and disconnect when it is removed.
    class Component {
    public:
        virtual ~Component() = default;

        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;

        GameObject& GetOwner() const { return *m_Owner; }
        Scene& GetScene() const;
        // The owner's transform.
        TransformComponent& GetTransform() const;

    protected:
        Component() = default;

        // Has the scene call SyncToSystems on the components of the owner at the end of its
        // next update. Returns false if they were already queued.
        bool MarkSyncNeeded();

    private:
        friend class GameObject;
        friend class Scene;

        // Creates the component's entry in the systems.
        virtual void ConnectToSystems(SceneSystems& systems) {}
        // Sends the component's current state: its data, its object's world transform and
        // whether the object is active.
        virtual void SyncToSystems(SceneSystems& systems) {}
        // Deletes the component's entry from the systems.
        virtual void DisconnectFromSystems(SceneSystems& systems) {}

        GameObject* m_Owner = nullptr;
    };

} // namespace VKE::Scene
