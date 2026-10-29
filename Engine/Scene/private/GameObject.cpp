#include "scene/GameObject.h"
#include "scene/Scene.h"

#include <algorithm>

namespace VKE::Scene {

    GameObject::GameObject(Scene& scene, std::string name)
        : m_Scene(scene), m_ID(ObjectID::Generate()), m_Name(std::move(name)) {
        // Added directly: the transform feeds no system, so the scene doesn't connect it.
        Iunique<TransformComponent> transform(new TransformComponent());
        transform->m_Owner = this;
        m_Transform = transform.get();
        m_Components.push_back({ComponentRegistry::GetID<TransformComponent>(), std::move(transform)});
    }

    GameObject::~GameObject() = default;

    bool GameObject::SetParent(GameObject* parent, bool keepWorldTransform) {
        if (parent == m_Parent) return true;
        if (parent && (parent == this || parent->IsChildOf(this) || &parent->m_Scene != &m_Scene)) return false;

        // Read before the move, while it still goes through the old parent.
        Mat4 world = m_Transform->GetWorldMatrix();

        Unlink();
        m_Parent = parent;
        if (m_Parent) {
            m_Parent->m_Children.push_back(this);
        } else {
            m_Scene.OnRootChanged(*this, true);
        }

        if (keepWorldTransform) {
            // The local transform that puts the object back where it was. A skew the old
            // parents gave it can't be kept.
            Mat4 local = m_Parent ? glm::inverse(m_Parent->m_Transform->GetWorldMatrix()) * world : world;
            m_Transform->SetLocal(Transform::Decompose(local));
        } else {
            // Same local values under a new parent: only the world values change.
            m_Transform->SetLocal(m_Transform->GetLocal());
        }
        RefreshActiveInHierarchy();
        return true;
    }

    bool GameObject::IsChildOf(const GameObject* ancestor) const {
        if (!ancestor) return false;
        for (const GameObject* object = m_Parent; object; object = object->m_Parent) {
            if (object == ancestor) return true;
        }
        return false;
    }

    size_t GameObject::GetIndexInParent() const {
        const std::vector<GameObject*>& siblings = m_Parent ? m_Parent->m_Children : m_Scene.m_Roots;
        return static_cast<size_t>(std::find(siblings.begin(), siblings.end(), this) - siblings.begin());
    }

    void GameObject::SetIndexInParent(size_t index) {
        std::vector<GameObject*>& siblings = m_Parent ? m_Parent->m_Children : m_Scene.m_Roots;
        siblings.erase(std::find(siblings.begin(), siblings.end(), this));
        siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(std::min(index, siblings.size())), this);
    }

    void GameObject::SetActive(bool active) {
        if (m_Active == active) return;
        m_Active = active;
        RefreshActiveInHierarchy();
    }

    Component* GameObject::AddComponent(ComponentTypeID type) {
        if (Component* existing = GetComponent(type)) return existing;

        const ComponentTypeInfo* info = ComponentRegistry::Find(type);
        if (!info || !info->Create) return nullptr;
        Iunique<Component> component(info->Create());
        Component* result = component.get();
        AttachComponent(type, std::move(component));
        return result;
    }

    void GameObject::AttachComponent(ComponentTypeID type, Iunique<Component> component) {
        Component& added = *component;
        added.m_Owner = this;
        m_Components.push_back({type, std::move(component)});
        m_Scene.ConnectComponent(added, type);
    }

    Component* GameObject::GetComponent(ComponentTypeID type) const {
        for (const ComponentEntry& entry : m_Components) {
            if (entry.Type == type) return entry.Instance.get();
        }
        return nullptr;
    }

    bool GameObject::RemoveComponent(ComponentTypeID type) {
        if (type == ComponentRegistry::GetID<TransformComponent>()) return false;
        Component* component = GetComponent(type);
        if (!component) return false;
        RemoveComponent(*component);
        return true;
    }

    void GameObject::RemoveComponent(Component& component) {
        m_Scene.DisconnectComponent(component);
        m_Components.erase(std::remove_if(m_Components.begin(), m_Components.end(),
                                          [&](const ComponentEntry& entry) { return entry.Instance.get() == &component; }),
                           m_Components.end());
    }

    bool GameObject::MarkSyncNeeded() {
        return m_Scene.QueueSync(*this);
    }

    void GameObject::RefreshActiveInHierarchy() {
        bool active = m_Active && (!m_Parent || m_Parent->m_ActiveInHierarchy);
        // Children only depend on this value, so they are unaffected if it didn't change.
        if (active == m_ActiveInHierarchy) return;

        m_ActiveInHierarchy = active;
        MarkSyncNeeded();
        for (GameObject* child : m_Children) {
            child->RefreshActiveInHierarchy();
        }
    }

    void GameObject::Unlink() {
        if (m_Parent) {
            auto& siblings = m_Parent->m_Children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
            m_Parent = nullptr;
        } else {
            m_Scene.OnRootChanged(*this, false);
        }
    }

} // namespace VKE::Scene
