#include "scene/Prefab.h"
#include "scene/GameObject.h"
#include "scene/Properties.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

#include <cstring>

namespace VKE::Scene {

    namespace {
        // Calls `visit` with every property of `object` that differs from `pristine`, with the
        // two values. Placement properties don't count.
        template<typename Visit>
        void VisitDifferences(const Component& component, const StructInfo& type, void* object, void* pristine,
                              const Visit& visit) {
            for (const PropertyInfo& property : type) {
                void* value = property.Get(object);
                void* original = property.Get(pristine);
                if (property.Type == PropertyType::Struct) {
                    VisitDifferences(component, *property.Struct, value, original, visit);
                } else if (!PrefabLibrary::IsPlacement(component, property) && !AreEqual(property, value, original)) {
                    visit(property, value, original);
                }
            }
        }

        // Calls `visit` with every component of `object`, the one it copies and its type.
        template<typename Visit>
        void VisitComponents(GameObject& object, GameObject& pristine, const Visit& visit) {
            for (Component* component : object.GetComponents()) {
                const ComponentTypeInfo* type = ComponentRegistry::Find(component->GetTypeID());
                Component* original = pristine.GetComponent(component->GetTypeID());
                if (!type || !original) continue;
                visit(*component, *original, *type);
            }
        }

        // Calls `visit` with each part of the instance under `object` and what it copies.
        template<typename Visit>
        void VisitParts(PrefabLibrary& prefabs, GameObject& object, const Visit& visit) {
            if (!object.IsPrefabPart()) return;
            if (GameObject* pristine = prefabs.GetPart(object)) visit(object, *pristine);
            for (GameObject* child : object.GetChildren()) {
                VisitParts(prefabs, *child, visit);
            }
        }
    } // namespace

    PrefabLibrary::PrefabLibrary(Assets::AssetManager& assets) : m_Assets(assets) {
    }

    PrefabLibrary::~PrefabLibrary() = default;

    GameObject* PrefabLibrary::GetRoot(Assets::AssetID prefab) {
        if (m_Assets.GetType(prefab) != Assets::AssetType::Prefab) {
            m_Prefabs.erase(prefab);
            return nullptr;
        }

        auto it = m_Prefabs.find(prefab);
        if (it == m_Prefabs.end()) {
            auto objects = MakeIunique<Scene>();
            std::filesystem::path file = m_Assets.GetFile(prefab);
            if (!SceneSerializer::LoadPrefab(*objects, m_Assets, file) || objects->GetRootObjects().empty()) {
                VKE_CORE_ERROR("Prefabs: '{}' can't be read", file.u8string());
                objects.reset();
            }
            it = m_Prefabs.emplace(prefab, std::move(objects)).first;
        }
        return it->second ? it->second->GetRootObjects().front() : nullptr;
    }

    GameObject* PrefabLibrary::GetPart(const GameObject& object) {
        GameObject* root = object.IsPrefabPart() ? GetRoot(object.GetPrefab()) : nullptr;
        return root ? root->GetScene().FindByID(object.GetPrefabPart()) : nullptr;
    }

    Component* PrefabLibrary::GetPart(const Component& component) {
        GameObject* part = GetPart(component.GetOwner());
        return part ? part->GetComponent(component.GetTypeID()) : nullptr;
    }

    void PrefabLibrary::Forget(Assets::AssetID prefab) {
        m_Prefabs.erase(prefab);
    }

    Assets::AssetID PrefabLibrary::Create(GameObject& root, const std::filesystem::path& file) {
        if (HasParts(root)) {
            VKE_CORE_ERROR("Prefabs: '{}' has an instance of a prefab in it; prefabs can't go inside prefabs",
                           root.GetName());
            return {};
        }
        if (!SceneSerializer::SavePrefab(root, m_Assets, file)) return {};
        return m_Assets.FindAsset(file.u8string());
    }

    bool PrefabLibrary::Save(Assets::AssetID prefab) {
        GameObject* root = GetRoot(prefab);
        return root && SceneSerializer::SavePrefab(*root, m_Assets, m_Assets.GetFile(prefab));
    }

    GameObject* PrefabLibrary::Instantiate(Scene& scene, Assets::AssetID prefab, GameObject* parent, ObjectID id) {
        GameObject* source = GetRoot(prefab);
        if (!source) return nullptr;

        // Parents first, siblings in order.
        struct Copier {
            Assets::AssetManager& Assets;
            Scene& Target;
            Assets::AssetID Prefab;
            ObjectID Instance;

            GameObject& Copy(const GameObject& part, GameObject* parent, ObjectID id) {
                GameObject& object = Target.CreateObject(part.GetName(), id);
                object.m_Prefab = Prefab;
                object.m_PrefabPart = part.GetID();
                if (parent) object.SetParent(parent, false);
                object.SetActive(part.IsActive());

                for (Component* component : part.GetComponents()) {
                    const ComponentTypeInfo* type = ComponentRegistry::Find(component->GetTypeID());
                    Component* copy = component == &part.GetTransform() ? &object.GetTransform()
                                                                        : object.AddComponent(type->ID);
                    if (!copy) continue;
                    CopyValues(*type->Properties, type->ToObject(copy), type->ToObject(component));
                    copy->ApplyPropertyChanges(Assets);
                }
                for (const GameObject* child : part.GetChildren()) {
                    Copy(*child, &object, ObjectID::Combine(Instance, child->GetID()));
                }
                return object;
            }
        };
        return &Copier{m_Assets, scene, prefab, id}.Copy(*source, parent, id);
    }

    bool PrefabLibrary::HasParts(const GameObject& object) {
        if (object.IsPrefabPart()) return true;
        for (const GameObject* child : object.GetChildren()) {
            if (HasParts(*child)) return true;
        }
        return false;
    }

    GameObject* PrefabLibrary::FindInstancePart(GameObject& root, ObjectID part) {
        if (root.GetPrefabPart() == part) return &root;
        for (GameObject* child : root.GetChildren()) {
            if (GameObject* found = FindInstancePart(*child, part)) return found;
        }
        return nullptr;
    }

    void PrefabLibrary::Unpack(GameObject& root) {
        root.m_Prefab = {};
        root.m_PrefabPart = {};
        for (GameObject* child : root.GetChildren()) {
            if (child->IsPrefabPart()) Unpack(*child);
        }
    }

    bool PrefabLibrary::IsPlacement(const Component& component, const PropertyInfo& property) {
        return component.GetTypeID() == ComponentRegistry::GetID<TransformComponent>() &&
               component.GetOwner().IsPrefabRoot() &&
               (std::strcmp(property.Name, "Position") == 0 || std::strcmp(property.Name, "Rotation") == 0);
    }

    u32 PrefabLibrary::CountOverrides(GameObject& root) {
        u32 count = 0;
        VisitParts(*this, root, [&](GameObject& object, GameObject& pristine) {
            if (&object != &root && object.GetName() != pristine.GetName()) ++count;
            if (object.IsActive() != pristine.IsActive()) ++count;
            VisitComponents(object, pristine, [&](Component& component, Component& original,
                                                  const ComponentTypeInfo& type) {
                VisitDifferences(component, *type.Properties, type.ToObject(&component), type.ToObject(&original),
                                 [&](const PropertyInfo&, void*, void*) { ++count; });
            });
        });
        return count;
    }

    void PrefabLibrary::RevertOverrides(GameObject& root) {
        VisitParts(*this, root, [&](GameObject& object, GameObject& pristine) {
            if (&object != &root) object.SetName(pristine.GetName());
            object.SetActive(pristine.IsActive());
            VisitComponents(object, pristine, [&](Component& component, Component& original,
                                                  const ComponentTypeInfo& type) {
                VisitDifferences(component, *type.Properties, type.ToObject(&component), type.ToObject(&original),
                                 [](const PropertyInfo& property, void* value, void* source) {
                                     CopyValue(property, value, source);
                                 });
                component.ApplyPropertyChanges(m_Assets);
            });
        });
    }

    void PrefabLibrary::ApplyOverrides(GameObject& root) {
        VisitParts(*this, root, [&](GameObject& object, GameObject& pristine) {
            if (&object != &root) pristine.SetName(object.GetName());
            pristine.SetActive(object.IsActive());
            VisitComponents(object, pristine, [&](Component& component, Component& original,
                                                  const ComponentTypeInfo& type) {
                VisitDifferences(component, *type.Properties, type.ToObject(&component), type.ToObject(&original),
                                 [](const PropertyInfo& property, void* value, void* target) {
                                     CopyValue(property, target, value);
                                 });
                original.ApplyPropertyChanges(m_Assets);
            });
        });
    }

} // namespace VKE::Scene
