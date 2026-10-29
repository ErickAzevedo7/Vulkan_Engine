#include "ObjectMenus.h"
#include "EditorScene.h"

#include <core/Log.h>
#include <scene/GameObject.h>
#include <ui/Icons.h>

#include <imgui.h>

#include <string>

namespace VKE::Editor {

    namespace Icons = UI::Icons;

    namespace {
        std::string WithIcon(const char* icon, const char* label) {
            return std::string(icon) + "  " + label;
        }
    } // namespace

    Scene::GameObject* DrawCreateObjectItems(EditorScene& scene, Scene::GameObject* parent) {
        Scene::GameObject* created = nullptr;
        if (ImGui::MenuItem(WithIcon(Icons::CircleDashed, "Create empty").c_str())) {
            created = &scene.CreateEmpty(parent);
        }
        if (ImGui::BeginMenu(WithIcon(Icons::Box, "3D object").c_str())) {
            if (ImGui::MenuItem("Cube")) created = scene.CreateShape(Shape::Cube, parent);
            if (ImGui::MenuItem("Sphere")) created = scene.CreateShape(Shape::Sphere, parent);
            if (ImGui::MenuItem("Plane")) created = scene.CreateShape(Shape::Plane, parent);
            if (ImGui::MenuItem("Cylinder")) created = scene.CreateShape(Shape::Cylinder, parent);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(WithIcon(Icons::Video, "Camera").c_str())) created = &scene.CreateCamera(parent);
        if (ImGui::BeginMenu(WithIcon(Icons::Lightbulb, "Light").c_str())) {
            if (ImGui::MenuItem(WithIcon(Icons::Sun, "Directional").c_str())) {
                created = &scene.CreateLight(Renderer::LightType::Directional, parent);
            }
            if (ImGui::MenuItem(WithIcon(Icons::Lightbulb, "Point").c_str())) {
                created = &scene.CreateLight(Renderer::LightType::Point, parent);
            }
            if (ImGui::MenuItem(WithIcon(Icons::Flashlight, "Spot").c_str())) {
                created = &scene.CreateLight(Renderer::LightType::Spot, parent);
            }
            ImGui::EndMenu();
        }

        if (created) {
            scene.Select(created);
            VKE_CORE_INFO("Editor: created '{}'", created->GetName());
        }
        return created;
    }

} // namespace VKE::Editor
