#include "scene/Scene.h"
#include "MeshShapeCache.h"
#include "SceneSystems.h"
#include "scene/GameObject.h"
#include "scene/components/CameraComponent.h"

#include <algorithm>

namespace VKE::Scene {

    namespace {
        // Children before their parents.
        void CollectPostOrder(GameObject& object, std::vector<GameObject*>& out) {
            for (GameObject* child : object.GetChildren()) {
                CollectPostOrder(*child, out);
            }
            out.push_back(&object);
        }
    } // namespace

    Scene::Scene()
        : m_MeshShapes(MakeIunique<MeshShapeCache>(m_PhysicsWorld)) {
    }

    Scene::~Scene() {
        SceneSystems systems = GetSystems();
        for (GameObject* root : m_Roots) {
            std::vector<GameObject*> objects;
            CollectPostOrder(*root, objects);
            for (GameObject* object : objects) {
                for (Component* component : object->GetComponents()) {
                    component->DisconnectFromSystems(systems);
                }
            }
        }
    }

    GameObject& Scene::CreateObject(std::string name, GameObject* parent) {
        Iunique<GameObject> object(new GameObject(*this, std::move(name)));
        GameObject& result = *object;
        result.m_Handle = m_Objects.Emplace(std::move(object));
        m_Roots.push_back(&result);

        if (parent) result.SetParent(parent, false);
        return result;
    }

    GameObject& Scene::CreateObject(std::string name, ObjectID id) {
        GameObject& object = CreateObject(std::move(name));
        object.m_ID = id;
        return object;
    }

    GameObject* Scene::FindByID(ObjectID id) const {
        for (const Iunique<GameObject>& object : m_Objects.Values()) {
            if (object->GetID() == id) return object.get();
        }
        return nullptr;
    }

    void Scene::Destroy(GameObject& object) {
        std::vector<GameObject*> doomed;
        CollectPostOrder(object, doomed);

        for (GameObject* target : doomed) {
            for (Component* component : target->GetComponents()) {
                DisconnectComponent(*component);
            }
        }
        // Children come first, so each object leaves a parent that is still alive.
        for (GameObject* target : doomed) {
            if (target->m_SyncQueued) {
                m_SyncQueue.erase(std::remove(m_SyncQueue.begin(), m_SyncQueue.end(), target), m_SyncQueue.end());
            }
            target->Unlink();
            m_Objects.Remove(target->m_Handle);
        }
    }

    GameObject* Scene::Get(Handle<GameObject> handle) const {
        const Iunique<GameObject>* object = m_Objects.Get(handle);
        return object ? object->get() : nullptr;
    }

    void Scene::Update() {
        SceneSystems systems = GetSystems();
        SyncQueued(systems);
        m_PhysicsWorld.Step();
        ApplyPhysicsStep();
        // Sends the objects the step moved.
        SyncQueued(systems);
    }

    void Scene::Sync() {
        SceneSystems systems = GetSystems();
        SyncQueued(systems);
        m_Contacts.clear();
    }

    SceneSystems Scene::GetSystems() {
        return SceneSystems{m_RenderScene, m_PhysicsWorld, *m_MeshShapes};
    }

    void Scene::SyncQueued(SceneSystems& systems) {
        std::vector<GameObject*> queue;
        queue.swap(m_SyncQueue);
        for (GameObject* object : queue) {
            object->m_SyncQueued = false;
            for (Component* component : object->GetComponents()) {
                component->SyncToSystems(systems);
            }
        }
    }

    void Scene::ApplyPhysicsStep() {
        for (Physics::BodyID body : m_PhysicsWorld.GetMovedBodies()) {
            const Physics::BodyDesc* desc = m_PhysicsWorld.GetBody(body);
            if (GameObject* object = Get(UnpackObjectHandle(desc->UserData))) {
                object->GetTransform().SetWorldPosition(desc->Pose.Position);
            }
        }

        m_Contacts.clear();
        for (const Physics::ContactEvent& event : m_PhysicsWorld.GetContactEvents()) {
            m_Contacts.push_back({UnpackObjectHandle(event.UserDataA), UnpackObjectHandle(event.UserDataB), event.Phase,
                                  event.IsTrigger, event.Normal});
        }
    }

    GameObject* Scene::GetMeshOwner(Renderer::MeshInstanceID mesh) const {
        const Renderer::MeshInstanceDesc* desc = m_RenderScene.GetMeshes().Get(mesh);
        return desc ? Get(UnpackObjectHandle(desc->Owner)) : nullptr;
    }

    Renderer::RenderView Scene::GetView() const {
        return m_MainCamera ? m_MainCamera->GetView() : Renderer::RenderView{};
    }

    void Scene::OnRootChanged(GameObject& object, bool isRoot) {
        if (isRoot) {
            m_Roots.push_back(&object);
        } else {
            m_Roots.erase(std::remove(m_Roots.begin(), m_Roots.end(), &object), m_Roots.end());
        }
    }

    bool Scene::QueueSync(GameObject& object) {
        if (object.m_SyncQueued) return false;
        object.m_SyncQueued = true;
        m_SyncQueue.push_back(&object);
        return true;
    }

    void Scene::ConnectComponent(Component& component, ComponentTypeID type) {
        SceneSystems systems = GetSystems();
        component.ConnectToSystems(systems);

        if (!m_MainCamera && type == ComponentRegistry::GetID<CameraComponent>()) {
            m_MainCamera = static_cast<CameraComponent*>(&component);
        }
    }

    void Scene::DisconnectComponent(Component& component) {
        SceneSystems systems = GetSystems();
        component.DisconnectFromSystems(systems);

        if (&component == static_cast<Component*>(m_MainCamera)) m_MainCamera = nullptr;
    }

} // namespace VKE::Scene
