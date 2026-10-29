#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <scene/ComponentRegistry.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {
    class Component;
    class GameObject;
    struct PropertyInfo;
    struct StructInfo;
} // namespace VKE::Scene

namespace VKE::Editor {

    class EditorScene;

    // The selected object: its name, whether it is on, and a block per component with the
    // properties the reflection generator describes, in their order. Components are added from
    // the registry and reset or removed from their block's menu. Every edit is applied to the
    // component as it happens.
    class InspectorPanel {
    public:
        InspectorPanel(EditorScene& scene, Assets::AssetManager& assets);

        // The window's title, with its icon.
        static const char* GetWindowName();

        void Draw(bool* open);

    private:
        void DrawObjectHeader(Scene::GameObject& object);
        void DrawComponent(Scene::Component& component);
        // Nested structs are listed inline. True if a value changed.
        bool DrawProperties(const Scene::StructInfo& type, void* object);
        bool DrawProperty(const Scene::PropertyInfo& property, void* value);
        bool DrawRotation(const char* label, const Scene::PropertyInfo& property, Quat& rotation);
        bool DrawMaterials(std::vector<Handle<Assets::MaterialData>>& materials);
        void DrawAddComponent(Scene::GameObject& object);

        EditorScene& m_Scene;
        Assets::AssetManager& m_Assets;
        // Component types whose blocks are closed, on every object.
        std::unordered_set<Scene::ComponentTypeID> m_Closed;
        std::string m_NameText;
        std::string m_AddFilter;

        // A rotation shown as angles: kept while the rotation stays the one they were made from,
        // so typing 180 doesn't turn into another set of angles for the same rotation.
        struct EulerCache {
            Handle<Scene::GameObject> Object;
            const Scene::PropertyInfo* Property = nullptr;
            Quat From{1.0f, 0.0f, 0.0f, 0.0f};
            Vec3 Degrees{0.0f};
        };
        EulerCache m_Euler;

        // The object whose properties are being drawn.
        Handle<Scene::GameObject> m_Object;
        // Changes to the components wait until they are drawn.
        Scene::ComponentTypeID m_ToRemove = Scene::kInvalidComponentType;
        Scene::ComponentTypeID m_ToReset = Scene::kInvalidComponentType;
    };

} // namespace VKE::Editor
