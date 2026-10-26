#pragma once

#include <scene/ComponentRegistry.h>

#include <core/Types.h>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {

    class GameObject;
    class Scene;
    struct SceneSystems;
    class TransformComponent;

    // Data attached to a GameObject. The engine's components keep a copy of their data in the
    // systems the scene feeds, such as the render scene, through three steps the scene calls:
    // connect when the component is added, sync at the end of an update in which it changed,
    // and disconnect when it is removed.
    //
    // Every component type declares itself with VKE_COMPONENT and marks its saved fields with
    // VKE_PROPERTY; the reflection generator registers it with those properties.
    class Component {
    public:
        virtual ~Component() = default;

        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;

        GameObject& GetOwner() const { return *m_Owner; }
        Scene& GetScene() const;
        // The owner's transform.
        TransformComponent& GetTransform() const;

        // Given by VKE_COMPONENT.
        virtual ComponentTypeID GetTypeID() const = 0;

        // Call after writing properties directly through the registry's tables, as a loader or
        // an editor does: the component applies them, as its setters would have. `assets` holds
        // the assets the properties refer to.
        void ApplyPropertyChanges(const Assets::AssetManager& assets);

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

        // Called by ApplyPropertyChanges. Marks the component for sync by default.
        virtual void OnPropertiesChanged(const Assets::AssetManager& assets);

        GameObject* m_Owner = nullptr;
    };

} // namespace VKE::Scene
