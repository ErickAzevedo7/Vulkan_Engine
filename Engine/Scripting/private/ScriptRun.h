#pragma once

#include "scripting/ScriptRunner.h"

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>

#include <string>
#include <vector>

namespace VKE::Scene {
    struct Contact;
    class GameObject;
    class Scene;
} // namespace VKE::Scene

namespace VKE::Scripting {

    class ScriptComponent;

    // One run of a scene as a game. Exists between ScriptRunner::Start and Stop, and only one at
    // a time: the functions scripts call find their scene through GetCurrent.
    class ScriptRun {
    public:
        ScriptRun(Scene::Scene& scene, const GameContext& context);
        ~ScriptRun();

        ScriptRun(const ScriptRun&) = delete;
        ScriptRun& operator=(const ScriptRun&) = delete;

        // Null while no game runs.
        static ScriptRun* GetCurrent() { return s_Current; }
        Scene::Scene& GetScene() const { return m_Scene; }
        const GameContext& GetContext() const { return m_Context; }
        Scene::PrefabLibrary& GetPrefabs() const { return m_Context.Prefabs; }
        const Platform::IInput& GetInput() const { return m_Context.Input; }

        void SetInputArea(bool focused, f32 viewX, f32 viewY);
        bool HasInputFocus() const { return m_InputFocused; }
        f32 GetViewX() const { return m_ViewX; }
        f32 GetViewY() const { return m_ViewY; }

        f32 GetDeltaTime() const { return m_DeltaTime; }
        f32 GetTime() const { return m_Time; }
        i32 GetFrame() const { return m_Frame; }

        void Update(f32 deltaTime);

        // A script joined or left the scene. Leaving tells it to stop, if it had started.
        void Add(ScriptComponent& script);
        void Remove(ScriptComponent& script);

        // Whether scripts are in OnStop, when objects can't get new scripts: the component list
        // of the object going away may be in use.
        bool IsStopping() const { return m_Stopping > 0; }
        // Happens at the end of the frame.
        void QueueDestroy(Handle<Scene::GameObject> object);

        // Scripts asked for these; the runner acts on them after the frame.
        void RequestScene(Assets::AssetID scene) { m_RequestedScene = scene; }
        Assets::AssetID GetRequestedScene() const { return m_RequestedScene; }
        void RequestQuit() { m_QuitRequested = true; }
        bool IsQuitRequested() const { return m_QuitRequested; }

        // A script crashed. No script runs after this. The first report is the one kept.
        void ReportFault(std::string message);
        bool HasFault() const { return m_HasFault; }
        const std::string& GetFault() const { return m_Fault; }

    private:
        void StartNewScripts();
        void DeliverContacts();
        void DeliverContact(const Scene::Contact& contact, Handle<Scene::GameObject> self,
                            Handle<Scene::GameObject> other, bool flipNormal);
        void DestroyQueued();
        // Closes the gaps left by scripts that left.
        void CompactScripts();
        void StopScript(ScriptComponent& script, bool gameEnded);

        // Calls `call` with every script that has started and whose object is active.
        template<typename Call>
        void ForEachRunning(const Call& call);

        static ScriptRun* s_Current;

        Scene::Scene& m_Scene;
        GameContext m_Context;
        // In the order they joined. A script that left is null until the end of the frame.
        std::vector<ScriptComponent*> m_Scripts;
        bool m_HasGaps = false;
        std::vector<Handle<Scene::GameObject>> m_ToDestroy;
        // Reused for the scripts of an object receiving a contact.
        std::vector<ScriptComponent*> m_ContactScripts;
        u32 m_Stopping = 0;
        Assets::AssetID m_RequestedScene;
        bool m_QuitRequested = false;
        bool m_HasFault = false;
        std::string m_Fault;
        bool m_InputFocused = true;
        f32 m_ViewX = 0.0f;
        f32 m_ViewY = 0.0f;
        f32 m_DeltaTime = 0.0f;
        f32 m_Time = 0.0f;
        i32 m_Frame = 0;
    };

} // namespace VKE::Scripting
