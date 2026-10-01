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

    // Must match MAX_LIGHTS in shaders/frame_uniforms.glsl.
    constexpr u32 kMaxLights = 8;

    // Material textures use bindings 0-5 in Assets::MaterialTexture order.
    constexpr u32 kMaterialUniformsBinding = 6;

    struct GpuLight {
        Vec4 PositionAndRange;
        Vec4 DirectionAndType;
        Vec4 ColorAndIntensity;
    };
    static_assert(sizeof(GpuLight) == 48, "GpuLight layout mismatch");

    struct FrameUniforms {
        Mat4 View;
        Mat4 Projection;
        Vec4 CameraPosition;
        UVec4 LightCount;
        // x = environment intensity.
        Vec4 EnvironmentParameters;
        GpuLight Lights[kMaxLights];
    };
    static_assert(sizeof(FrameUniforms) == 560, "FrameUniforms layout mismatch");

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

    struct ObjectPushConstants {
        Mat4 Model;
        Vec4 NormalMatrix[3];
    };
    static_assert(sizeof(ObjectPushConstants) == 112, "ObjectPushConstants layout mismatch");

} // namespace VKE::Renderer
