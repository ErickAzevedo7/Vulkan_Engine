#include "PlaySession.h"

#include "SceneDocument.h"
#include "SceneRecovery.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>

namespace VKE::Editor {

PlaySession::PlaySession(
		SceneDocument &document, PrefabLibrary &prefabs, AssetManager &assets, SceneRecovery &recovery) :
		m_Document(document), m_Prefabs(prefabs), m_Assets(assets), m_Recovery(recovery) {}

bool PlaySession::Play() {
	if (IsRunning()) {
		return true;
	}
	m_Snapshot = SceneSerializer::ToText(*m_Document.Objects, m_Prefabs);
	if (m_Snapshot.empty()) {
		return false;
	}
	m_DirtyAtPlay = m_Document.Dirty;
	m_State = State::Playing;
	if (m_Document.Dirty) {
		m_Recovery.Write(m_Snapshot, m_Document.ID);
	}
	m_Runner.Start(*m_Document.Objects, MakeGameContext());
	VKE_CORE_INFO(
			"Editor: Play: scene kept in memory ({} objects); Stop restores it", m_Document.Objects->GetObjectCount());
	return true;
}

GameContext PlaySession::MakeGameContext() {
	return { m_Prefabs, *m_Input, [this](AssetID scene) { SwitchPlayedScene(scene); }, [this] { Stop(); },
		[this](const std::string &message) {
			m_Fault = message;
			Stop();
		} };
}

void PlaySession::SwitchPlayedScene(AssetID scene) {
	auto loaded = MakeIunique<Scene>();
	if (m_Assets.GetType(scene) != AssetType::Scene ||
			!SceneSerializer::Load(*loaded, m_Prefabs, m_Assets.GetFile(scene))) {
		VKE_CORE_ERROR("Editor: the game asked for scene {}, which can't be loaded", scene.ToString());
		return;
	}
	m_Runner.Stop();
	m_Document.Objects = std::move(loaded);
	m_Document.Selected = {};
	m_Runner.Start(*m_Document.Objects, MakeGameContext());
}

void PlaySession::TogglePause() {
	if (IsRunning()) {
		m_State = m_State == State::Playing ? State::Paused : State::Playing;
	}
}

void PlaySession::Stop() {
	if (!IsRunning()) {
		return;
	}
	m_Runner.Stop();
	m_Recovery.Discard();
	if (!Reload(m_Document, m_Prefabs, m_Snapshot, "the scene kept at Play")) {
		VKE_CORE_ERROR("Editor: the scene kept at Play can't be restored; what is open stays");
		return;
	}
	m_Snapshot.clear();
	m_State = State::Stopped;
	m_Document.Dirty = m_DirtyAtPlay;
	VKE_CORE_INFO("Editor: Stop: scene restored");
}

void PlaySession::Update(f32 deltaTime) {
	m_Runner.Update(deltaTime);
}

void PlaySession::SwitchGame(GameModule &game) {
	m_Runner.SwitchGame(game);
}

std::string PlaySession::TakeFault() {
	std::string fault;
	fault.swap(m_Fault);
	return fault;
}

} // namespace VKE::Editor
