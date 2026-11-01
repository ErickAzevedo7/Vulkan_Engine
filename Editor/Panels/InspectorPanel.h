#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <scene/ComponentRegistry.h>

#include <imgui.h>

#include <functional>
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

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    class EditorScene;
    class MaterialEdits;

    // The selected object: its name, whether it is on, and a block per component with the
    // properties the reflection generator describes, in their order. Components are added from
    // the registry and reset or removed from their block's menu. Every edit is applied to the
    // component as it happens. An object of a prefab instance also shows its prefab, with a mark
    // on the properties that differ from it; a right click on them reverts or applies them.
    //
    // Or the selected asset: a material's values and textures, applied as they are edited and
    // saved to its .mat with Save; a texture's image and color space; what a model holds.
    class InspectorPanel {
    public:
        InspectorPanel(EditorScene& scene, Assets::AssetManager& assets, MaterialEdits& edits, UI::UISystem& ui);

        // The window's title, with its icon.
        static const char* GetWindowName();

        void Draw(bool* open);

    private:
        void DrawObjectHeader(Scene::GameObject& object);
        void DrawPrefabBar(Scene::GameObject& object);
        // A block's header, which opens and closes it. Returns its bottom right corner.
        ImVec2 DrawBlockHeader(const char* icon, const std::string& label, bool& open);
        void DrawComponent(Scene::Component& component);
        // Nested structs are listed inline. True if a value changed. `pristine` is the same
        // object in the prefab of `component`'s instance, or null.
        bool DrawProperties(Scene::Component& component, const Scene::StructInfo& type, void* object, void* pristine);
        // The mark on an overridden row that started at `top`, and its menu.
        bool DrawOverride(Scene::Component& component, const Scene::PropertyInfo& property, void* value, void* pristine,
                          f32 top);
        bool DrawProperty(Scene::Component& component, const Scene::PropertyInfo& property, void* value);
        // A field holding the AssetID of an asset of `type`.
        bool DrawAssetReference(const std::string& label, void* value, Assets::AssetType type, const char* icon,
                                const char* typeName);
        bool DrawRotation(const char* label, const Scene::PropertyInfo& property, Quat& rotation);
        bool DrawMaterials(std::vector<Handle<Assets::MaterialData>>& materials);
        void DrawAddComponent(Scene::GameObject& object);

        void DrawAsset(Assets::AssetID asset);
        void DrawAssetHeader(Assets::AssetID asset, Assets::AssetType type);
        // A block of an asset, open unless closed before. True while open; EndAssetBlock after.
        bool BeginAssetBlock(const char* icon, const char* label);
        void EndAssetBlock();
        void DrawMaterial(Assets::AssetID asset);
        void DrawTexture(Assets::AssetID asset);
        void DrawModel(Assets::AssetID asset);
        void DrawMesh(Assets::AssetID asset);
        // The texture's image, loaded in its own color space if it isn't loaded; 0 if it can't be.
        u64 GetTextureImage(Assets::AssetID texture);

        EditorScene& m_Scene;
        Assets::AssetManager& m_Assets;
        MaterialEdits& m_Edits;
        UI::UISystem& m_UI;
        // Asset blocks closed, by their label.
        std::unordered_set<std::string> m_ClosedAssetBlocks;
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
        // What changes a prefab waits too: it replaces the scene's objects.
        std::function<void()> m_PrefabChange;
    };

} // namespace VKE::Editor
