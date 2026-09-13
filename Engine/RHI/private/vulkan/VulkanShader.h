#pragma once

#include <rhi/RHIShader.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanShader : public RHIShader {
    public:
        VulkanShader(VulkanDevice& device, const std::string& spirvPath, ShaderStageKind stage);
        ~VulkanShader() override;

        ShaderStageKind GetStage() const override { return m_Stage; }
        VkShaderModule GetHandle() const { return m_Module; }

    private:
        VulkanDevice& m_Device;
        VkShaderModule m_Module = VK_NULL_HANDLE;
        ShaderStageKind m_Stage;
    };

} // namespace VKE::RHI
