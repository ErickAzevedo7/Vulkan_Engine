#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <reflection/Marks.h>
#include <vector>

namespace VKE::Renderer {

    // 60 degrees, the usual default for a game camera. Vertical, like every angle in RenderView.
    constexpr f32 kDefaultFieldOfView = glm::radians(60.0f);

    struct MeshInstanceDesc {
        Handle<Assets::MeshData> Mesh;
        // Replaces the mesh's materials, indexed like MeshData::Materials. Empty keeps the mesh's own.
        std::vector<Handle<Assets::MaterialData>> Materials;
        Mat4 Transform{1.0f};
        bool Visible = true;
    };

    VKE_ENUM()
    enum class LightType : u32 {
        Directional = 0,
        Point = 1,
        Spot = 2,
    };

    struct LightDesc {
        LightType Type = LightType::Directional;
        Vec3 Color{1.0f};
        f32 Intensity = 1.0f;
        // Point and spot lights.
        Vec3 Position{0.0f};
        f32 Range = 10.0f;
        // Directional and spot lights: the direction the light travels.
        Vec3 Direction{0.0f, -1.0f, 0.0f};
        // Spot lights, in radians from the center of the cone. Light fades from
        // full strength at the inner angle to nothing at the outer one.
        f32 InnerConeAngle = glm::radians(20.0f);
        f32 OuterConeAngle = glm::radians(30.0f);
        bool CastsShadow = false;
    };

    // What surrounds the scene, lighting it and showing behind everything else.
    struct EnvironmentDesc {
        // Equirectangular image, usually a .hdr. An invalid handle uses a stand-in sky.
        Handle<Assets::TextureData> Source;
        f32 Intensity = 1.0f;
    };

    using MeshInstanceID = Handle<MeshInstanceDesc>;
    using LightID = Handle<LightDesc>;

    // Where the scene is seen from. The projection uses the output's current aspect ratio.
    struct RenderView {
        Mat4 View{1.0f};
        Vec3 Position{0.0f};
        // Vertical, in radians.
        f32 FieldOfView = kDefaultFieldOfView;
        f32 NearPlane = 0.1f;
        f32 FarPlane = 1000.0f;
    };

} // namespace VKE::Renderer
