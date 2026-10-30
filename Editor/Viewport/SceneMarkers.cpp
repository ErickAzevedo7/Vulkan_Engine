#include "SceneMarkers.h"
#include "WorldShapes.h"

#include <assets/AssetManager.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/TransformComponent.h>
#include <ui/Style.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <vector>

namespace VKE::Editor {

    namespace Colors = UI::Colors;
    using Renderer::WorldDepth;
    using Renderer::WorldStyle;

    namespace {
        constexpr f32 kTwoPi = 6.28318530718f;
        constexpr f32 kIconRadius = 15.0f;
        constexpr f32 kIconHalfSize = 11.0f;
        // How far a camera's view is drawn, in meters.
        constexpr f32 kFrustumLength = 3.0f;

        void ForEachObject(const Scene::Scene& scene, const std::function<void(Scene::GameObject&)>& visit) {
            std::function<void(Scene::GameObject&)> walk = [&](Scene::GameObject& object) {
                visit(object);
                for (Scene::GameObject* child : object.GetChildren()) {
                    walk(*child);
                }
            };
            for (Scene::GameObject* root : scene.GetRootObjects()) {
                walk(*root);
            }
        }

        AABB TransformBounds(const AABB& bounds, const Mat4& transform) {
            AABB result(Vec3(FLT_MAX), Vec3(-FLT_MAX));
            for (u32 corner = 0; corner < 8; ++corner) {
                Vec3 point((corner & 1) ? bounds.Max.x : bounds.Min.x, (corner & 2) ? bounds.Max.y : bounds.Min.y,
                           (corner & 4) ? bounds.Max.z : bounds.Min.z);
                Vec3 world = Vec3(transform * Vec4(point, 1.0f));
                result.Min = glm::min(result.Min, world);
                result.Max = glm::max(result.Max, world);
            }
            return result;
        }

        void AddWireBox(Renderer::WorldDrawList& list, const AABB& bounds, u32 color, f32 width) {
            auto corner = [&](u32 i) {
                return Vec3((i & 1) ? bounds.Max.x : bounds.Min.x, (i & 2) ? bounds.Max.y : bounds.Min.y,
                            (i & 4) ? bounds.Max.z : bounds.Min.z);
            };
            // Every pair of corners that differ in one axis.
            for (u32 i = 0; i < 8; ++i) {
                for (u32 bit : {1u, 2u, 4u}) {
                    if (!(i & bit)) list.AddLine(corner(i), corner(i | bit), color, width);
                }
            }
        }
    } // namespace

    SceneMarkers::SceneMarkers(Assets::AssetManager& assets) : m_Assets(assets) {
    }

    void SceneMarkers::Draw(const Scene::Scene& scene, const Scene::GameObject* selected, const ScreenView& view,
                            Renderer::WorldDrawList& list) {
        if (ShowGrid) DrawGrid(view, list);
        if (selected) DrawSelected(*selected, view, list);
        if (ShowIcons) DrawIcons(scene, selected, view, list);
        list.SetStyle({});
    }

    void SceneMarkers::DrawGrid(const ScreenView& view, Renderer::WorldDrawList& list) const {
        // Cells of 1, 10, 100 meters... as the camera rises, with lines ten times apart ten
        // times brighter, fading out with the distance.
        f32 height = std::max(std::abs(view.Position.y), 1.0f);
        f32 cell = std::pow(10.0f, std::floor(std::log10(height)));
        constexpr i32 kMinorLines = 40;
        constexpr i32 kMajorLines = 15;

        auto drawLines = [&](f32 step, i32 count, u32 color, bool skipMajor) {
            f32 centerX = std::round(view.Position.x / step) * step;
            f32 centerZ = std::round(view.Position.z / step) * step;
            f32 extent = step * static_cast<f32>(count);
            for (i32 i = -count; i <= count; ++i) {
                f32 x = centerX + step * static_cast<f32>(i);
                f32 z = centerZ + step * static_cast<f32>(i);
                // Lines on a major line, or on an axis, are drawn by those.
                bool xMajor = skipMajor && std::abs(std::remainder(x, step * 10.0f)) < step * 0.5f;
                bool zMajor = skipMajor && std::abs(std::remainder(z, step * 10.0f)) < step * 0.5f;
                if (!xMajor && std::abs(x) > step * 0.5f) {
                    list.AddLine(Vec3(x, 0.0f, centerZ - extent), Vec3(x, 0.0f, centerZ + extent), color, 1.0f);
                }
                if (!zMajor && std::abs(z) > step * 0.5f) {
                    list.AddLine(Vec3(centerX - extent, 0.0f, z), Vec3(centerX + extent, 0.0f, z), color, 1.0f);
                }
            }
        };

        WorldStyle style;
        style.Depth = WorldDepth::Tested;
        style.FadeStart = cell * 10.0f;
        style.FadeEnd = cell * 40.0f;
        list.SetStyle(style);
        drawLines(cell, kMinorLines, ToVertexColor(Colors::ControlBorder, 0.35f), true);

        style.FadeStart = cell * 40.0f;
        style.FadeEnd = cell * 150.0f;
        list.SetStyle(style);
        drawLines(cell * 10.0f, kMajorLines, ToVertexColor(Colors::ControlBorder, 0.7f), false);

        // The X and Z axes through the origin.
        f32 extent = cell * 150.0f;
        list.AddLine(Vec3(view.Position.x - extent, 0.0f, 0.0f), Vec3(view.Position.x + extent, 0.0f, 0.0f),
                     ToVertexColor(Colors::AxisX, 0.75f), 1.5f);
        list.AddLine(Vec3(0.0f, 0.0f, view.Position.z - extent), Vec3(0.0f, 0.0f, view.Position.z + extent),
                     ToVertexColor(Colors::AxisZ, 0.75f), 1.5f);
    }

    const MarkerIcon* SceneMarkers::GetIcon(const Scene::GameObject& object) const {
        if (const auto* light = object.GetComponent<Scene::LightComponent>()) {
            switch (light->GetSettings().Type) {
                case Renderer::LightType::Directional: return &m_Icons.Directional;
                case Renderer::LightType::Point: return &m_Icons.Point;
                case Renderer::LightType::Spot: return &m_Icons.Spot;
            }
        }
        if (object.HasComponent<Scene::CameraComponent>()) return &m_Icons.Camera;
        return nullptr;
    }

    void SceneMarkers::DrawIcons(const Scene::Scene& scene, const Scene::GameObject* selected, const ScreenView& view,
                                 Renderer::WorldDrawList& list) const {
        struct Entry {
            const Scene::GameObject* Object;
            const MarkerIcon* Icon;
            Vec3 Position;
            f32 Depth;
        };
        std::vector<Entry> entries;
        ForEachObject(scene, [&](Scene::GameObject& object) {
            const MarkerIcon* icon = object.IsActiveInHierarchy() ? GetIcon(object) : nullptr;
            if (!icon) return;
            Vec3 position = object.GetTransform().GetWorld().Position;
            f32 depth = glm::dot(position - view.Position, view.Forward);
            if (depth > 0.05f) entries.push_back({&object, icon, position, depth});
        });

        // Far to near, so nearer icons cover farther ones.
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.Depth > b.Depth; });

        WorldStyle discStyle;
        discStyle.Depth = WorldDepth::OnTop;
        WorldStyle iconStyle = discStyle;
        iconStyle.Texture = m_Icons.Texture;
        for (const Entry& entry : entries) {
            f32 pixel = view.GetPixelSize(entry.Position);
            bool isSelected = entry.Object == selected;

            list.SetStyle(discStyle);
            u32 disc = ToVertexColor(isSelected ? Colors::Accent : Colors::Overlay, isSelected ? 0.9f : 1.0f);
            f32 radius = kIconRadius * pixel;
            Vec3 previous = entry.Position + view.Right * radius;
            for (u32 i = 1; i <= 24; ++i) {
                f32 angle = kTwoPi * static_cast<f32>(i) / 24.0f;
                Vec3 point = entry.Position + (view.Right * std::cos(angle) + view.Up * std::sin(angle)) * radius;
                list.AddTriangle(entry.Position, previous, point, disc);
                previous = point;
            }

            list.SetStyle(iconStyle);
            Vec3 right = view.Right * (kIconHalfSize * pixel * entry.Icon->Shape.x);
            Vec3 up = view.Up * (kIconHalfSize * pixel * entry.Icon->Shape.y);
            Vec3 corners[4] = {entry.Position - right + up, entry.Position + right + up, entry.Position + right - up,
                               entry.Position - right - up};
            u32 color = ToVertexColor(isSelected ? Colors::OnAccent : Colors::Text);
            list.AddQuad(corners, color, entry.Icon->UVMin, entry.Icon->UVMax);
        }
    }

    Scene::GameObject* SceneMarkers::PickIcon(const Scene::Scene& scene, const ScreenView& view,
                                              const Vec2& pixel) const {
        if (!ShowIcons) return nullptr;
        Scene::GameObject* best = nullptr;
        f32 bestDepth = FLT_MAX;
        ForEachObject(scene, [&](Scene::GameObject& object) {
            if (!object.IsActiveInHierarchy() || !GetIcon(object)) return;
            Vec3 position = object.GetTransform().GetWorld().Position;
            Vec2 screen;
            if (!view.ToScreen(position, screen) || glm::length(screen - pixel) > kIconRadius) return;
            f32 depth = glm::dot(position - view.Position, view.Forward);
            if (depth < bestDepth) {
                bestDepth = depth;
                best = &object;
            }
        });
        return best;
    }

    const AABB* SceneMarkers::GetMeshBounds(Handle<Assets::MeshData> mesh) {
        auto it = m_MeshBounds.find(mesh);
        if (it != m_MeshBounds.end()) return &it->second;
        const Assets::MeshData* data = m_Assets.GetMesh(mesh);
        if (!data || data->Vertices.empty()) return nullptr;

        AABB bounds(Vec3(FLT_MAX), Vec3(-FLT_MAX));
        for (const Assets::Vertex& vertex : data->Vertices) {
            bounds.Min = glm::min(bounds.Min, vertex.Position);
            bounds.Max = glm::max(bounds.Max, vertex.Position);
        }
        return &m_MeshBounds.emplace(mesh, bounds).first->second;
    }

    bool SceneMarkers::GetBounds(const Scene::GameObject& object, AABB& bounds) {
        bool found = false;
        std::function<void(const Scene::GameObject&)> walk = [&](const Scene::GameObject& current) {
            if (const auto* mesh = current.GetComponent<Scene::MeshComponent>()) {
                if (const AABB* local = GetMeshBounds(mesh->GetMesh())) {
                    AABB world = TransformBounds(*local, current.GetTransform().GetWorldMatrix());
                    if (found) {
                        bounds.Merge(world);
                    } else {
                        bounds = world;
                        found = true;
                    }
                }
            }
            for (const Scene::GameObject* child : current.GetChildren()) {
                walk(*child);
            }
        };
        walk(object);
        return found;
    }

    void SceneMarkers::DrawSelected(const Scene::GameObject& object, const ScreenView& view,
                                    Renderer::WorldDrawList& list) {
        WorldStyle style;
        style.Depth = WorldDepth::OnTop;
        list.SetStyle(style);

        AABB bounds;
        if (GetBounds(object, bounds)) AddWireBox(list, bounds, ToVertexColor(Colors::Accent, 0.55f), 1.0f);

        const Transform& world = object.GetTransform().GetWorld();
        Vec3 forward = world.GetForward();
        Vec3 right = world.GetRight();
        Vec3 up = world.GetUp();
        u32 color = ToVertexColor(Colors::Warning, 0.7f);
        u32 faint = ToVertexColor(Colors::Warning, 0.35f);

        if (const auto* light = object.GetComponent<Scene::LightComponent>()) {
            const Scene::LightSettings& settings = light->GetSettings();
            switch (settings.Type) {
                case Renderer::LightType::Point:
                    for (const Vec3& axis : {right, up, forward}) {
                        AddCircle(list, world.Position, axis, settings.Range, color, 1.0f, 64);
                    }
                    break;
                case Renderer::LightType::Spot: {
                    f32 radius = std::tan(std::min(settings.OuterConeAngle, 1.5f)) * settings.Range;
                    f32 inner = std::tan(std::min(settings.InnerConeAngle, 1.5f)) * settings.Range;
                    Vec3 end = world.Position + forward * settings.Range;
                    AddCircle(list, end, forward, radius, color, 1.0f, 48);
                    AddCircle(list, end, forward, inner, faint, 1.0f, 48);
                    for (const Vec3& side : {right, -right, up, -up}) {
                        list.AddLine(world.Position, end + side * radius, color, 1.0f);
                    }
                    break;
                }
                case Renderer::LightType::Directional: {
                    // Rays along the light's direction, from a disc around the object.
                    f32 radius = 0.5f;
                    f32 length = 2.0f;
                    AddCircle(list, world.Position, forward, radius, color, 1.0f, 32);
                    for (u32 i = 0; i < 8; ++i) {
                        f32 angle = kTwoPi * static_cast<f32>(i) / 8.0f;
                        Vec3 start = world.Position + (right * std::cos(angle) + up * std::sin(angle)) * radius;
                        list.AddLine(start, start + forward * length, faint, 1.0f);
                    }
                    list.AddLine(world.Position, world.Position + forward * (length * 1.5f), color, 1.5f);
                    break;
                }
            }
        }

        if (const auto* camera = object.GetComponent<Scene::CameraComponent>()) {
            // The view out to a few meters, at the Viewport's shape.
            f32 halfHeight = std::tan(camera->FieldOfView * 0.5f) * kFrustumLength;
            f32 halfWidth = halfHeight * view.Size.x / view.Size.y;
            Vec3 center = world.Position + forward * kFrustumLength;
            Vec3 across = right * halfWidth;
            Vec3 above = up * halfHeight;
            Vec3 corners[4] = {center - across + above, center + across + above, center + across - above,
                               center - across - above};
            for (int i = 0; i < 4; ++i) {
                list.AddLine(world.Position, corners[i], color, 1.0f);
                list.AddLine(corners[i], corners[(i + 1) % 4], color, 1.0f);
            }
            // A mark on the top edge, to tell up from down.
            Vec3 top = center + up * (halfHeight * 1.25f);
            list.AddLine(corners[0] + right * (halfWidth * 0.6f), top, color, 1.0f);
            list.AddLine(top, corners[1] - right * (halfWidth * 0.6f), color, 1.0f);
        }
    }

} // namespace VKE::Editor
