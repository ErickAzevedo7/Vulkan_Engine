#pragma once

#include "ScriptType.h"

#include <core/Types.h>
#include <scene/Component.h>

#include <vke/Script.h>

namespace VKE::Scripting {

    // The engine's side of a script on an object: a component holding the instance in the game's
    // DLL, which the registry's tables read and write. Every script type shares this class, each
    // with its own entry in the registry.
    //
    // Everything the engine asks of a script while the game runs goes through the Call functions.
    class ScriptComponent final : public Scene::Component {
    public:
        explicit ScriptComponent(const ScriptType& type);
        ~ScriptComponent() override;

        // `component` as a script, or null.
        static ScriptComponent* From(Scene::Component& component);

        Scene::ComponentTypeID GetTypeID() const override { return m_Type.ID; }
        void* GetInstance() const { return m_Instance; }

        bool IsStarted() const { return m_Started; }
        // Its place in the running game's list of scripts.
        size_t GetRunIndex() const { return m_RunIndex; }
        void SetRunIndex(size_t index) { m_RunIndex = index; }

        void CallStart();
        void CallUpdate(f32 deltaTime);
        void CallLateUpdate(f32 deltaTime);
        void CallStop(vke::StopReason reason);
        void CallCollision(vke::detail::ContactPhase phase, const vke::Collision& collision);
        void CallTrigger(vke::detail::ContactPhase phase, vke::Object other);

    private:
        void ConnectToSystems(Scene::SceneSystems& systems) override;
        void DisconnectFromSystems(Scene::SceneSystems& systems) override;

        const ScriptType& m_Type;
        void* m_Instance;
        vke::Script* m_Script;
        size_t m_RunIndex = 0;
        bool m_Started = false;
    };

} // namespace VKE::Scripting
