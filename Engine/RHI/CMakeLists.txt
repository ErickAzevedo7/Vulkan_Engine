add_library(VKE_RHI STATIC)

find_package(glfw3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

target_sources(VKE_RHI PRIVATE
    private/vulkan/VulkanAllocator.cpp
    private/vulkan/VulkanDevice.cpp
    private/vulkan/VulkanSwapchain.cpp
    private/vulkan/VulkanShader.cpp
    private/vulkan/VulkanBuffer.cpp
    private/vulkan/VulkanRenderPass.cpp
    private/vulkan/VulkanFramebuffer.cpp
    private/vulkan/VulkanPipeline.cpp
    private/vulkan/VulkanCommandBuffer.cpp
    private/vulkan/VulkanTexture.cpp
    private/vulkan/VulkanSampler.cpp
    private/vulkan/VulkanDescriptor.cpp
)

target_include_directories(VKE_RHI
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_RHI
    PUBLIC  VKE_Core
    PRIVATE VKE_Platform
            Vulkan::Vulkan
            glfw
            GPUOpen::VulkanMemoryAllocator
)

set_target_properties(VKE_RHI PROPERTIES FOLDER "Engine")
