#pragma once

#include <core/Handle.h>
#include <core/Types.h>

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
        explicit ScriptRun(Scene::Scene& scene);
        ~ScriptRun();

        ScriptRun(const ScriptRun&) = delete;
        ScriptRun& operator=(const ScriptRun&) = delete;

        // Null while no game runs.
        static ScriptRun* GetCurrent() { return s_Current; }
        Scene::Scene& GetScene() const { return m_Scene; }

        void Update(f32 deltaTime);

        // A script joined or left the scene. Leaving tells it to stop, if it had started.
        void Add(ScriptComponent& script);
        void Remove(ScriptComponent& script);

        // Whether scripts are in OnStop, when objects can't get new scripts: the component list
        // of the object going away may be in use.
        bool IsStopping() const { return m_Stopping > 0; }
        // Happens at the end of the frame.
        void QueueDestroy(Handle<Scene::GameObject> object);

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
        // In the order they joined. A script that left is null until the end of the frame.
        std::vector<ScriptComponent*> m_Scripts;
        bool m_HasGaps = false;
        std::vector<Handle<Scene::GameObject>> m_ToDestroy;
        // Reused for the scripts of an object receiving a contact.
        std::vector<ScriptComponent*> m_ContactScripts;
        u32 m_Stopping = 0;
    };

} // namespace VKE::Scripting
