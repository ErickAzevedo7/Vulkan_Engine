#include "ApiCommon.h"
#include "ScriptComponent.h"

#include <core/Log.h>
#include <core/ObjectID.h>
#include <scene/ComponentRegistry.h>
#include <scene/Prefab.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>

#include <vke/detail/ApiBindings.h>

#include <string_view>

namespace vke::api {

    namespace {
        using VKE::Scene::GameObject;
        using VKE::Scripting::FindObject;
        using VKE::Scripting::ScriptRun;
        using VKE::Scripting::ToHandle;
        using VKE::Scripting::ToObject;

        const VKE::Scene::ComponentTypeInfo* FindScriptType(const char* name) {
            const VKE::Scene::ComponentTypeInfo* type = VKE::Scene::ComponentRegistry::Find(std::string_view(name));
            if (type && type->Context) return type;
            VKE_CORE_WARN("Scripts: '{}' isn't a script type", name);
            return nullptr;
        }

        GameObject* FindByName(GameObject& object, std::string_view name) {
            if (object.GetName() == name) return &object;
            for (GameObject* child : object.GetChildren()) {
                if (GameObject* found = FindByName(*child, name)) return found;
            }
            return nullptr;
        }
    } // namespace

    bool Object_IsValid(Object self) {
        return FindObject(self) != nullptr;
    }

    const char* Object_GetName(Object self) {
        GameObject* object = FindObject(self);
        return object ? object->GetName().c_str() : "";
    }

    void Object_SetName(Object self, const char* name) {
        if (GameObject* object = FindObject(self)) object->SetName(name ? name : "");
    }

    bool Object_IsActive(Object self) {
        GameObject* object = FindObject(self);
        return object && object->IsActive();
    }

    void Object_SetActive(Object self, bool active) {
        if (GameObject* object = FindObject(self)) object->SetActive(active);
    }

    Transform Object_GetTransform(Object self) {
        return Transform(self);
    }

    Object Object_GetParent(Object self) {
        GameObject* object = FindObject(self);
        GameObject* parent = object ? object->GetParent() : nullptr;
        return parent ? ToObject(parent->GetHandle()) : Object();
    }

    void Object_SetParent(Object self, Object parent, Space keep) {
        GameObject* object = FindObject(self);
        if (!object) return;
        if (!object->SetParent(FindObject(parent), keep == Space::World)) {
            VKE_CORE_WARN("Scripts: '{}' can't go under an object below it", object->GetName());
        }
    }

    int Object_GetChildCount(Object self) {
        GameObject* object = FindObject(self);
        return object ? static_cast<int>(object->GetChildren().size()) : 0;
    }

    Object Object_GetChild(Object self, int index) {
        GameObject* object = FindObject(self);
        if (!object || index < 0 || index >= static_cast<int>(object->GetChildren().size())) return {};
        return ToObject(object->GetChildren()[static_cast<size_t>(index)]->GetHandle());
    }

    Light Object_GetLight(Object self) {
        return VKE::Scripting::FindComponent<VKE::Scene::LightComponent>(self) ? Light(self) : Light();
    }

    Camera Object_GetCamera(Object self) {
        return VKE::Scripting::FindComponent<VKE::Scene::CameraComponent>(self) ? Camera(self) : Camera();
    }

    Mesh Object_GetMesh(Object self) {
        return VKE::Scripting::FindComponent<VKE::Scene::MeshComponent>(self) ? Mesh(self) : Mesh();
    }

    void* Object_AddScriptByName(Object self, const char* name) {
        GameObject* object = FindObject(self);
        if (!object) return nullptr;
        if (ScriptRun::GetCurrent()->IsStopping()) {
            VKE_CORE_WARN("Scripts: can't add '{}' to '{}' during OnStop", name, object->GetName());
            return nullptr;
        }
        const VKE::Scene::ComponentTypeInfo* type = FindScriptType(name);
        if (!type) return nullptr;
        return VKE::Scripting::ScriptComponent::From(*object->AddComponent(type->ID))->GetInstance();
    }

    void* Object_GetScriptByName(Object self, const char* name) {
        GameObject* object = FindObject(self);
        if (!object) return nullptr;
        const VKE::Scene::ComponentTypeInfo* type = FindScriptType(name);
        VKE::Scene::Component* component = type ? object->GetComponent(type->ID) : nullptr;
        return component ? VKE::Scripting::ScriptComponent::From(*component)->GetInstance() : nullptr;
    }

    Object CreateObject(const char* name) {
        ScriptRun* run = ScriptRun::GetCurrent();
        if (!run) return {};
        return ToObject(run->GetScene().CreateObject(name ? name : "GameObject").GetHandle());
    }

    Object FindObject(const char* name) {
        ScriptRun* run = ScriptRun::GetCurrent();
        if (!run || !name) return {};
        for (GameObject* root : run->GetScene().GetRootObjects()) {
            if (GameObject* found = FindByName(*root, name)) return ToObject(found->GetHandle());
        }
        return {};
    }

    void Destroy(Object object) {
        if (::VKE::Scripting::FindObject(object)) ScriptRun::GetCurrent()->QueueDestroy(ToHandle(object));
    }

    Object Instantiate(PrefabRef prefab, Vec3 position) {
        ScriptRun* run = ScriptRun::GetCurrent();
        if (!run || !prefab.IsSet()) return {};
        VKE::Assets::AssetID id{detail::Bits::GetID(prefab)};
        GameObject* instance = run->GetPrefabs().Instantiate(run->GetScene(), id, nullptr, VKE::ObjectID::Generate());
        if (!instance) {
            VKE_CORE_WARN("Scripts: a PrefabRef points at something that isn't a prefab");
            return {};
        }
        instance->GetTransform().SetWorldPosition(VKE::Scripting::ToEngine(position));
        return ToObject(instance->GetHandle());
    }

} // namespace vke::api
