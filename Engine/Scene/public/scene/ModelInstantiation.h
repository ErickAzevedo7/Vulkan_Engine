#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {

    class GameObject;
    class Scene;

    // Builds a model's node hierarchy in `scene`, with a mesh component on each node that has
    // geometry. Returns the object made from the root node, named after the model, or null if
    // the model isn't loaded in `assets`.
    GameObject* InstantiateModel(Scene& scene, const Assets::AssetManager& assets, Handle<Assets::ModelData> model,
                                 GameObject* parent = nullptr);

} // namespace VKE::Scene
