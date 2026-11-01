#pragma once

#include <vke/Script.h>
#include <vke/detail/GameInfo.h>

#include <type_traits>

// How the engine calls into a script, through plain functions the game's DLL gives it.

namespace vke::detail {

    inline const ScriptCalls& ScriptTables::GetCalls() {
        static const ScriptCalls kCalls = {
            [](Script* script, Object owner) { script->m_Owner = owner; },
            [](Script* script) { script->OnStart(); },
            [](Script* script, float deltaTime) { script->OnUpdate(deltaTime); },
            [](Script* script, float deltaTime) { script->OnLateUpdate(deltaTime); },
            [](Script* script, StopReason reason) { script->OnStop(reason); },
            [](Script* script) { script->OnReload(); },
            [](Script* script, ContactPhase phase, const Collision& collision) {
                switch (phase) {
                case ContactPhase::Start: script->OnCollisionStart(collision); break;
                case ContactPhase::Stay: script->OnCollisionStay(collision); break;
                case ContactPhase::End: script->OnCollisionEnd(collision); break;
                }
            },
            [](Script* script, ContactPhase phase, Object other) {
                switch (phase) {
                case ContactPhase::Start: script->OnTriggerStart(other); break;
                case ContactPhase::Stay: script->OnTriggerStay(other); break;
                case ContactPhase::End: script->OnTriggerEnd(other); break;
                }
            },
        };
        return kCalls;
    }

    // A class that overrides a function names it as its own: the engine then calls it every frame.
    template<typename T>
    constexpr bool ScriptTables::UsesUpdate() {
        return !std::is_same_v<decltype(&T::OnUpdate), void (Script::*)(float)>;
    }

    template<typename T>
    constexpr bool ScriptTables::UsesLateUpdate() {
        return !std::is_same_v<decltype(&T::OnLateUpdate), void (Script::*)(float)>;
    }

} // namespace vke::detail
