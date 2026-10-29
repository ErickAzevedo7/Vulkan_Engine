#pragma once

#include <scene/Component.h>
#include <scene/components/TransformComponent.h>

#include <core/Handle.h>
#include <core/ObjectID.h>
#include <core/Types.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace VKE::Scene {

    // Something in a scene: a name, a place in the hierarchy and a set of components, at most
    // one of each type. Every object has a TransformComponent. Created and destroyed through
    // its Scene.
    class GameObject {
        struct ComponentEntry;

    public:
        // The object's components, in the order they were added, the transform first.
        class ComponentRange {
        public:
            class Iterator {
            public:
                explicit Iterator(std::vector<ComponentEntry>::const_iterator it) : m_It(it) {}
                Component* operator*() const { return m_It->Instance.get(); }
                Iterator& operator++() {
                    ++m_It;
                    return *this;
                }
                bool operator!=(const Iterator& other) const { return m_It != other.m_It; }

            private:
                std::vector<ComponentEntry>::const_iterator m_It;
            };

            explicit ComponentRange(const std::vector<ComponentEntry>& entries) : m_Entries(entries) {}

            Iterator begin() const { return Iterator(m_Entries.begin()); }
            Iterator end() const { return Iterator(m_Entries.end()); }
            size_t Size() const { return m_Entries.size(); }

        private:
            const std::vector<ComponentEntry>& m_Entries;
        };

        ~GameObject();

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;

        // A reference that stays safe to hold: Scene::Get returns null once the object is destroyed.
        Handle<GameObject> GetHandle() const { return m_Handle; }
        // Identifies the object in saved scenes and in references between saved objects.
        // Kept when a scene is loaded; a copy of an object gets a new one.
        ObjectID GetID() const { return m_ID; }
        Scene& GetScene() const { return m_Scene; }

        const std::string& GetName() const { return m_Name; }
        void SetName(std::string name) { m_Name = std::move(name); }

        TransformComponent& GetTransform() const { return *m_Transform; }

        // A null parent makes the object a root. With `keepWorldTransform` the object stays where
        // it is in the world; otherwise its local transform is kept. Fails if `parent` is this
        // object, one of its descendants, or in another scene.
        GameObject* GetParent() const { return m_Parent; }
        // In the order they were added, unless moved with SetIndexInParent.
        const std::vector<GameObject*>& GetChildren() const { return m_Children; }
        bool SetParent(GameObject* parent, bool keepWorldTransform = true);
        bool IsChildOf(const GameObject* ancestor) const;

        // The object's place among its parent's children, or among the scene's roots for a root.
        // Setting it moves the object there, shifting the others; past the end means last.
        size_t GetIndexInParent() const;
        void SetIndexInParent(size_t index);

        // An inactive object, and everything under it, is not drawn.
        bool IsActive() const { return m_Active; }
        bool IsActiveInHierarchy() const { return m_ActiveInHierarchy; }
        void SetActive(bool active);

        ComponentRange GetComponents() const { return ComponentRange(m_Components); }

        // Adds a component of a registered type, as a scene file names it. Returns the existing
        // one if the object already has that type, or null if the type can't be added by name.
        Component* AddComponent(ComponentTypeID type);
        Component* GetComponent(ComponentTypeID type) const;
        // False if the object has no component of that type, or the type is the transform.
        bool RemoveComponent(ComponentTypeID type);

        // Returns the existing component if the object already has one of this type.
        template<typename T, typename... Args>
        T* AddComponent(Args&&... args) {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from VKE::Scene::Component");
            static_assert(!std::is_same_v<T, TransformComponent>, "Every object already has a TransformComponent");
            if (T* existing = GetComponent<T>()) return existing;

            Iunique<T> component(new T(std::forward<Args>(args)...));
            T* result = component.get();
            AttachComponent(ComponentRegistry::GetID<T>(), std::move(component));
            return result;
        }

        // Finds the exact type: a component of a derived type doesn't match.
        template<typename T>
        T* GetComponent() const {
            return static_cast<T*>(GetComponent(ComponentRegistry::GetID<T>()));
        }

        template<typename T>
        bool HasComponent() const {
            return GetComponent<T>() != nullptr;
        }

        // The first component of this type on this object or below it, depth first.
        template<typename T>
        T* GetComponentInChildren() const {
            if (T* component = GetComponent<T>()) return component;
            for (GameObject* child : m_Children) {
                if (T* component = child->GetComponentInChildren<T>()) return component;
            }
            return nullptr;
        }

        template<typename T>
        void RemoveComponent() {
            static_assert(!std::is_same_v<T, TransformComponent>, "The TransformComponent can't be removed");
            if (T* component = GetComponent<T>()) RemoveComponent(*component);
        }

    private:
        friend class Component;
        friend class Scene;

        struct ComponentEntry {
            ComponentTypeID Type;
            Iunique<Component> Instance;
        };

        GameObject(Scene& scene, std::string name);

        void AttachComponent(ComponentTypeID type, Iunique<Component> component);
        void RemoveComponent(Component& component);

        // Has the scene sync this object's components at its next update. Returns false if
        // they were already queued.
        bool MarkSyncNeeded();

        // Recomputes the cached active state of this object and its children, and queues a
        // sync for every object whose state changed.
        void RefreshActiveInHierarchy();

        // Leaves the parent's children, or the scene's roots, without recomputing anything.
        void Unlink();

        Scene& m_Scene;
        Handle<GameObject> m_Handle;
        ObjectID m_ID;
        std::string m_Name;
        bool m_Active = true;
        // Cached from the parents, so per-frame checks don't walk up the hierarchy.
        bool m_ActiveInHierarchy = true;
        // Already in the scene's list of objects to sync.
        bool m_SyncQueued = false;

        GameObject* m_Parent = nullptr;
        std::vector<GameObject*> m_Children;

        // The first entry of m_Components.
        TransformComponent* m_Transform = nullptr;
        std::vector<ComponentEntry> m_Components;
    };

} // namespace VKE::Scene
