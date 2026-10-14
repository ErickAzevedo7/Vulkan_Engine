#include "ShadowPass.h"
#include "CubeFaces.h"

#include <core/Log.h>
#include <platform/FileSystem.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace VKE::Renderer {

    namespace {
        constexpr u32 kShadowMapSize = 2048;
        constexpr u32 kPointShadowMapSize = 512;
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
        Mat4 FitDirectional(const Vec3& direction, const AABB& bounds, f32& texelSize) {
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

            texelSize = std::max(maximum.x - minimum.x, maximum.y - minimum.y) / static_cast<f32>(kShadowMapSize);
            // The light looks down -Z, so near and far are the negated Z range.
            Mat4 projection = glm::orthoRH_ZO(minimum.x, maximum.x, minimum.y, maximum.y, -maximum.z, -minimum.z);
            return projection * view;
        }

        Mat4 FitSpot(const LightDesc& light, const Vec3& direction, f32& texelSize) {
            f32 halfAngle = glm::clamp(light.OuterConeAngle, glm::radians(1.0f), glm::radians(89.0f));
            f32 farPlane = std::max(light.Range, kSpotNearPlane * 2.0f);
            texelSize = 2.0f * std::tan(halfAngle) / static_cast<f32>(kShadowMapSize);
            Mat4 projection = glm::perspectiveRH_ZO(2.0f * halfAngle, 1.0f, kSpotNearPlane, farPlane);
            return projection * LookAlong(light.Position, direction);
        }
    } // namespace

    ShadowPass::ShadowPass(RHI::RHIDevice& device) {
        RHI::RenderPassDesc passDesc;
        passDesc.HasDepthAttachment = true;
        passDesc.DepthAttachment.Format = kShadowMapFormat;
        passDesc.DepthAttachment.IsDepth = true;
        passDesc.DepthAttachment.SampledAfter = true;
        m_RenderPass = RHI::RHIRenderPass::Create(device, passDesc);

        RHI::TextureDesc mapDesc;
        mapDesc.Width = kShadowMapSize;
        mapDesc.Height = kShadowMapSize;
        mapDesc.ArrayLayers = kMaxShadows;
        mapDesc.Format = kShadowMapFormat;
        mapDesc.Usage = RHI::TextureUsage::DepthAttachment | RHI::TextureUsage::Sampled;
        mapDesc.DebugName = "ShadowMaps";
        m_ShadowMaps = RHI::RHITexture::Create(device, mapDesc);

        for (u32 layer = 0; layer < kMaxShadows; ++layer) {
            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {m_ShadowMaps->GetAttachmentView(0, layer)};
            framebufferDesc.Width = kShadowMapSize;
            framebufferDesc.Height = kShadowMapSize;
            m_Framebuffers.push_back(RHI::RHIFramebuffer::Create(device, *m_RenderPass, framebufferDesc));
        }

        RHI::TextureDesc pointMapDesc;
        pointMapDesc.Width = kPointShadowMapSize;
        pointMapDesc.Height = kPointShadowMapSize;
        pointMapDesc.ArrayLayers = kMaxPointShadows * 6;
        pointMapDesc.Type = RHI::TextureType::TextureCube;
        pointMapDesc.Format = kShadowMapFormat;
        pointMapDesc.Usage = RHI::TextureUsage::DepthAttachment | RHI::TextureUsage::Sampled;
        pointMapDesc.DebugName = "PointShadowMaps";
        m_PointShadowMaps = RHI::RHITexture::Create(device, pointMapDesc);

        for (u32 layer = 0; layer < kMaxPointShadows * 6; ++layer) {
            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {m_PointShadowMaps->GetAttachmentView(0, layer)};
            framebufferDesc.Width = kPointShadowMapSize;
            framebufferDesc.Height = kPointShadowMapSize;
            m_PointFramebuffers.push_back(RHI::RHIFramebuffer::Create(device, *m_RenderPass, framebufferDesc));
        }

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

        // Layers no light renders into are still bound, so every layer starts
        // cleared to the far plane and ready to be sampled.
        device.ExecuteImmediate([&](RHI::RHICommandBuffer& cmd) {
            RHI::ClearValue clear;
            for (auto* framebuffers : {&m_Framebuffers, &m_PointFramebuffers}) {
                for (auto& framebuffer : *framebuffers) {
                    cmd.BeginRenderPass(*m_RenderPass, *framebuffer, clear);
                    cmd.EndRenderPass();
                }
            }
        });
    }

    void ShadowPass::Prepare(const std::vector<LightDesc>& lights, u32 lightCount, const AABB& sceneBounds,
                             FrameUniforms& frame) {
        m_Views.clear();
        u32 shadowCount = 0;
        u32 pointShadowCount = 0;

        for (u32 i = 0; i < lightCount; ++i) {
            const LightDesc& light = lights[i];
            if (!light.CastsShadow) continue;

            if (light.Type == LightType::Point) {
                if (pointShadowCount == kMaxPointShadows) {
                    if (!m_PointLimitWarned) {
                        VKE_CORE_WARN("Renderer: more than {} point lights cast shadows, the rest have none",
                                      kMaxPointShadows);
                        m_PointLimitWarned = true;
                    }
                    continue;
                }

                u32 cube = pointShadowCount++;
                // The shader rebuilds depth from the same planes.
                f32 farPlane = std::max(light.Range, kPointShadowNearPlane * 2.0f);
                Mat4 projection = GetCubeFaceProjection(kPointShadowNearPlane, farPlane);
                std::array<Mat4, 6> views = GetCubeFaceViews(light.Position);
                for (u32 face = 0; face < 6; ++face) {
                    m_Views.push_back({projection * views[face], m_PointFramebuffers[cube * 6 + face].get(), true,
                                       light.Position, farPlane});
                }
                frame.Lights[i].SpotAndShadow.z = static_cast<f32>(cube);
                continue;
            }

            if (shadowCount == kMaxShadows) {
                if (!m_LimitWarned) {
                    VKE_CORE_WARN("Renderer: more than {} lights cast shadows, the rest have none", kMaxShadows);
                    m_LimitWarned = true;
                }
                continue;
            }

            f32 directionLength = glm::length(light.Direction);
            Vec3 direction = directionLength > 0.0f ? light.Direction / directionLength : Vec3(0.0f, -1.0f, 0.0f);

            f32 texelSize = 0.0f;
            bool isSpot = light.Type == LightType::Spot;
            Mat4 viewProjection = isSpot ? FitSpot(light, direction, texelSize)
                                         : FitDirectional(direction, sceneBounds, texelSize);

            m_Views.push_back({viewProjection, m_Framebuffers[shadowCount].get(), isSpot, light.Position, light.Range});
            frame.Shadows[shadowCount].ViewProjection = viewProjection;
            frame.Shadows[shadowCount].Parameters = Vec4(texelSize, static_cast<f32>(shadowCount), 0.0f, 0.0f);
            frame.Lights[i].SpotAndShadow.z = static_cast<f32>(shadowCount);
            ++shadowCount;
        }
    }

    void ShadowPass::Record(RHI::RHICommandBuffer& cmd, const std::vector<DrawItem>& items) {
        RHI::ClearValue clear;
        for (const ShadowView& view : m_Views) {
            cmd.BeginRenderPass(*m_RenderPass, *view.Framebuffer, clear);
            cmd.BindPipeline(*m_Pipeline);

            const GpuMesh* boundMesh = nullptr;
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

            cmd.EndRenderPass();
        }
    }

} // namespace VKE::Renderer
