#pragma once

#include <string>

// Forward declarations
class Scene;
class ResourceContext;

/// Handles saving and loading of scene data to/from .iscene JSON files.
class ProjectSerializer {
public:
    /// Save the active scene to a file.
    /// @param filePath  Full path including filename (e.g. "projects/myScene.iscene")
    /// @param scene     The scene to serialize
    /// @param resources Resource context for material/mesh lookup
    /// @return true on success
    static bool save(const std::string& filePath, Scene* scene, ResourceContext& resources);

    /// Load a scene from file, replacing the current active scene contents.
    /// @param filePath  Full path including filename
    /// @param scene     The scene to populate (will be cleared first)
    /// @param resources Resource context for material/mesh reconstruction
    /// @return true on success
    static bool load(const std::string& filePath, Scene* scene, ResourceContext& resources);
};
