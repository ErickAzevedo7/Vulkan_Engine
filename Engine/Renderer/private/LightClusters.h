#pragma once

#include "ShaderLayouts.h"

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>

#include <array>
#include <vector>

namespace VKE::Renderer {

    // Sorts a frame's lights by where they reach, so a pixel only computes the ones near it.
    //
    // The view is cut into a grid of clusters: a grid over the screen, repeated in slices that
    // get thicker with distance. Each cluster lists the point and spot lights that touch it.
    // Directional lights reach everything and are in no cluster. There is no limit on the number
    // of lights: the lists are as long as the scene needs. Lights that touch no cluster can't
    // light anything on screen, and are left out of the frame.
    class LightClusters {
    public:
        // Fills the lists below for `view` drawn with `projection` into an output of `width` by
        // `height` pixels, and writes in `frame` what the shader needs to read them.
        void Build(const std::vector<LightDesc>& lights, const RenderView& view, const Mat4& projection, u32 width,
                   u32 height, FrameUniforms& frame);

        // The lights the frame uses, directional ones first.
        std::vector<GpuLight>& GetLights() { return m_Lights; }
        // What each entry of GetLights() was made from.
        const std::vector<const LightDesc*>& GetSources() const { return m_Sources; }
        // Where each cluster's lights are in GetIndices().
        const std::vector<GpuCluster>& GetClusters() const { return m_Clusters; }
        // Indices into GetLights().
        const std::vector<u32>& GetIndices() const { return m_Indices; }

    private:
        struct Hit {
            u32 Cluster;
            u32 Light;
        };

        void AddLight(const LightDesc& light);
        // Adds to m_Hits every cluster that a sphere touches, given in view space with the camera
        // looking down -Z. Returns whether there is one.
        bool FindClusters(const Vec3& center, f32 radius, u32 light, f32 nearPlane, f32 farPlane, f32 tanX, f32 tanY);

        std::vector<GpuLight> m_Lights;
        std::vector<const LightDesc*> m_Sources;
        std::vector<GpuCluster> m_Clusters;
        std::vector<u32> m_Indices;
        std::vector<Hit> m_Hits;
        // Where each slice starts in depth, and where the last one ends.
        std::array<f32, kClusterSlices + 1> m_SliceDepths{};
        f32 m_SliceScale = 0.0f;
        f32 m_SliceBias = 0.0f;
    };

} // namespace VKE::Renderer
