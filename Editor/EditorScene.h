#pragma once

#include <assets/AssetTypes.h>
#include <core/Types.h>
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
    // saved to, and whether it changed since.
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

        // Invalid until the scene is saved.
        Assets::AssetID GetID() const { return m_ID; }
        const std::filesystem::path& GetFile() const { return m_File; }
        // "Sponza.scene", or "Untitled" until saved.
        std::string GetFileName() const;

        bool IsDirty() const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }

        // New objects go at the root of the scene and mark it changed.
        Scene::GameObject& CreateEmpty();
        // Null, logged, if the engine's content is missing the shape.
        Scene::GameObject* CreateShape(Shape shape);
        Scene::GameObject& CreateCamera();
        Scene::GameObject& CreateLight(Renderer::LightType type);

    private:
        Assets::AssetManager& m_Assets;
        Iunique<Scene::Scene> m_Scene;
        Assets::AssetID m_ID;
        std::filesystem::path m_File;
        bool m_Dirty = false;
    };

} // namespace VKE::Editor
