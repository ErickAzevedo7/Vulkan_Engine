#pragma once

#include <core/Handle.h>
#include <core/ObjectID.h>
#include <core/SlotMap.h>
#include <scene/ComponentRegistry.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <physics/PhysicsTypes.h>
#include <physics/PhysicsWorld.h>
#include <renderer/RenderScene.h>
#include <renderer/RenderTypes.h>

#include <string>
#include <vector>

namespace VKE::Scene {

    class CameraComponent;
    class Component;
    class GameObject;
    class MeshShapeCache;
    struct SceneSystems;

    // Two colliders' objects that touched during an update.
    struct Contact {
        // Either can be destroyed by the time the contact is read: Scene::Get returns null for it.
        // An End contact can also refer to an object destroyed during the update.
        Handle<GameObject> A;
        Handle<GameObject> B;
        Physics::ContactPhase Phase = Physics::ContactPhase::Begin;
        // Either collider is a trigger, so nothing was pushed.
        bool IsTrigger = false;
        // Solid contacts that haven't ended: the direction that moves A away from B.
        Vec3 Normal{0.0f};
    };

    // Owns a hierarchy of GameObjects and the render scene and physics world their components fill.
    //
    // Components connect to those systems when added and disconnect when removed. Changes in
    // between, to a component or to its object's transform or active state, are sent once per
    // frame by Update.
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
        // The object with this ID, or null. Looks through every object.
        GameObject* FindByID(ObjectID id) const;
        const std::vector<GameObject*>& GetRootObjects() const { return m_Roots; }
        size_t GetObjectCount() const { return m_Objects.Size(); }

        // Sends the changes made since the last update to the physics world, pushes movable
        // colliders out of solid ones (moving their objects), and sends the result to the render
        // scene. Call it once per frame, before rendering.
        void Update();
        // Only sends the changes on, with no physics step: nothing moves by itself. For a scene
        // being edited or paused. Call it instead of Update.
        void Sync();

        // What touched what during the last Update, each pair once.
        const std::vector<Contact>& GetContacts() const { return m_Contacts; }

        const Renderer::RenderScene& GetRenderScene() const { return m_RenderScene; }
        // The object whose MeshComponent drew `mesh`, or null.
        GameObject* GetMeshOwner(Renderer::MeshInstanceID mesh) const;

        // The camera the scene is seen from. The first camera added is used until another is set.
        CameraComponent* GetMainCamera() const { return m_MainCamera; }
        void SetMainCamera(CameraComponent* camera) { m_MainCamera = camera; }
        // The main camera's view, or a view from the origin if there is none.
        Renderer::RenderView GetView() const;

    private:
        friend class GameObject;
        friend class SceneSerializer;

        // For loading: the object keeps the ID it was saved with.
        GameObject& CreateObject(std::string name, ObjectID id);

        void OnRootChanged(GameObject& object, bool isRoot);
        // Returns false if the object was already queued.
        bool QueueSync(GameObject& object);
        void ConnectComponent(Component& component, ComponentTypeID type);
        void DisconnectComponent(Component& component);

        SceneSystems GetSystems();
        // Syncs the components of every queued object.
        void SyncQueued(SceneSystems& systems);
        // Moves the objects the physics step pushed and collects its contacts.
        void ApplyPhysicsStep();

        // Declared before the objects, so they outlive the components that fill them.
        Renderer::RenderScene m_RenderScene;
        Physics::PhysicsWorld m_PhysicsWorld;
        Iunique<MeshShapeCache> m_MeshShapes;
        std::vector<Contact> m_Contacts;
        CameraComponent* m_MainCamera = nullptr;

        SlotMap<Iunique<GameObject>, GameObject> m_Objects;
        std::vector<GameObject*> m_Roots;
        // Objects whose components are synced at the next update.
        std::vector<GameObject*> m_SyncQueue;
    };

} // namespace VKE::Scene
