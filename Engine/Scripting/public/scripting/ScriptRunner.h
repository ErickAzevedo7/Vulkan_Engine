#pragma once

#include <core/Types.h>

namespace VKE::Scene {
    class Scene;
} // namespace VKE::Scene

namespace VKE::Scripting {

    class ScriptRun;

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
        void Start(Scene::Scene& scene);
        // One frame, `deltaTime` seconds after the last. Call it instead of Scene::Update,
        // before rendering. Does nothing unless started.
        void Update(f32 deltaTime);
        // Tells every started script the game ended. Call before replacing the scene.
        void Stop();

    private:
        Iunique<ScriptRun> m_Run;
    };

} // namespace VKE::Scripting
