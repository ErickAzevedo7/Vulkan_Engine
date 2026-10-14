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

    // Slots of the environment bind group.
    constexpr u32 kIrradianceBinding = 0;
    constexpr u32 kPrefilterBinding = 1;
    constexpr u32 kBrdfLutBinding = 2;
    constexpr u32 kRadianceBinding = 3;
    constexpr u32 kShadowMapBinding = 4;
    constexpr u32 kPointShadowMapBinding = 5;

    // Must match MAX_LIGHTS in shaders/frame_uniforms.glsl.
    constexpr u32 kMaxLights = 8;
    // Must match MAX_SHADOWS in shaders/frame_uniforms.glsl. One shadow map layer each.
    constexpr u32 kMaxShadows = 4;
    // Cubes in the point light shadow array.
    constexpr u32 kMaxPointShadows = 4;
    // Must match kPointShadowNearPlane in shaders/forward.frag.
    constexpr f32 kPointShadowNearPlane = 0.05f;

    // Material textures use bindings 0-5 in Assets::MaterialTexture order.
    constexpr u32 kMaterialUniformsBinding = 6;

    struct GpuLight {
        Vec4 PositionAndRange;
        Vec4 DirectionAndType;
        Vec4 ColorAndIntensity;
        // x = cosine of the inner cone angle, y = cosine of the outer one, z = shadow index,
        // or -1 without a shadow: into FrameUniforms::Shadows for directional and spot
        // lights, and the cube in the point shadow array for point lights.
        Vec4 SpotAndShadow;
    };
    static_assert(sizeof(GpuLight) == 64, "GpuLight layout mismatch");

    struct GpuShadow {
        Mat4 ViewProjection;
        // x = size of a shadow map texel in world units, one unit in front of the light
        // for perspective shadows. Multiplied by clip w, it works for both projections.
        // y = shadow map layer, kept as a float so the shader never converts it.
        Vec4 Parameters;
    };
    static_assert(sizeof(GpuShadow) == 80, "GpuShadow layout mismatch");

    struct FrameUniforms {
        Mat4 View;
        Mat4 Projection;
        Vec4 CameraPosition;
        UVec4 LightCount;
        // x = environment intensity.
        Vec4 EnvironmentParameters;
        GpuLight Lights[kMaxLights];
        GpuShadow Shadows[kMaxShadows];
    };
    static_assert(sizeof(FrameUniforms) == 1008, "FrameUniforms layout mismatch");

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
