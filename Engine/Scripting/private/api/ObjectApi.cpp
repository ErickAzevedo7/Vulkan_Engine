#include "ObjectHandles.h"
#include "ScriptComponent.h"
#include "ScriptRun.h"

#include <core/Log.h>
#include <scene/ComponentRegistry.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>

#include <vke/detail/ApiBindings.h>

namespace vke::api {

    namespace {
        using VKE::Scene::GameObject;
        using VKE::Scripting::ScriptRun;

        // Null if the object is gone, or no game is running.
        GameObject* Find(Object object) {
            ScriptRun* run = ScriptRun::GetCurrent();
            return run ? run->GetScene().Get(VKE::Scripting::ToHandle(object)) : nullptr;
        }
    } // namespace

    bool Object_IsValid(Object self) {
        return Find(self) != nullptr;
    }

    const char* Object_GetName(Object self) {
        GameObject* object = Find(self);
        return object ? object->GetName().c_str() : "";
    }

    void* Object_AddScriptByName(Object self, const char* name) {
        GameObject* object = Find(self);
        if (!object) return nullptr;
        if (ScriptRun::GetCurrent()->IsStopping()) {
            VKE_CORE_WARN("Scripts: can't add '{}' to '{}' during OnStop", name, object->GetName());
            return nullptr;
        }

        const VKE::Scene::ComponentTypeInfo* type = VKE::Scene::ComponentRegistry::Find(std::string_view(name));
        if (!type || !type->Context) {
            VKE_CORE_WARN("Scripts: '{}' isn't a script type", name);
            return nullptr;
        }
        auto* script = VKE::Scripting::ScriptComponent::From(*object->AddComponent(type->ID));
        return script->GetInstance();
    }

    Object CreateObject(const char* name) {
        ScriptRun* run = ScriptRun::GetCurrent();
        if (!run) return {};
        return VKE::Scripting::ToObject(run->GetScene().CreateObject(name ? name : "GameObject").GetHandle());
    }

    void Destroy(Object object) {
        if (Find(object)) ScriptRun::GetCurrent()->QueueDestroy(VKE::Scripting::ToHandle(object));
    }

} // namespace vke::api
