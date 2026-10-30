#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>

#include <filesystem>
#include <string>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {
    class GameObject;
    class Scene;
} // namespace VKE::Scene

namespace VKE::Editor {

    // The engine's shapes, in Content/Shapes.
    enum class Shape {
        Cube,
        Sphere,
        Plane,
        Cylinder,
    };

    // The scene open in the editor: its objects, the scene asset it was opened from and is
    // saved to, whether it changed since, and what is selected: an object or an asset.
    class EditorScene {
    public:
        explicit EditorScene(Assets::AssetManager& assets);
        ~EditorScene();

        // An unsaved scene with a camera and a light.
        void New();
        // False, logged, if the asset isn't a scene or can't be read; the open scene stays.
        bool Open(Assets::AssetID scene);
        // Writes the scene to its file. False if it has none yet or the write fails.
        bool Save();
        // Writes the scene to `file`, which must be inside the project's assets, and makes it
        // the scene's file. False, logged, if it isn't or the write fails.
        bool SaveAs(const std::filesystem::path& file);

        Scene::Scene& GetScene() { return *m_Scene; }
        const Scene::Scene& GetScene() const { return *m_Scene; }
        Assets::AssetManager& GetAssets() { return m_Assets; }

        // Invalid until the scene is saved.
        Assets::AssetID GetID() const { return m_ID; }
        // Where the scene asset is now, which follows it when it is moved; empty until saved, or
        // once its file is deleted.
        std::filesystem::path GetFile() const;
        // "Sponza.scene", or "Untitled" until saved.
        std::string GetFileName() const;

        bool IsDirty() const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }

        // The selected object, or null. Nothing is selected after New and Open.
        Scene::GameObject* GetSelected() const;
        // Selecting anything, or nothing, deselects the asset.
        void Select(Scene::GameObject* object);
        // The selected asset, or invalid. Selecting one deselects the object.
        Assets::AssetID GetSelectedAsset() const { return m_SelectedAsset; }
        void SelectAsset(Assets::AssetID asset);

        // New objects go last under `parent`, or at the root of the scene, and mark it changed.
        Scene::GameObject& CreateEmpty(Scene::GameObject* parent = nullptr);
        // Null, logged, if the engine's content is missing the shape.
        Scene::GameObject* CreateShape(Shape shape, Scene::GameObject* parent = nullptr);
        Scene::GameObject& CreateCamera(Scene::GameObject* parent = nullptr);
        Scene::GameObject& CreateLight(Renderer::LightType type, Scene::GameObject* parent = nullptr);
        // The objects of a model asset, last under `parent` or at the root of the scene, with the
        // root at `position` in its parent's space. Null, logged, if it isn't a model or can't load.
        Scene::GameObject* InstantiateModel(Assets::AssetID model, Scene::GameObject* parent = nullptr,
                                            const Vec3& position = Vec3(0.0f));
        // Destroys the object and its children, and marks the scene changed.
        void Destroy(Scene::GameObject& object);

    private:
        Assets::AssetManager& m_Assets;
        Iunique<Scene::Scene> m_Scene;
        Assets::AssetID m_ID;
        bool m_Dirty = false;
        Handle<Scene::GameObject> m_Selected;
        Assets::AssetID m_SelectedAsset;
    };

} // namespace VKE::Editor
