#pragma once

#include "DrawItem.h"
#include "GpuCache.h"
#include "LightClusters.h"
#include "RenderTarget.h"
#include "ShadowPass.h"
#include "SkyboxPass.h"
#include "TonemapPass.h"

#include <core/Types.h>
#include <math/AABB.h>
#include <renderer/RenderTypes.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <unordered_map>
#include <vector>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {

    class RenderScene;

    // What the shaders read about one frame. The lists grow with the scene; a frame only
    // replaces its own, which the GPU has stopped reading by the time the frame starts.
    struct FrameData {
        Iunique<RHI::RHIBuffer> Uniforms;
        Iunique<RHI::RHIBuffer> Lights;
        Iunique<RHI::RHIBuffer> Clusters;
        Iunique<RHI::RHIBuffer> LightIndices;
        Iunique<RHI::RHIBuffer> Shadows;
        Iunique<RHI::RHIBindGroup> Group;
    };

    // What drawing a scene needs for one output: a high dynamic range color and depth target,
    // the frame's data and the tonemap's view of the target, one of each per frame in flight,
    // at the output's size.
    struct SceneTargets {
        std::vector<RenderTarget> Targets;
        std::vector<FrameData> Frames;
        std::vector<Iunique<RHI::RHIBindGroup>> TonemapSources;
        u32 Width = 0;
        u32 Height = 0;
    };

    // Draws a render scene in a single forward pass, with its shadows and sky, into a high
    // dynamic range target, then resolves that target into displayable color. Lights are
    // sorted into clusters of the view, so their number has no fixed limit.
    class ForwardPath {
    public:
        ForwardPath(RHI::RHIDevice& device, Assets::AssetManager& assets, u32 framesInFlight);
        ~ForwardPath();

        Iunique<SceneTargets> CreateTargets(u32 width, u32 height, const char* debugName);

        // Records the scene's shadows and the scene itself into targets.Targets[frameIndex].
        // Must be called outside any render pass.
        void Record(RHI::RHICommandBuffer& cmd, u32 frameIndex, const RenderScene& scene, const RenderView& view,
                    SceneTargets& targets);

        // Tonemaps what Record() drew into targets.Targets[frameIndex] over the whole output.
        // Must be called inside `outputPass`, which the caller begins and ends, so it can draw
        // more on top. `encodeSrgb` is for outputs whose format does not encode by itself.
        void Resolve(RHI::RHICommandBuffer& cmd, u32 frameIndex, SceneTargets& targets,
                     RHI::RHIRenderPass& outputPass, bool encodeSrgb);

        GpuCache& GetGpuCache() { return *m_GpuCache; }
        void SetExposure(f32 exposure) { m_Exposure = exposure; }
        void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }

    private:
        void CreatePipeline();
        void CollectDrawItems(const RenderScene& scene);
        RHI::RHIBindGroup& GetEnvironmentGroup(const RenderScene& scene);
        void WriteFrameData(FrameData& data, const RenderScene& scene, const RenderView& view, u32 width, u32 height);

        RHI::RHIDevice& m_Device;
        u32 m_FramesInFlight;

        // Destroyed in reverse declaration order.
        Iunique<RHI::RHIRenderPass> m_ScenePass;
        Iunique<RHI::RHIBindGroupLayout> m_FrameLayout;
        Iunique<RHI::RHIBindGroupLayout> m_EnvironmentLayout;
        Iunique<RHI::RHIBindGroupLayout> m_MaterialLayout;
        Iunique<RHI::RHIPipeline> m_Pipeline;

        Iunique<GpuCache> m_GpuCache;
        Iunique<ShadowPass> m_ShadowPass;
        Iunique<SkyboxPass> m_SkyboxPass;
        Iunique<TonemapPass> m_TonemapPass;

        Iunique<RHI::RHISampler> m_EnvironmentSampler;
        // One per environment a scene has used, kept until shutdown: a frame may record
        // several scenes, and an earlier one may still need its group.
        std::unordered_map<const GpuEnvironment*, Iunique<RHI::RHIBindGroup>> m_EnvironmentGroups;

        LightClusters m_LightClusters;
        std::vector<GpuShadow> m_Shadows;
        std::vector<DrawItem> m_DrawItems;
        // World-space bounds of everything in m_DrawItems.
        AABB m_SceneBounds;
        f32 m_Exposure = 1.0f;
        bool m_ShadowsEnabled = true;
    };

} // namespace VKE::Renderer
