#include "VulkanBindGroup.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanSampler.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"

#include <algorithm>

namespace VKE::RHI {

    VulkanBindGroupLayout::VulkanBindGroupLayout(VulkanDevice& device, const BindGroupLayoutDesc& desc)
        : m_Device(device), m_Slots(desc.Slots) {
        std::sort(m_Slots.begin(), m_Slots.end(),
                  [](const BindingSlot& a, const BindingSlot& b) { return a.Binding < b.Binding; });

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(m_Slots.size());
        for (const auto& slot : m_Slots) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = slot.Binding;
            binding.descriptorType = ToVkDescriptorType(slot.Type);
            binding.descriptorCount = 1;
            binding.stageFlags = ToVkShaderStages(slot.Stages);
            bindings.push_back(binding);

            if (slot.Type == BindingType::CombinedImageSampler) {
                ++m_ImageSlotCount;
            } else {
                ++m_BufferSlotCount;
            }
            if (slot.Type == BindingType::UniformBufferDynamic) {
                ++m_DynamicOffsetCount;
            }
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<u32>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_Device.GetDevice(), &layoutInfo, nullptr, &m_Layout));
        m_Device.SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<u64>(m_Layout), desc.DebugName);
    }

    VulkanBindGroupLayout::~VulkanBindGroupLayout() {
        if (m_Layout) vkDestroyDescriptorSetLayout(m_Device.GetDevice(), m_Layout, nullptr);
    }

    VulkanBindGroup::VulkanBindGroup(VulkanDevice& device, const BindGroupDesc& desc) : m_Device(device) {
        VKE_CORE_ASSERT(desc.Layout, "BindGroupDesc requires a layout");
        const auto& layout = static_cast<VulkanBindGroupLayout&>(*desc.Layout);
        const auto& slots = layout.GetSlots();
        VKE_CORE_ASSERT(desc.Resources.size() == slots.size(), "Bind group needs one resource per layout slot");

        m_DynamicOffsetCount = layout.GetDynamicOffsetCount();
        m_Allocation = m_Device.GetDescriptorAllocator().Allocate(layout.GetHandle());
        m_Device.SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<u64>(m_Allocation.Set), desc.DebugName);

        // Sized from the layout and never grown, so the pointers stored in the writes stay valid.
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkWriteDescriptorSet> writes;
        bufferInfos.reserve(layout.GetBufferSlotCount());
        imageInfos.reserve(layout.GetImageSlotCount());
        writes.reserve(slots.size());

        for (const auto& resource : desc.Resources) {
            // The slot's type decides how the resource is written.
            auto slot = std::find_if(slots.begin(), slots.end(),
                                     [&](const BindingSlot& s) { return s.Binding == resource.Binding; });
            VKE_CORE_ASSERT(slot != slots.end(), "Bound resource has no matching layout slot");
            if (slot == slots.end()) continue;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_Allocation.Set;
            write.dstBinding = resource.Binding;
            write.descriptorCount = 1;
            write.descriptorType = ToVkDescriptorType(slot->Type);

            if (slot->Type == BindingType::CombinedImageSampler) {
                VKE_CORE_ASSERT(resource.Texture && resource.Sampler, "Image sampler slot needs a texture and a sampler");
                VKE_CORE_ASSERT(imageInfos.size() < imageInfos.capacity(), "More image resources than layout slots");
                if (imageInfos.size() == imageInfos.capacity()) continue;

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageView = static_cast<VulkanTexture*>(resource.Texture)->GetImageView();
                imageInfo.sampler = static_cast<VulkanSampler*>(resource.Sampler)->GetHandle();
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfos.push_back(imageInfo);
                write.pImageInfo = &imageInfos.back();
            } else {
                VKE_CORE_ASSERT(resource.Buffer, "Uniform buffer slot needs a buffer");
                VKE_CORE_ASSERT(bufferInfos.size() < bufferInfos.capacity(), "More buffer resources than layout slots");
                if (bufferInfos.size() == bufferInfos.capacity()) continue;
                VKE_CORE_ASSERT(slot->Type != BindingType::UniformBufferDynamic || resource.Size > 0,
                                "Dynamic uniform buffer slot needs an explicit size");

                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = static_cast<VulkanBuffer*>(resource.Buffer)->GetHandle();
                bufferInfo.offset = resource.Offset;
                bufferInfo.range = resource.Size > 0 ? resource.Size : VK_WHOLE_SIZE;
                bufferInfos.push_back(bufferInfo);
                write.pBufferInfo = &bufferInfos.back();
            }

            writes.push_back(write);
        }

        vkUpdateDescriptorSets(m_Device.GetDevice(), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VulkanBindGroup::~VulkanBindGroup() {
        m_Device.GetDescriptorAllocator().Free(m_Allocation);
    }

    Iunique<RHIBindGroupLayout> RHIBindGroupLayout::Create(RHIDevice& device, const BindGroupLayoutDesc& desc) {
        return MakeIunique<VulkanBindGroupLayout>(static_cast<VulkanDevice&>(device), desc);
    }

    Iunique<RHIBindGroup> RHIBindGroup::Create(RHIDevice& device, const BindGroupDesc& desc) {
        return MakeIunique<VulkanBindGroup>(static_cast<VulkanDevice&>(device), desc);
    }

} // namespace VKE::RHI
