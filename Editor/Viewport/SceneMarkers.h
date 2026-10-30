#pragma once

#include "Gizmo/GizmoMath.h"

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/AABB.h>
#include <math/MathTypes.h>
#include <renderer/Overlay.h>
#include <renderer/WorldDraw.h>

#include <unordered_map>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {
    class GameObject;
    class Scene;
} // namespace VKE::Scene

namespace VKE::Editor {

    // Where an icon is in a texture.
    struct MarkerIcon {
        Vec2 UVMin{0.0f};
        Vec2 UVMax{0.0f};
        // Width and height over the larger of the two, so the icon keeps its shape.
        Vec2 Shape{1.0f};
    };

    // The icons markers show for objects that draw nothing themselves.
    struct MarkerIcons {
        Renderer::OverlayTexture Texture;
        MarkerIcon Directional;
        MarkerIcon Point;
        MarkerIcon Spot;
        MarkerIcon Camera;
    };

    // What the Viewport draws over the scene besides the gizmo: a grid on the ground, icons for
    // lights and cameras, and for the selected object its bounds and the reach of its light or
    // camera.
    class SceneMarkers {
    public:
        explicit SceneMarkers(Assets::AssetManager& assets);

        void SetIcons(const MarkerIcons& icons) { m_Icons = icons; }

        bool ShowGrid = true;
        bool ShowIcons = true;

        void Draw(const Scene::Scene& scene, const Scene::GameObject* selected, const ScreenView& view,
                  Renderer::WorldDrawList& list);

        // The object whose icon is under `pixel`, nearest the camera, or null.
        Scene::GameObject* PickIcon(const Scene::Scene& scene, const ScreenView& view, const Vec2& pixel) const;

        // The world bounds of the meshes of `object` and its children. False if none has one.
        bool GetBounds(const Scene::GameObject& object, AABB& bounds);

    private:
        void DrawGrid(const ScreenView& view, Renderer::WorldDrawList& list) const;
        void DrawIcons(const Scene::Scene& scene, const Scene::GameObject* selected, const ScreenView& view,
                       Renderer::WorldDrawList& list) const;
        void DrawSelected(const Scene::GameObject& object, const ScreenView& view, Renderer::WorldDrawList& list);
        // Null for objects without an icon.
        const MarkerIcon* GetIcon(const Scene::GameObject& object) const;
        // A mesh's own bounds, computed once.
        const AABB* GetMeshBounds(Handle<Assets::MeshData> mesh);

        Assets::AssetManager& m_Assets;
        MarkerIcons m_Icons;
        std::unordered_map<Handle<Assets::MeshData>, AABB> m_MeshBounds;
    };

} // namespace VKE::Editor
