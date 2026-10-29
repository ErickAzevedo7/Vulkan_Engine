#include "Display.h"

#include <scene/Component.h>
#include <scene/GameObject.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/ColliderComponent.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/TransformComponent.h>
#include <ui/Icons.h>

#include <cctype>
#include <vector>

namespace VKE::Editor::Display {

    namespace Icons = UI::Icons;

    namespace {
        bool IsUpper(char c) {
            return std::isupper(static_cast<unsigned char>(c)) != 0;
        }

        bool IsLower(char c) {
            return std::islower(static_cast<unsigned char>(c)) != 0;
        }
    } // namespace

    std::string ToWords(std::string_view name) {
        // A word starts at a capital after a small letter or a digit, or at the last capital
        // of a run followed by a small letter: "HDRColor" is "HDR" and "Color".
        std::vector<std::string> words;
        std::string word;
        for (size_t i = 0; i < name.size(); ++i) {
            char c = name[i];
            if (c == '_' || c == ' ') {
                if (!word.empty()) words.push_back(std::move(word));
                word.clear();
                continue;
            }
            bool starts = !word.empty() && IsUpper(c) &&
                          (!IsUpper(word.back()) || (i + 1 < name.size() && IsLower(name[i + 1])));
            if (starts) {
                words.push_back(std::move(word));
                word.clear();
            }
            word += c;
        }
        if (!word.empty()) words.push_back(std::move(word));

        std::string result;
        for (size_t i = 0; i < words.size(); ++i) {
            std::string& part = words[i];
            // Acronyms keep their capitals; other words after the first start small.
            bool acronym = part.size() > 1 && IsUpper(part[0]) && IsUpper(part[1]);
            if (i > 0) {
                result += ' ';
                if (!acronym) part[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(part[0])));
            }
            result += part;
        }
        return result;
    }

    std::string GetComponentLabel(const Scene::ComponentTypeInfo& type) {
        std::string_view name = type.Name;
        constexpr std::string_view kSuffix = "Component";
        if (name.size() > kSuffix.size() && name.substr(name.size() - kSuffix.size()) == kSuffix) {
            name.remove_suffix(kSuffix.size());
        }
        return ToWords(name);
    }

    const char* GetComponentIcon(const Scene::Component& component) {
        using namespace Scene;
        ComponentTypeID type = component.GetTypeID();
        if (type == ComponentRegistry::GetID<TransformComponent>()) return Icons::Move;
        if (type == ComponentRegistry::GetID<MeshComponent>()) return Icons::Box;
        if (type == ComponentRegistry::GetID<CameraComponent>()) return Icons::Video;
        if (type == ComponentRegistry::GetID<ColliderComponent>()) return Icons::Scan;
        if (type == ComponentRegistry::GetID<LightComponent>()) {
            switch (static_cast<const LightComponent&>(component).GetSettings().Type) {
            case Renderer::LightType::Directional: return Icons::Sun;
            case Renderer::LightType::Point: return Icons::Lightbulb;
            case Renderer::LightType::Spot: return Icons::Flashlight;
            }
        }
        return Icons::SlidersHorizontal;
    }

    const char* GetObjectIcon(const Scene::GameObject& object) {
        for (const Scene::Component* component : object.GetComponents()) {
            if (component != &object.GetTransform()) return GetComponentIcon(*component);
        }
        return object.GetChildren().empty() ? Icons::CircleDashed : Icons::Boxes;
    }

} // namespace VKE::Editor::Display
