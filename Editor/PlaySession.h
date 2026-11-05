#pragma once

#include <assets/AssetTypes.h>
#include <core/Types.h>
#include <scripting/ScriptRunner.h>

#include <string>

namespace VKE {
class AssetManager;
class GameModule;
class IInput;
class PrefabLibrary;
} // namespace VKE

namespace VKE::Editor {

struct SceneDocument;
class SceneRecovery;

// The open scene running as a game, with its scripts. Play keeps the scene as text in memory
// and Stop puts that text back, so everything that happened in between is lost.
class PlaySession {
public:
	PlaySession(SceneDocument &document, PrefabLibrary &prefabs, AssetManager &assets, SceneRecovery &recovery);

	// True while playing or paused.
	bool IsRunning() const { return m_State != State::Stopped; }
	bool IsPaused() const { return m_State == State::Paused; }
	// Whether the scene had unsaved changes when Play was pressed.
	bool WasDirtyAtPlay() const { return m_DirtyAtPlay; }

	// False, logged, if the scene can't be kept.
	bool Play();
	// Between playing and paused.
	void TogglePause();
	// Back to the scene as it was at Play, with the same object selected if it was there.
	void Stop();
	// Runs the game's frame, `deltaTime` seconds after the last.
	void Update(f32 deltaTime);
	// The objects stay and the scripts on them change, as ScriptRunner::SwitchGame does.
	void SwitchGame(GameModule &game);

	// What scripts read the keyboard and the mouse from.
	void SetInput(const IInput &input) { m_Input = &input; }
	// Whether they are the game's now, and where its view starts in the window.
	void SetInputArea(bool focused, f32 viewX, f32 viewY) { m_Runner.SetInputArea(focused, viewX, viewY); }
	// What a script that crashed said, once; empty if none did since the last call.
	std::string TakeFault();

private:
	enum class State {
		Stopped,
		Playing,
		Paused,
	};

	GameContext MakeGameContext();
	// A script asked for another scene. Stop still goes back to the one Play started from.
	void SwitchPlayedScene(AssetID scene);

	SceneDocument &m_Document;
	PrefabLibrary &m_Prefabs;
	AssetManager &m_Assets;
	SceneRecovery &m_Recovery;
	const IInput *m_Input = nullptr;
	State m_State = State::Stopped;
	std::string m_Snapshot;
	bool m_DirtyAtPlay = false;
	std::string m_Fault;
	// Last: it stops the scripts of a scene that is still there.
	ScriptRunner m_Runner;
};

} // namespace VKE::Editor
