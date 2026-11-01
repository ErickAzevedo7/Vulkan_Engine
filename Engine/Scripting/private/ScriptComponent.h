#pragma once

#include "ScriptType.h"

#include <core/Types.h>
#include <scene/Component.h>

#include <vke/Script.h>

#include <memory>
#include <string>

namespace VKE::Scripting {

    // The engine's side of a script on an object: a component holding the instance in the game's
    // DLL, which the registry's tables read and write. Every script type shares this class, each
    // with its own entry in the registry.
    //
    // Everything the engine asks of a script goes through the Guard function, so a script that
    // faults only stops the game. A script that couldn't be constructed, or that faulted, is
    // broken: it runs no more. If it couldn't be constructed, a plain copy of the default instance
    // holds its fields instead, which the tables reach all the same, so its values aren't lost.
    class ScriptComponent final : public Scene::Component {
    public:
        explicit ScriptComponent(const ScriptType& type);
        ~ScriptComponent() override;

        // `component` as a script, or null.
        static ScriptComponent* From(Scene::Component& component);

        Scene::ComponentTypeID GetTypeID() const override { return m_Type.ID; }
        void* GetInstance() const { return m_Instance; }
        bool IsBroken() const { return m_Broken; }

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
        // Runs `call`, which is part of the function called `function`, unless the script is broken.
        template<typename Call>
        void Guard(const char* function, const Call& call);

        // Points the ObjectRef fields at their objects, for the ones that hold one.
        void ResolveReferences();

        void ConnectToSystems(Scene::SceneSystems& systems) override;
        void DisconnectFromSystems(Scene::SceneSystems& systems) override;
        void OnPropertiesChanged(const Assets::AssetManager& assets) override;

        const ScriptType& m_Type;
        void* m_Instance = nullptr;
        vke::Script* m_Script = nullptr;
        std::unique_ptr<char[]> m_FailedInstance;
        bool m_Broken = false;
        size_t m_RunIndex = 0;
        bool m_Started = false;
    };

} // namespace VKE::Scripting
