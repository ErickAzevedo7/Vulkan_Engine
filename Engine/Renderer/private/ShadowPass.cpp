#include "ShadowPass.h"
#include "CubeFaces.h"
#include "LightClusters.h"

#include <core/Log.h>
#include <platform/FileSystem.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace VKE::Renderer {

    namespace {
        // Clamped to what the device can make. Powers of two, like every tile.
        constexpr u32 kAtlasSize = 8192;
        // Largest tiles: a directional light always wants this one.
        constexpr u32 kMaxShadowSize = 2048;
        constexpr u32 kMaxPointFaceSize = 1024;
        // A cube face sees past its 90 degrees by this many texels on each side, so a filtered
        // lookup near the edge of a face stays inside the face's tile.
        constexpr u32 kPointFaceGutter = 2;
        constexpr RHI::Format kShadowMapFormat = RHI::Format::D32_SFLOAT;
        constexpr f32 kSpotNearPlane = 0.05f;
        // Keeps casters right at the edge of the scene bounds from being clipped.
        constexpr f32 kDirectionalPadding = 0.5f;

        bool TouchesSphere(const AABB& bounds, const Vec3& center, f32 radius) {
            Vec3 closest = glm::clamp(center, bounds.Min, bounds.Max);
            Vec3 offset = closest - center;
            return glm::dot(offset, offset) <= radius * radius;
        }

        Mat4 LookAlong(const Vec3& eye, const Vec3& direction) {
            Vec3 up = std::abs(direction.y) > 0.99f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
            return glm::lookAtRH(eye, eye + direction, up);
        }

        // Orthographic projection that covers the whole scene, seen along `direction`.
        Mat4 FitDirectional(const Vec3& direction, const AABB& bounds, u32 size, f32& texelSize) {
            Vec3 center = bounds.GetCenter();
            f32 radius = glm::length(bounds.GetExtents()) + kDirectionalPadding;
            Mat4 view = LookAlong(center - direction * radius, direction);

            Vec3 minimum(FLT_MAX);
            Vec3 maximum(-FLT_MAX);
            for (u32 corner = 0; corner < 8; ++corner) {
                Vec3 point((corner & 1) ? bounds.Max.x : bounds.Min.x, (corner & 2) ? bounds.Max.y : bounds.Min.y,
                           (corner & 4) ? bounds.Max.z : bounds.Min.z);
                Vec3 lightSpace = Vec3(view * Vec4(point, 1.0f));
                minimum = glm::min(minimum, lightSpace);
                maximum = glm::max(maximum, lightSpace);
            }
            minimum -= Vec3(kDirectionalPadding);
            maximum += Vec3(kDirectionalPadding);

            texelSize = std::max(maximum.x - minimum.x, maximum.y - minimum.y) / static_cast<f32>(size);
            // The light looks down -Z, so near and far are the negated Z range.
            Mat4 projection = glm::orthoRH_ZO(minimum.x, maximum.x, minimum.y, maximum.y, -maximum.z, -minimum.z);
            return projection * view;
        }

        Mat4 FitSpot(const LightDesc& light, const Vec3& direction, u32 size, f32& texelSize) {
            f32 halfAngle = glm::clamp(light.OuterConeAngle, glm::radians(1.0f), glm::radians(89.0f));
            f32 farPlane = std::max(light.Range, kSpotNearPlane * 2.0f);
            texelSize = 2.0f * std::tan(halfAngle) / static_cast<f32>(size);
            Mat4 projection = glm::perspectiveRH_ZO(2.0f * halfAngle, 1.0f, kSpotNearPlane, farPlane);
            return projection * LookAlong(light.Position, direction);
        }

        // The smallest tile, a power of two, with a texel per pixel of `pixels` on screen.
        u32 GetTileSize(f32 pixels, u32 largest) {
            u32 size = kMinShadowTileSize;
            while (size < largest && static_cast<f32>(size) < pixels) {
                size *= 2;
            }
            return size;
        }

        Vec4 ToAtlasRect(const ShadowTile& tile, u32 atlasSize) {
            f32 scale = 1.0f / static_cast<f32>(atlasSize);
            return Vec4(static_cast<f32>(tile.X), static_cast<f32>(tile.Y), static_cast<f32>(tile.Size),
                        static_cast<f32>(tile.Size)) *
                   scale;
        }
    } // namespace

    ShadowPass::ShadowPass(RHI::RHIDevice& device) {
        RHI::RenderPassDesc passDesc;
        passDesc.HasDepthAttachment = true;
        passDesc.DepthAttachment.Format = kShadowMapFormat;
        passDesc.DepthAttachment.IsDepth = true;
        passDesc.DepthAttachment.SampledAfter = true;
        m_RenderPass = RHI::RHIRenderPass::Create(device, passDesc);

        m_AtlasSize = std::min(kAtlasSize, device.GetLimits().MaxTextureSize);

        RHI::TextureDesc atlasDesc;
        atlasDesc.Width = m_AtlasSize;
        atlasDesc.Height = m_AtlasSize;
        atlasDesc.Format = kShadowMapFormat;
        atlasDesc.Usage = RHI::TextureUsage::DepthAttachment | RHI::TextureUsage::Sampled;
        atlasDesc.DebugName = "ShadowAtlas";
        m_Atlas = RHI::RHITexture::Create(device, atlasDesc);

        RHI::FramebufferDesc framebufferDesc;
        framebufferDesc.AttachmentViews = {m_Atlas->GetAttachmentView(0, 0)};
        framebufferDesc.Width = m_AtlasSize;
        framebufferDesc.Height = m_AtlasSize;
        m_Framebuffer = RHI::RHIFramebuffer::Create(device, *m_RenderPass, framebufferDesc);

        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        RHI::PipelineDesc pipelineDesc;
        pipelineDesc.VertexShaderPath = (shaderDir / "shadow.vert.spv").string();
        pipelineDesc.VertexStride = sizeof(Assets::Vertex);
        pipelineDesc.VertexAttributes = {{0, RHI::Format::R32G32B32_SFLOAT, offsetof(Assets::Vertex, Position)}};
        pipelineDesc.PushConstantRanges = {{RHI::ShaderStageFlags::Vertex, 0, sizeof(ShadowPushConstants)}};
        // Single-sided surfaces still have to block light coming from behind them.
        pipelineDesc.CullMode = RHI::CullMode::None;
        pipelineDesc.DepthTestEnable = true;
        pipelineDesc.DepthWriteEnable = true;
        pipelineDesc.DepthBiasEnable = true;
        pipelineDesc.DepthBiasConstant = 2.0f;
        pipelineDesc.DepthBiasSlope = 2.0f;
        m_Pipeline = RHI::RHIPipeline::Create(device, *m_RenderPass, pipelineDesc);

        RHI::SamplerDesc samplerDesc;
        samplerDesc.MipFilter = RHI::Filter::Nearest;
        samplerDesc.AddressModeU = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.AddressModeV = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.AddressModeW = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.MaxAnisotropy = 1.0f;
        samplerDesc.CompareEnable = true;
        samplerDesc.CompareOp = RHI::CompareOp::LessOrEqual;
        samplerDesc.DebugName = "ShadowSampler";
        m_Sampler = RHI::RHISampler::Create(device, samplerDesc);

        // The atlas is bound even on frames no light renders into it, so it starts ready to be
        // sampled.
        device.ExecuteImmediate([&](RHI::RHICommandBuffer& cmd) {
            cmd.BeginRenderPass(*m_RenderPass, *m_Framebuffer, RHI::ClearValue{});
            cmd.EndRenderPass();
        });
    }

    void ShadowPass::Prepare(LightClusters& lights, const Vec3& viewPosition, f32 pixelsAtOneMeter,
                             const AABB& sceneBounds, std::vector<GpuShadow>& outShadows) {
        m_Views.clear();
        m_Requests.clear();

        const std::vector<const LightDesc*>& sources = lights.GetSources();
        for (u32 i = 0; i < sources.size(); ++i) {
            const LightDesc& light = *sources[i];
            if (!light.CastsShadow) continue;

            ShadowRequest request;
            request.Light = i;
            if (light.Type == LightType::Directional) {
                // Directional lights reach the whole scene, so they come before all others.
                request.Size = kMaxShadowSize;
                request.Priority = FLT_MAX;
            } else {
                // How large what the light reaches looks on screen.
                f32 distance = std::max(glm::length(light.Position - viewPosition) - light.Range, 1.0f);
                f32 pixels = 2.0f * light.Range * pixelsAtOneMeter / distance;
                bool isPoint = light.Type == LightType::Point;
                request.FaceCount = isPoint ? 6 : 1;
                request.Size = GetTileSize(pixels, isPoint ? kMaxPointFaceSize : kMaxShadowSize);
                request.Priority = pixels;
            }
            m_Requests.push_back(request);
        }

        PackShadowAtlas(m_Requests, m_AtlasSize, m_Tiles);

        for (const ShadowRequest& request : m_Requests) {
            if (request.Size == 0) {
                if (!m_SpaceWarned) {
                    VKE_CORE_WARN("Renderer: the shadow atlas is full; the least important lights have no shadows");
                    m_SpaceWarned = true;
                }
                continue;
            }

            GpuLight& gpuLight = lights.GetLights()[request.Light];
            const LightDesc& light = *sources[request.Light];
            gpuLight.SpotAndShadow.z = static_cast<f32>(outShadows.size());

            if (light.Type == LightType::Point) {
                // The shader rebuilds depth from the same planes.
                f32 farPlane = std::max(light.Range, kPointShadowNearPlane * 2.0f);
                Mat4 projection = GetCubeFaceProjection(kPointShadowNearPlane, farPlane);

                // Widened so the face's own 90 degrees leave a border in the tile.
                f32 inner = static_cast<f32>(request.Size - 2 * kPointFaceGutter);
                f32 widen = inner / static_cast<f32>(request.Size);
                projection[0][0] *= widen;
                projection[1][1] *= widen;

                std::array<Mat4, 6> views = GetCubeFaceViews(light.Position);
                for (u32 face = 0; face < 6; ++face) {
                    const ShadowTile& tile = m_Tiles[request.FirstTile + face];
                    Mat4 viewProjection = projection * views[face];
                    m_Views.push_back({viewProjection, tile, true, light.Position, farPlane});
                    outShadows.push_back({viewProjection, Vec4(2.0f / inner, 0.0f, 0.0f, 0.0f),
                                          ToAtlasRect(tile, m_AtlasSize)});
                }
                continue;
            }

            f32 directionLength = glm::length(light.Direction);
            Vec3 direction = directionLength > 0.0f ? light.Direction / directionLength : Vec3(0.0f, -1.0f, 0.0f);

            f32 texelSize = 0.0f;
            bool isSpot = light.Type == LightType::Spot;
            Mat4 viewProjection = isSpot ? FitSpot(light, direction, request.Size, texelSize)
                                         : FitDirectional(direction, sceneBounds, request.Size, texelSize);

            const ShadowTile& tile = m_Tiles[request.FirstTile];
            m_Views.push_back({viewProjection, tile, isSpot, light.Position, light.Range});
            outShadows.push_back({viewProjection, Vec4(texelSize, 0.0f, 0.0f, 0.0f), ToAtlasRect(tile, m_AtlasSize)});
        }
    }

    void ShadowPass::Record(RHI::RHICommandBuffer& cmd, const std::vector<DrawItem>& items) {
        if (m_Views.empty()) return;

        cmd.BeginRenderPass(*m_RenderPass, *m_Framebuffer, RHI::ClearValue{});
        cmd.BindPipeline(*m_Pipeline);

        const GpuMesh* boundMesh = nullptr;
        for (const ShadowView& view : m_Views) {
            RHI::Viewport viewport;
            viewport.X = static_cast<f32>(view.Tile.X);
            viewport.Y = static_cast<f32>(view.Tile.Y);
            viewport.Width = static_cast<f32>(view.Tile.Size);
            viewport.Height = static_cast<f32>(view.Tile.Size);
            cmd.SetViewport(viewport);

            RHI::ScissorRect scissor;
            scissor.X = static_cast<i32>(view.Tile.X);
            scissor.Y = static_cast<i32>(view.Tile.Y);
            scissor.Width = view.Tile.Size;
            scissor.Height = view.Tile.Size;
            cmd.SetScissor(scissor);

            for (const DrawItem& item : items) {
                if (view.CullToRange && !TouchesSphere(item.WorldBounds, view.Position, view.Range)) continue;

                if (item.Mesh != boundMesh) {
                    cmd.BindVertexBuffer(*item.Mesh->VertexBuffer);
                    cmd.BindIndexBuffer(*item.Mesh->IndexBuffer, RHI::IndexType::U32);
                    boundMesh = item.Mesh;
                }

                ShadowPushConstants object;
                object.ModelViewProjection = view.ViewProjection * item.Instance->Transform;
                cmd.PushConstants(RHI::ShaderStageFlags::Vertex, 0, sizeof(object), &object);
                cmd.DrawIndexed(item.Submesh->IndexCount, 1, item.Submesh->FirstIndex, 0, 0);
            }
        }

        cmd.EndRenderPass();
    }

} // namespace VKE::Renderer
