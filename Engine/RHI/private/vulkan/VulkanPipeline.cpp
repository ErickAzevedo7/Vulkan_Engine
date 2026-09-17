#include "VulkanPipeline.h"
#include "VulkanBindGroup.h"
#include "VulkanDevice.h"
#include "VulkanRenderPass.h"
#include "VulkanShader.h"
#include "VulkanUtils.h"

namespace VKE::RHI {

    VulkanPipeline::VulkanPipeline(VulkanDevice& device, VulkanRenderPass& renderPass, const PipelineDesc& desc)
        : m_Device(device) {
        // Shader modules are only needed while the pipeline is built.
        VulkanShader vertShader(device, desc.VertexShaderPath, ShaderStageKind::Vertex);
        VulkanShader fragShader(device, desc.FragmentShaderPath, ShaderStageKind::Fragment);

        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertShader.GetHandle();
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragShader.GetHandle();
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vertStageInfo, fragStageInfo};

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = desc.VertexStride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributes;
        attributes.reserve(desc.VertexAttributes.size());
        for (const auto& attr : desc.VertexAttributes) {
            VkVertexInputAttributeDescription vkAttr{};
            vkAttr.location = attr.Location;
            vkAttr.binding = 0;
            vkAttr.format = ToVkFormat(attr.Format);
            vkAttr.offset = attr.Offset;
            attributes.push_back(vkAttr);
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &binding;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = ToVkTopology(desc.Topology);

        // Viewport and scissor are dynamic so the pipeline survives resizes.
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        PolygonMode polygonMode = desc.PolygonMode;
        if (polygonMode != PolygonMode::Fill && !m_Device.GetEnabledFeatures().fillModeNonSolid) {
            VKE_CORE_WARN("Pipeline: line/point polygon mode is not supported by this GPU, using Fill");
            polygonMode = PolygonMode::Fill;
        }

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = ToVkPolygonMode(polygonMode);
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = ToVkCullMode(desc.CullMode);
        rasterizer.frontFace = ToVkFrontFace(desc.FrontFace);

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.DepthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.DepthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = ToVkCompareOp(desc.DepthCompareOp);

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkPushConstantRange> pushConstantRanges;
        pushConstantRanges.reserve(desc.PushConstantRanges.size());
        for (const auto& range : desc.PushConstantRanges) {
            VKE_CORE_ASSERT(range.Offset + range.Size <= m_Device.GetLimits().MaxPushConstantSize,
                            "Push constant range exceeds the device limit");
            VkPushConstantRange vkRange{};
            vkRange.stageFlags = ToVkShaderStages(range.Stages);
            vkRange.offset = range.Offset;
            vkRange.size = range.Size;
            pushConstantRanges.push_back(vkRange);
        }

        std::vector<VkDescriptorSetLayout> setLayouts;
        setLayouts.reserve(desc.BindGroupLayouts.size());
        for (RHIBindGroupLayout* layout : desc.BindGroupLayouts) {
            setLayouts.push_back(layout ? static_cast<VulkanBindGroupLayout*>(layout)->GetHandle()
                                        : m_Device.GetEmptySetLayout());
        }

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<u32>(setLayouts.size());
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.pushConstantRangeCount = static_cast<u32>(pushConstantRanges.size());
        layoutInfo.pPushConstantRanges = pushConstantRanges.data();
        VK_CHECK(vkCreatePipelineLayout(m_Device.GetDevice(), &layoutInfo, nullptr, &m_Layout));

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_Layout;
        pipelineInfo.renderPass = renderPass.GetHandle();
        pipelineInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(m_Device.GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));
    }

    VulkanPipeline::~VulkanPipeline() {
        if (m_Pipeline) vkDestroyPipeline(m_Device.GetDevice(), m_Pipeline, nullptr);
        if (m_Layout) vkDestroyPipelineLayout(m_Device.GetDevice(), m_Layout, nullptr);
    }

    Iunique<RHIPipeline> RHIPipeline::Create(RHIDevice& device, RHIRenderPass& renderPass, const PipelineDesc& desc) {
        return MakeIunique<VulkanPipeline>(static_cast<VulkanDevice&>(device), static_cast<VulkanRenderPass&>(renderPass),
                                            desc);
    }

} // namespace VKE::RHI
