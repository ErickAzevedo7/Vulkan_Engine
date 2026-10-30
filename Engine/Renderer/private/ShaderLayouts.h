#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>

// C++ copies of the data shaders read. Members are only Vec4, UVec4 and Mat4,
// so the layout matches the shaders without extra padding.

namespace VKE::Renderer {

    // Bind group indices. 3 is still free.
    constexpr u32 kFrameSet = 0;
    constexpr u32 kEnvironmentSet = 1;
    constexpr u32 kMaterialSet = 2;

    // Slots of the frame bind group. Slot 0 holds the FrameUniforms.
    constexpr u32 kLightListBinding = 1;
    constexpr u32 kClusterListBinding = 2;
    constexpr u32 kLightIndexListBinding = 3;
    constexpr u32 kShadowListBinding = 4;

    // Slots of the environment bind group.
    constexpr u32 kIrradianceBinding = 0;
    constexpr u32 kPrefilterBinding = 1;
    constexpr u32 kBrdfLutBinding = 2;
    constexpr u32 kRadianceBinding = 3;
    constexpr u32 kShadowAtlasBinding = 4;

    // The grid of light clusters: across the screen, down it, and in depth. The shader gets
    // them from FrameUniforms::ClusterCounts.
    constexpr u32 kClusterColumns = 16;
    constexpr u32 kClusterRows = 9;
    constexpr u32 kClusterSlices = 24;

    // Must match kPointShadowNearPlane in shaders/forward.frag.
    constexpr f32 kPointShadowNearPlane = 0.05f;

    // Material textures use bindings 0-5 in Assets::MaterialTexture order.
    constexpr u32 kMaterialUniformsBinding = 6;

    struct GpuLight {
        Vec4 PositionAndRange;
        Vec4 DirectionAndType;
        Vec4 ColorAndIntensity;
        // x = cosine of the inner cone angle, y = cosine of the outer one, z = shadow index,
        // or -1 without a shadow: the first of six for a point light, one per face in the
        // order +X, -X, +Y, -Y, +Z, -Z.
        Vec4 SpotAndShadow;
    };
    static_assert(sizeof(GpuLight) == 64, "GpuLight layout mismatch");

    struct GpuShadow {
        Mat4 ViewProjection;
        // x = size of a shadow map texel in world units, one unit in front of the light
        // for perspective shadows. Multiplied by clip w, it works for both projections.
        Vec4 Parameters;
        // Where it is in the shadow atlas, in UVs: xy = corner, zw = size.
        Vec4 AtlasRect;
    };
    static_assert(sizeof(GpuShadow) == 96, "GpuShadow layout mismatch");

    // Where a cluster's lights are in the list of light indices.
    struct GpuCluster {
        u32 Offset = 0;
        u32 Count = 0;
    };
    static_assert(sizeof(GpuCluster) == 8, "GpuCluster layout mismatch");

    struct FrameUniforms {
        Mat4 View;
        Mat4 Projection;
        Vec4 CameraPosition;
        // x = directional lights, the first ones in the list of lights.
        UVec4 LightCounts;
        // x, y, z = clusters across the screen, down it and in depth.
        UVec4 ClusterCounts;
        // x, y = clusters per pixel. z, w = scale and bias of a pixel's slice: log(depth) * z + w.
        Vec4 ClusterScale;
        // x = environment intensity.
        Vec4 EnvironmentParameters;
    };
    static_assert(sizeof(FrameUniforms) == 208, "FrameUniforms layout mismatch");

    struct MaterialUniforms {
        Vec4 BaseColor;
        Vec4 EmissiveAndOcclusion;
        Vec4 MetallicRoughness;
    };
    static_assert(sizeof(MaterialUniforms) == 48, "MaterialUniforms layout mismatch");

    // Used by the passes that render one cubemap face at a time.
    struct CapturePushConstants {
        Mat4 ViewProjection;
        // Per-pass values, such as the roughness of a prefiltered mip.
        Vec4 Parameters;
    };
    static_assert(sizeof(CapturePushConstants) == 80, "CapturePushConstants layout mismatch");

    struct ShadowPushConstants {
        Mat4 ModelViewProjection;
    };
    static_assert(sizeof(ShadowPushConstants) == 64, "ShadowPushConstants layout mismatch");

    struct ObjectPushConstants {
        Mat4 Model;
        Vec4 NormalMatrix[3];
    };
    static_assert(sizeof(ObjectPushConstants) == 112, "ObjectPushConstants layout mismatch");

} // namespace VKE::Renderer
