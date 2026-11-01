#pragma once

#include <assets/AssetTypes.h>
#include <core/Types.h>

#include <functional>
#include <string>

namespace VKE::Platform {
    class IInput;
} // namespace VKE::Platform

namespace VKE::Scene {
    class PrefabLibrary;
    class Scene;
} // namespace VKE::Scene

namespace VKE::Scripting {

    class ScriptRun;

    // What a game needs from the program running it.
    struct GameContext {
        Scene::PrefabLibrary& Prefabs;
        const Platform::IInput& Input;
        // A script asked for another scene. Replace the scene: Stop, then Start with the new one.
        std::function<void(Assets::AssetID scene)> LoadScene;
        // A script asked to end the game.
        std::function<void()> Quit;
        // A script crashed, as `message` says, and no script runs anymore. The memory may be
        // damaged: end the game, without saving anything from it.
        std::function<void(const std::string& message)> Fault;
    };

    // Runs a scene as a game: its scripts' OnStart, OnUpdate and the rest, with the scene's own
    // update in the middle. A scene with no scripts just updates.
    class ScriptRunner {
    public:
        ScriptRunner();
        // Stops, so declare it after the scene it runs.
        ~ScriptRunner();

        ScriptRunner(const ScriptRunner&) = delete;
        ScriptRunner& operator=(const ScriptRunner&) = delete;

        // The scripts of `scene` start at the next Update. Scripts added later join in by themselves.
        void Start(Scene::Scene& scene, const GameContext& context);
        // One frame, `deltaTime` seconds after the last. Call it instead of Scene::Update,
        // before rendering. Does nothing unless started. The context's functions run at its end.
        void Update(f32 deltaTime);
        // Tells every started script the game ended. Call before replacing the scene.
        void Stop();

        // Whether the keyboard and the mouse are the game's, and where its view starts in the
        // window. They are, from the corner of the window, unless this says otherwise.
        void SetInputArea(bool focused, f32 viewX, f32 viewY);

    private:
        Iunique<ScriptRun> m_Run;
        bool m_InputFocused = true;
        f32 m_ViewX = 0.0f;
        f32 m_ViewY = 0.0f;
    };

} // namespace VKE::Scripting
