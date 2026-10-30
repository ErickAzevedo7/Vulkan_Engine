#pragma once

#include "Gizmo/GizmoMath.h"
#include "Gizmo/TransformGizmo.h"
#include "Viewport/EditorCamera.h"
#include "Viewport/SceneMarkers.h"

#include <core/Types.h>
#include <renderer/Overlay.h>
#include <renderer/RenderTypes.h>
#include <renderer/WorldDraw.h>

#include <string>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::Scene {
    class Scene;
} // namespace VKE::Scene

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    class EditorScene;

    // Shows the scene from the editor's camera, drawn into a view texture the size of the
    // panel, with the grid, the icons and the gizmo over it. The mouse flies, orbits and pans
    // the camera, drags the gizmo, and selects what it clicks. A model dragged from Assets is
    // placed where it is dropped.
    //
    //   Right button: look around, WASD/QE to fly, wheel for speed   Alt + left: orbit
    //   Middle: pan   Wheel: zoom   F: frame the selected   W/E/R: move, rotate, scale
    class ViewportPanel {
    public:
        ViewportPanel(EditorScene& scene, Assets::AssetManager& assets, Renderer::Renderer& renderer,
                      UI::UISystem& ui);

        void Create();
        void Destroy();

        // The window's title, with its icon.
        static const char* GetWindowName();

        // `stats` shows in the bottom-left corner.
        void Draw(bool* open, const std::string& stats);

        // Whether the panel showed this frame, so the scene must be drawn into its texture.
        bool IsVisible() const { return m_Visible; }
        Renderer::ViewTextureID GetTexture() const { return m_Texture; }
        // Where to draw the scene from, and what to draw over it, this frame.
        Renderer::RenderView GetView() const { return m_Camera.GetView(); }
        const Renderer::WorldDrawList& GetWorldDrawList() const { return m_World; }

    private:
        enum class Navigation {
            None,
            Fly,
            Orbit,
            Pan,
        };

        void LoadIcons();
        // Puts the camera where the scene's main camera is, for a scene just opened.
        void PlaceCamera();
        void ReceivePick();
        void HandleCamera(bool hovered, f32 viewportHeight);
        void HandleShortcuts(bool hovered);
        // True while the gizmo has the mouse.
        bool HandleGizmo(const ScreenView& view, const Vec2& mouse, bool hovered);
        void HandleClick(const ScreenView& view, const Vec2& mouse, bool hovered, bool gizmoBusy);
        void FocusSelected();
        void DrawToolbar(const Vec2& min);

        EditorScene& m_Scene;
        Renderer::Renderer& m_Renderer;
        UI::UISystem& m_UI;
        Renderer::ViewTextureID m_Texture;
        bool m_Visible = false;

        EditorCamera m_Camera;
        // Where the camera was in the scene, while a prefab is open.
        EditorCamera m_SceneCamera;
        bool m_EditingPrefab = false;
        TransformGizmo m_Gizmo;
        SceneMarkers m_Markers;
        Renderer::WorldDrawList m_World;
        Navigation m_Navigation = Navigation::None;
        bool m_IconsLoaded = false;
        bool m_Snap = false;

        // The scene the camera was placed for (EditorScene::GetOpenCount), and the one the last
        // pick was asked in: a new scene places the camera again, and drops picks from the old one.
        u32 m_PlacedFor = ~0u;
        const Scene::Scene* m_PickedIn = nullptr;
        // A left press that becomes a click if the mouse hardly moves before it is released.
        bool m_ClickPending = false;
        Vec2 m_ClickStart{0.0f};
    };

} // namespace VKE::Editor
