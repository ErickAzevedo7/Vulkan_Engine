#pragma once

#include <rhi/RHITexture.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanTexture : public RHITexture {
    public:
        VulkanTexture(VulkanDevice& device, const TextureDesc& desc, const void* initialData);
        ~VulkanTexture() override;

        u32 GetWidth() const override { return m_Width; }
        u32 GetHeight() const override { return m_Height; }
        Format GetFormat() const override { return m_Format; }
        u32 GetMipLevels() const override { return m_MipLevels; }
        void* GetView() const override { return m_View; }

        VkImage GetImage() const { return m_Image; }
        VkImageView GetImageView() const { return m_View; }

    private:
        void Upload(const void* data);

        VulkanDevice& m_Device;
        VkImage m_Image = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        VkImageView m_View = VK_NULL_HANDLE;
        u32 m_Width = 0;
        u32 m_Height = 0;
        u32 m_MipLevels = 1;
        Format m_Format = Format::Undefined;
    };

} // namespace VKE::RHI
