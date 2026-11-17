#include "ScriptRun.h"

#include "ObjectHandles.h"
#include "ScriptComponent.h"
#include "scripting/GameModule.h"

#include <core/Assert.h>
#include <core/Profiling.h>
#include <scene/GameObject.h>
#include <scene/Prefab.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <vke/Script.h>

#include <algorithm>

namespace VKE {

namespace {
void CollectScripts(GameObject &object, std::vector<ScriptComponent *> &out) {
	for (Component *component : object.GetComponents()) {
		if (ScriptComponent *script = ScriptComponent::From(*component)) {
			out.push_back(script);
		}
	}
	for (GameObject *child : object.GetChildren()) {
		CollectScripts(*child, out);
	}
}

vke::detail::ContactPhase ToPhase(ContactPhase phase) {
	switch (phase) {
		case ContactPhase::Begin:
			return vke::detail::ContactPhase::Start;
		case ContactPhase::Stay:
			return vke::detail::ContactPhase::Stay;
		case ContactPhase::End:
			break;
	}
	return vke::detail::ContactPhase::End;
}
} // namespace

ScriptRun *ScriptRun::s_Current = nullptr;

ScriptRun::ScriptRun(Scene &scene, const GameContext &context) : m_Scene(scene), m_Context(context) {
	VKE_CORE_ASSERT(!s_Current, "Only one game runs at a time");
	s_Current = this;

	std::vector<ScriptComponent *> scripts;
	for (GameObject *root : m_Scene.GetRootObjects()) {
		CollectScripts(*root, scripts);
	}
	for (ScriptComponent *script : scripts) {
		Add(*script);
	}
}

ScriptRun::~ScriptRun() {
	for (ScriptComponent *script : m_Scripts) {
		if (script) {
			StopScript(*script, true);
		}
	}
	s_Current = nullptr;
}

template <typename Call> void ScriptRun::ForEachRunning(const Call &call) {
	// By index, and only up to the scripts there are now: a script added meanwhile starts
	// next frame, and adding can move the list.
	size_t count = m_Scripts.size();
	for (size_t i = 0; i < count && !m_HasFault; ++i) {
		ScriptComponent *script = m_Scripts[i];
		if (script && script->IsStarted() && script->GetOwner().IsActiveInHierarchy()) {
			call(*script);
		}
	}
}

void ScriptRun::SetInputArea(bool focused, f32 viewX, f32 viewY) {
	m_InputFocused = focused;
	m_ViewX = viewX;
	m_ViewY = viewY;
}

void ScriptRun::Update(f32 deltaTime) {
	VKE_PROFILE_FUNCTION();
	m_DeltaTime = deltaTime;
	m_Time += deltaTime;
	++m_Frame;

	StartNewScripts();
	{
		VKE_PROFILE_SCOPE("Scripts: OnUpdate");
		ForEachRunning([&](ScriptComponent &script) { script.CallUpdate(deltaTime); });
	}
	if (m_HasFault) {
		return;
	}
	m_Scene.Update();
	DeliverContacts();
	{
		VKE_PROFILE_SCOPE("Scripts: OnLateUpdate");
		ForEachRunning([&](ScriptComponent &script) { script.CallLateUpdate(deltaTime); });
	}
	if (m_HasFault) {
		return;
	}
	DestroyQueued();
	CompactScripts();
	// Sends what the scripts changed after the physics step.
	m_Scene.Sync();
}

void ScriptRun::SwitchGame(GameModule &game) {
	struct KeptScript {
		ScriptComponent *Script;
		std::string Name;
		std::string Values;
	};

	// A script the new code doesn't have ends now, while its own code is there.
	for (size_t i = 0; i < m_Scripts.size(); ++i) {
		ScriptComponent *script = m_Scripts[i];
		if (script && !game.WillHaveScript(script->GetTypeName())) {
			script->GetOwner().RemoveComponent(script->GetTypeID());
		}
	}

	AssetManager &assets = GetPrefabs().GetAssets();
	std::vector<KeptScript> kept;
	for (ScriptComponent *script : m_Scripts) {
		if (!script) {
			continue;
		}
		kept.push_back({ script, script->GetTypeName(), SceneSerializer::ComponentToText(*script, assets) });
		script->DropInstance();
	}
	GetPrefabs().ForgetAll();
	game.Switch();

	for (KeptScript &script : kept) {
		if (script.Script->MakeInstance() && !script.Script->IsBroken()) {
			SceneSerializer::LoadComponentText(*script.Script, assets, script.Values);
		} else {
			ReportFault("Script '" + script.Name + "' on '" + script.Script->GetOwner().GetName() +
					"' crashed in its constructor");
		}
	}

	// Once they are all back, as they can reach each other.
	for (KeptScript &script : kept) {
		if (!m_HasFault && script.Script->IsStarted()) {
			script.Script->CallReload();
		}
	}
}

void ScriptRun::Add(ScriptComponent &script) {
	script.SetRunIndex(m_Scripts.size());
	m_Scripts.push_back(&script);
}

void ScriptRun::Remove(ScriptComponent &script) {
	m_Scripts[script.GetRunIndex()] = nullptr;
	m_HasGaps = true;
	StopScript(script, false);
}

void ScriptRun::QueueDestroy(Handle<GameObject> object) {
	m_ToDestroy.push_back(object);
}

void ScriptRun::ReportFault(std::string message) {
	if (m_HasFault) {
		return;
	}
	m_HasFault = true;
	m_Fault = std::move(message);
}

void ScriptRun::StartNewScripts() {
	VKE_PROFILE_FUNCTION();
	size_t count = m_Scripts.size();
	for (size_t i = 0; i < count && !m_HasFault; ++i) {
		ScriptComponent *script = m_Scripts[i];
		if (script && !script->IsStarted() && script->GetOwner().IsActiveInHierarchy()) {
			script->CallStart();
		}
	}
}

void ScriptRun::DeliverContacts() {
	VKE_PROFILE_FUNCTION();
	for (const Contact &contact : m_Scene.GetContacts()) {
		if (m_HasFault) {
			return;
		}
		DeliverContact(contact, contact.A, contact.B, false);
		DeliverContact(contact, contact.B, contact.A, true);
	}
}

void ScriptRun::DeliverContact(
		const Contact &contact, Handle<GameObject> self, Handle<GameObject> other, bool flipNormal) {
	GameObject *object = m_Scene.Get(self);
	if (!object || !object->IsActiveInHierarchy()) {
		return;
	}

	// Copied: a script can add another to the object.
	m_ContactScripts.clear();
	for (Component *component : object->GetComponents()) {
		if (ScriptComponent *script = ScriptComponent::From(*component)) {
			m_ContactScripts.push_back(script);
		}
	}

	vke::detail::ContactPhase phase = ToPhase(contact.Phase);
	vke::Object otherObject = ToObject(other);
	for (ScriptComponent *script : m_ContactScripts) {
		if (!script->IsStarted() || m_HasFault) {
			continue;
		}
		if (contact.IsTrigger) {
			script->CallTrigger(phase, otherObject);
		} else {
			Vec3 normal = flipNormal ? -contact.Normal : contact.Normal;
			script->CallCollision(phase, { otherObject, { normal.x, normal.y, normal.z } });
		}
	}
}

void ScriptRun::DestroyQueued() {
	VKE_PROFILE_FUNCTION();
	// By index: an OnStop can ask for more. Asking twice, or for what went with a parent,
	// finds nothing.
	for (size_t i = 0; i < m_ToDestroy.size(); ++i) {
		if (GameObject *object = m_Scene.Get(m_ToDestroy[i])) {
			m_Scene.Destroy(*object);
		}
	}
	m_ToDestroy.clear();
}

void ScriptRun::CompactScripts() {
	if (!m_HasGaps) {
		return;
	}
	m_Scripts.erase(std::remove(m_Scripts.begin(), m_Scripts.end(), nullptr), m_Scripts.end());
	for (size_t i = 0; i < m_Scripts.size(); ++i) {
		m_Scripts[i]->SetRunIndex(i);
	}
	m_HasGaps = false;
}

void ScriptRun::StopScript(ScriptComponent &script, bool gameEnded) {
	// After a fault the scripts' state can't be trusted.
	if (!script.IsStarted() || m_HasFault) {
		return;
	}
	++m_Stopping;
	script.CallStop(gameEnded ? vke::StopReason::GameEnded : vke::StopReason::Destroyed);
	--m_Stopping;
}

} // namespace VKE
