#include "ShadowPass.h"

#include <core/Log.h>
#include <platform/FileSystem.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace VKE::Renderer {

    namespace {
        constexpr u32 kShadowMapSize = 2048;
        constexpr RHI::Format kShadowMapFormat = RHI::Format::D32_SFLOAT;
        constexpr f32 kSpotNearPlane = 0.05f;
        // Keeps casters right at the edge of the scene bounds from being clipped.
        constexpr f32 kDirectionalPadding = 0.5f;

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
            for (auto& framebuffer : m_Framebuffers) {
                cmd.BeginRenderPass(*m_RenderPass, *framebuffer, clear);
                cmd.EndRenderPass();
            }
        });
    }

    void ShadowPass::Prepare(const std::vector<LightDesc>& lights, u32 lightCount, const AABB& sceneBounds,
                             FrameUniforms& frame) {
        m_ShadowCount = 0;
        for (u32 i = 0; i < lightCount; ++i) {
            const LightDesc& light = lights[i];
            if (!light.CastsShadow || light.Type == LightType::Point) continue;

            if (m_ShadowCount == kMaxShadows) {
                if (!m_LimitWarned) {
                    VKE_CORE_WARN("Renderer: more than {} lights cast shadows, the rest have none", kMaxShadows);
                    m_LimitWarned = true;
                }
                break;
            }

            f32 directionLength = glm::length(light.Direction);
            Vec3 direction = directionLength > 0.0f ? light.Direction / directionLength : Vec3(0.0f, -1.0f, 0.0f);

            f32 texelSize = 0.0f;
            Mat4 viewProjection = light.Type == LightType::Directional
                                      ? FitDirectional(direction, sceneBounds, texelSize)
                                      : FitSpot(light, direction, texelSize);

            m_ViewProjections[m_ShadowCount] = viewProjection;
            frame.Shadows[m_ShadowCount].ViewProjection = viewProjection;
            frame.Shadows[m_ShadowCount].Parameters = Vec4(texelSize, static_cast<f32>(m_ShadowCount), 0.0f, 0.0f);
            frame.Lights[i].SpotAndShadow.z = static_cast<f32>(m_ShadowCount);
            ++m_ShadowCount;
        }
    }

    void ShadowPass::Record(RHI::RHICommandBuffer& cmd, const std::vector<DrawItem>& items) {
        RHI::ClearValue clear;
        for (u32 shadow = 0; shadow < m_ShadowCount; ++shadow) {
            cmd.BeginRenderPass(*m_RenderPass, *m_Framebuffers[shadow], clear);
            cmd.BindPipeline(*m_Pipeline);

            const GpuMesh* boundMesh = nullptr;
            for (const DrawItem& item : items) {
                if (item.Mesh != boundMesh) {
                    cmd.BindVertexBuffer(*item.Mesh->VertexBuffer);
                    cmd.BindIndexBuffer(*item.Mesh->IndexBuffer, RHI::IndexType::U32);
                    boundMesh = item.Mesh;
                }

                ShadowPushConstants object;
                object.ModelViewProjection = m_ViewProjections[shadow] * item.Instance->Transform;
                cmd.PushConstants(RHI::ShaderStageFlags::Vertex, 0, sizeof(object), &object);
                cmd.DrawIndexed(item.Submesh->IndexCount, 1, item.Submesh->FirstIndex, 0, 0);
            }

            cmd.EndRenderPass();
        }
    }

} // namespace VKE::Renderer
