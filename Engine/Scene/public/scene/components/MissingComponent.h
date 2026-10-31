#pragma once

#include <scene/Component.h>

#include <string>

namespace VKE::Scene {

    // Stands in for a component whose type isn't there, like a script whose game didn't load. It
    // does nothing and keeps what the file said about the component, which saving writes back, so
    // the values return with the type.
    class MissingComponent final : public Component {
    public:
        explicit MissingComponent(ComponentTypeID type) : m_Type(type) {}

        ComponentTypeID GetTypeID() const override { return m_Type; }

        // The component's YAML as a scene or prefab file had it.
        const std::string& GetSavedText() const { return m_SavedText; }
        void SetSavedText(std::string text) { m_SavedText = std::move(text); }

    private:
        ComponentTypeID m_Type;
        std::string m_SavedText;
    };

} // namespace VKE::Scene
