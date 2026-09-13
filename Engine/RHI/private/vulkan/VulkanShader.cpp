#include "VulkanShader.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <platform/FileSystem.h>

namespace VKE::RHI {

    VulkanShader::VulkanShader(VulkanDevice& device, const std::string& spirvPath, ShaderStageKind stage)
        : m_Device(device), m_Stage(stage) {
        // Pre-compiled SPIR-V only — no runtime GLSL compilation, same as the
        // legacy engine's VulkanCore::createShaderModule. See
        // sandbox/CMakeLists.txt for the glslc build step that produces these.
        std::vector<Byte> code = Platform::FileSystem::ReadFileBytes(spirvPath);
        VKE_CORE_ASSERT(!code.empty(), "Failed to read shader file");

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const u32*>(code.data());

        VK_CHECK(vkCreateShaderModule(m_Device.GetDevice(), &createInfo, nullptr, &m_Module));
    }

    VulkanShader::~VulkanShader() {
        if (m_Module) {
            vkDestroyShaderModule(m_Device.GetDevice(), m_Module, nullptr);
        }
    }

    Iunique<RHIShader> RHIShader::CreateFromFile(RHIDevice& device, const std::string& spirvPath, ShaderStageKind stage) {
        return MakeIunique<VulkanShader>(static_cast<VulkanDevice&>(device), spirvPath, stage);
    }

} // namespace VKE::RHI
