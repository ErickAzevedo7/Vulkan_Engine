#include "ScriptComponent.h"
#include "ObjectHandles.h"
#include "ScriptRun.h"

#include <core/Log.h>
#include <core/ObjectID.h>
#include <platform/GuardedCall.h>
#include <scene/GameObject.h>
#include <scene/Prefab.h>
#include <scene/Scene.h>

#include <cstring>

namespace VKE::Scripting {

    namespace {
        // The running game, if this scene is what it runs: prefabs keep scenes of their own.
        ScriptRun* FindRun(const Scene::Component& component) {
            ScriptRun* run = ScriptRun::GetCurrent();
            return run && &run->GetScene() == &component.GetScene() ? run : nullptr;
        }

        // Runs `call` guarded. False if it faulted, with what happened in `outFault`.
        template<typename Call>
        bool RunGuarded(const Call& call, std::string& outFault) {
            auto run = [](void* data) { (*static_cast<const Call*>(data))(); };
            return Platform::GuardedCall(run, const_cast<Call*>(&call), outFault);
        }

        void ResolveFields(const Scene::StructInfo& type, void* object, Scene::GameObject& owner) {
            for (const Scene::PropertyInfo& property : type) {
                void* value = property.Get(object);
                if (property.Type == Scene::PropertyType::Struct) {
                    ResolveFields(*property.Struct, value, owner);
                } else if (property.Type == Scene::PropertyType::Object) {
                    Scene::GameObject* target =
                        Scene::PrefabLibrary::FindReferenced(owner, *static_cast<ObjectID*>(value));
                    vke::detail::Bits::SetObject(*static_cast<vke::ObjectRef*>(value),
                                                 target ? ToObject(target->GetHandle()) : vke::Object());
                }
            }
        }
    } // namespace

    ScriptComponent::ScriptComponent(const ScriptType& type) : m_Type(type) {
        std::string fault;
        bool created = RunGuarded(
            [&] {
                m_Instance = m_Type.Create();
                m_Script = m_Type.ToScript(m_Instance);
            },
            fault);
        if (created) return;

        VKE_CORE_ERROR("Scripts: the constructor of '{}' crashed: {}", m_Type.Name, fault);
        m_Broken = true;
        m_Script = nullptr;
        m_Instance = nullptr;
        if (m_Type.DefaultInstance) {
            m_FailedInstance.reset(new char[m_Type.Size]);
            std::memcpy(m_FailedInstance.get(), m_Type.DefaultInstance, m_Type.Size);
            m_Instance = m_FailedInstance.get();
        }
    }

    ScriptComponent::~ScriptComponent() {
        if (m_FailedInstance || !m_Instance) return;
        std::string fault;
        if (!RunGuarded([&] { m_Type.Destroy(m_Instance); }, fault)) {
            VKE_CORE_ERROR("Scripts: the destructor of '{}' crashed: {}", m_Type.Name, fault);
        }
    }

    template<typename Call>
    void ScriptComponent::Guard(const char* function, const Call& call) {
        if (m_Broken) return;
        std::string fault;
        if (RunGuarded(call, fault)) return;

        m_Broken = true;
        std::string message = "Script '" + m_Type.Name + "' on '" + GetOwner().GetName() + "' crashed in " + function +
                              ": " + fault;
        VKE_CORE_ERROR("Scripts: {}", message);
        if (ScriptRun* run = FindRun(*this)) run->ReportFault(std::move(message));
    }

    ScriptComponent* ScriptComponent::From(Scene::Component& component) {
        const Scene::ComponentTypeInfo* type = Scene::ComponentRegistry::Find(component.GetTypeID());
        return type && type->Context ? static_cast<ScriptComponent*>(&component) : nullptr;
    }

    void ScriptComponent::ConnectToSystems(Scene::SceneSystems&) {
        Guard("its setup", [&] { m_Type.Calls->SetOwner(m_Script, ToObject(GetOwner().GetHandle())); });
        if (ScriptRun* run = FindRun(*this)) run->Add(*this);
    }

    void ScriptComponent::DisconnectFromSystems(Scene::SceneSystems&) {
        if (ScriptRun* run = FindRun(*this)) run->Remove(*this);
    }

    void ScriptComponent::OnPropertiesChanged(const Assets::AssetManager&) {
        if (m_Started) ResolveReferences();
    }

    void ScriptComponent::ResolveReferences() {
        ResolveFields(m_Type.Properties, m_Instance, GetOwner());
    }

    void ScriptComponent::CallStart() {
        if (m_Broken) {
            if (ScriptRun* run = FindRun(*this)) {
                run->ReportFault("Script '" + m_Type.Name + "' on '" + GetOwner().GetName() +
                                 "' crashed in its constructor");
            }
            return;
        }
        m_Started = true;
        ResolveReferences();
        Guard("OnStart", [&] { m_Type.Calls->Start(m_Script); });
    }

    void ScriptComponent::CallUpdate(f32 deltaTime) {
        if (m_Type.UsesUpdate) Guard("OnUpdate", [&] { m_Type.Calls->Update(m_Script, deltaTime); });
    }

    void ScriptComponent::CallLateUpdate(f32 deltaTime) {
        if (m_Type.UsesLateUpdate) Guard("OnLateUpdate", [&] { m_Type.Calls->LateUpdate(m_Script, deltaTime); });
    }

    void ScriptComponent::CallStop(vke::StopReason reason) {
        m_Started = false;
        Guard("OnStop", [&] { m_Type.Calls->Stop(m_Script, reason); });
    }

    void ScriptComponent::CallCollision(vke::detail::ContactPhase phase, const vke::Collision& collision) {
        Guard("a collision callback", [&] { m_Type.Calls->Collision(m_Script, phase, collision); });
    }

    void ScriptComponent::CallTrigger(vke::detail::ContactPhase phase, vke::Object other) {
        Guard("a trigger callback", [&] { m_Type.Calls->Trigger(m_Script, phase, other); });
    }

} // namespace VKE::Scripting
