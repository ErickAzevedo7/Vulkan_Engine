#pragma once

namespace VKE::Scene {
    class GameObject;
} // namespace VKE::Scene

namespace VKE::Editor {

    class EditorScene;

    // The items that create objects (empty, shapes, camera, lights), for the GameObject menu and
    // the Hierarchy's menus; call inside an open menu. What they create goes last under `parent`,
    // or at the root, and is selected. Returns the object created this frame, or null.
    Scene::GameObject* DrawCreateObjectItems(EditorScene& scene, Scene::GameObject* parent);

} // namespace VKE::Editor
