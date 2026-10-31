#pragma once

#include "ScriptType.h"

#include <scene/Component.h>

namespace VKE::Scripting {

    // The engine's side of a script on an object: a component holding the instance in the game's
    // DLL, which the registry's tables read and write. Every script type shares this class, each
    // with its own entry in the registry.
    class ScriptComponent final : public Scene::Component {
    public:
        explicit ScriptComponent(const ScriptType& type) : m_Type(type), m_Instance(type.Create()) {}
        ~ScriptComponent() override { m_Type.Destroy(m_Instance); }

        Scene::ComponentTypeID GetTypeID() const override { return m_Type.ID; }
        void* GetInstance() const { return m_Instance; }

    private:
        const ScriptType& m_Type;
        void* m_Instance;
    };

} // namespace VKE::Scripting
