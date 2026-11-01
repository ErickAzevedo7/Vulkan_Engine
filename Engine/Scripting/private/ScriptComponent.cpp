#include "ScriptComponent.h"
#include "ObjectHandles.h"
#include "ScriptRun.h"

#include <scene/GameObject.h>
#include <scene/Scene.h>

namespace VKE::Scripting {

    namespace {
        // The running game, if this scene is what it runs: prefabs keep scenes of their own.
        ScriptRun* FindRun(const Scene::Component& component) {
            ScriptRun* run = ScriptRun::GetCurrent();
            return run && &run->GetScene() == &component.GetScene() ? run : nullptr;
        }
    } // namespace

    ScriptComponent::ScriptComponent(const ScriptType& type)
        : m_Type(type), m_Instance(type.Create()), m_Script(type.ToScript(m_Instance)) {
    }

    ScriptComponent::~ScriptComponent() {
        m_Type.Destroy(m_Instance);
    }

    ScriptComponent* ScriptComponent::From(Scene::Component& component) {
        const Scene::ComponentTypeInfo* type = Scene::ComponentRegistry::Find(component.GetTypeID());
        return type && type->Context ? static_cast<ScriptComponent*>(&component) : nullptr;
    }

    void ScriptComponent::ConnectToSystems(Scene::SceneSystems&) {
        m_Type.Calls->SetOwner(m_Script, ToObject(GetOwner().GetHandle()));
        if (ScriptRun* run = FindRun(*this)) run->Add(*this);
    }

    void ScriptComponent::DisconnectFromSystems(Scene::SceneSystems&) {
        if (ScriptRun* run = FindRun(*this)) run->Remove(*this);
    }

    void ScriptComponent::CallStart() {
        m_Started = true;
        m_Type.Calls->Start(m_Script);
    }

    void ScriptComponent::CallUpdate(f32 deltaTime) {
        if (m_Type.UsesUpdate) m_Type.Calls->Update(m_Script, deltaTime);
    }

    void ScriptComponent::CallLateUpdate(f32 deltaTime) {
        if (m_Type.UsesLateUpdate) m_Type.Calls->LateUpdate(m_Script, deltaTime);
    }

    void ScriptComponent::CallStop(vke::StopReason reason) {
        m_Started = false;
        m_Type.Calls->Stop(m_Script, reason);
    }

    void ScriptComponent::CallCollision(vke::detail::ContactPhase phase, const vke::Collision& collision) {
        m_Type.Calls->Collision(m_Script, phase, collision);
    }

    void ScriptComponent::CallTrigger(vke::detail::ContactPhase phase, vke::Object other) {
        m_Type.Calls->Trigger(m_Script, phase, other);
    }

} // namespace VKE::Scripting
