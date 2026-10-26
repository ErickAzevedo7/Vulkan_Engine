#pragma once

#include <filesystem>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {

    class Scene;

    // Saves scenes to .scene files and loads them back. A file lists every object with its ID,
    // name, parent and components, each component by its registered type name and the fields it
    // visits. Objects refer to each other by ID and to assets by asset ID.
    class SceneSerializer {
    public:
        // Returns false if the file can't be written. Assets made in code have no asset ID, so
        // fields pointing at them are left out, with a warning.
        static bool Save(const Scene& scene, const Assets::AssetManager& assets, const std::filesystem::path& file);

        // Adds the file's objects to `scene`, with the IDs they were saved with. Returns false if
        // the file can't be read or isn't a scene. An object whose ID repeats one earlier in the
        // file, or one already in the scene, is skipped with its children, with a warning.
        static bool Load(Scene& scene, Assets::AssetManager& assets, const std::filesystem::path& file);
    };

} // namespace VKE::Scene
