#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanUtils.h"

#include <core/Log.h>
#include <platform/Window.h>

#include <GLFW/glfw3.h>

#include <cstring>
#include <map>
#include <set>

namespace VKE::RHI {

    namespace {
        // Must match the version passed to the memory allocator.
        constexpr u32 kVulkanApiVersion = VK_API_VERSION_1_2;

        const std::vector<const char*> kValidationLayers = {"VK_LAYER_KHRONOS_validation"};

        const std::vector<const char*> kDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef VKE_DEBUG
        constexpr bool kEnableValidationLayers = true;
#else
        constexpr bool kEnableValidationLayers = false;
#endif

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                      void* /*userData*/) {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                VKE_CORE_ERROR("[Vulkan] {}", callbackData->pMessage);
            } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                VKE_CORE_WARN("[Vulkan] {}", callbackData->pMessage);
            } else {
                VKE_CORE_TRACE("[Vulkan] {}", callbackData->pMessage);
            }
            return VK_FALSE;
        }

        void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = DebugCallback;
        }

        VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
                                               const VkAllocationCallbacks* allocator,
                                               VkDebugUtilsMessengerEXT* messenger) {
            auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            return func ? func(instance, createInfo, allocator, messenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                            const VkAllocationCallbacks* allocator) {
            auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (func) func(instance, messenger, allocator);
        }
    } // namespace

    VulkanDevice::VulkanDevice(Platform::IWindow& window) {
        CreateInstance();
        SetupDebugMessenger();
        CreateSurface(window);
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateCommandPool();
        QueryDeviceInfo();
        CreateDescriptorResources();

        m_Allocator = MakeIunique<VulkanAllocator>(m_Instance, m_PhysicalDevice, m_Device, kVulkanApiVersion);

        VKE_CORE_INFO("VulkanDevice initialized");
    }

    VulkanDevice::~VulkanDevice() {
        m_Allocator.reset();
        m_DescriptorAllocator.reset();
        if (m_EmptySetLayout) vkDestroyDescriptorSetLayout(m_Device, m_EmptySetLayout, nullptr);

        if (m_CommandPool) vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        if (m_Device) vkDestroyDevice(m_Device, nullptr);
        if (kEnableValidationLayers && m_DebugMessenger) DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        if (m_Surface) vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        if (m_Instance) vkDestroyInstance(m_Instance, nullptr);

        VKE_CORE_INFO("VulkanDevice shutdown");
    }

    void VulkanDevice::WaitIdle() {
        vkDeviceWaitIdle(m_Device);
    }

    void VulkanDevice::SubmitAndWait(VkCommandBuffer commandBuffer) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &fence));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence));
        VK_CHECK(vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(m_Device, fence, nullptr);
    }

    void VulkanDevice::ExecuteImmediate(const std::function<void(RHICommandBuffer&)>& record) {
        VulkanCommandBuffer commandBuffer(*this);
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        record(commandBuffer);
        commandBuffer.End();
        SubmitAndWait(commandBuffer.GetHandle());
    }

    void VulkanDevice::SetDebugName(VkObjectType type, u64 handle, const char* name) const {
        if (!kEnableValidationLayers || !name) return;

        auto setObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkSetDebugUtilsObjectNameEXT"));
        if (!setObjectName) return;

        VkDebugUtilsObjectNameInfoEXT info{};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        setObjectName(m_Device, &info);
    }

    bool VulkanDevice::CheckValidationLayerSupport() const {
        u32 layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : kValidationLayers) {
            bool found = false;
            for (const auto& layerProperties : availableLayers) {
                if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    std::vector<const char*> VulkanDevice::GetRequiredInstanceExtensions() const {
        u32 glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (kEnableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    void VulkanDevice::CreateInstance() {
        VKE_CORE_ASSERT(!kEnableValidationLayers || CheckValidationLayerSupport(),
                         "Validation layers requested but not available");

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Engine — Sandbox";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Vulkan Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = kVulkanApiVersion;

        auto extensions = GetRequiredInstanceExtensions();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (kEnableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<u32>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();

            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));
    }

    void VulkanDevice::SetupDebugMessenger() {
        if (!kEnableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        PopulateDebugMessengerCreateInfo(createInfo);

        VK_CHECK(CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger));
    }

    void VulkanDevice::CreateSurface(Platform::IWindow& window) {
        // GetNativeHandle() is documented (platform/Window.h) as a GLFWwindow*
        // meant to be interpreted only by RHI backends — this is that backend.
        auto* glfwWindow = static_cast<GLFWwindow*>(window.GetNativeHandle());
        VK_CHECK(glfwCreateWindowSurface(m_Instance, glfwWindow, nullptr, &m_Surface));
    }

    bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) const {
        u32 extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    VulkanDevice::QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device) const {
        QueueFamilyIndices indices;

        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (u32 i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.Graphics = i;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            if (presentSupport) {
                indices.Present = i;
            }

            if (indices.IsComplete()) break;
        }
        return indices;
    }

    int VulkanDevice::RateDeviceSuitability(VkPhysicalDevice device) const {
        if (!FindQueueFamilies(device).IsComplete()) return 0;
        if (!CheckDeviceExtensionSupport(device)) return 0;

        u32 formatCount = 0, presentModeCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
        if (formatCount == 0 || presentModeCount == 0) return 0;

        // Point light shadows live in a cube array.
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);
        if (!features.imageCubeArray) return 0;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        int score = 0;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        score += static_cast<int>(properties.limits.maxImageDimension2D);
        return score;
    }

    void VulkanDevice::PickPhysicalDevice() {
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        VKE_CORE_ASSERT(deviceCount > 0, "No Vulkan-capable GPU found");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        std::multimap<int, VkPhysicalDevice> candidates;
        for (auto device : devices) {
            candidates.insert({RateDeviceSuitability(device), device});
        }

        VKE_CORE_ASSERT(candidates.rbegin()->first > 0, "No suitable GPU found");
        m_PhysicalDevice = candidates.rbegin()->second;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
        VKE_CORE_INFO("Selected GPU: {}", properties.deviceName);
    }

    void VulkanDevice::CreateLogicalDevice() {
        QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
        m_GraphicsQueueFamily = indices.Graphics.value();
        m_PresentQueueFamily = indices.Present.value();

        std::set<u32> uniqueQueueFamilies = {m_GraphicsQueueFamily, m_PresentQueueFamily};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        for (u32 family : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = family;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Cube arrays are required when the GPU is picked; the rest are enabled when available.
        VkPhysicalDeviceFeatures availableFeatures{};
        vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &availableFeatures);

        m_EnabledFeatures = {};
        m_EnabledFeatures.samplerAnisotropy = availableFeatures.samplerAnisotropy;
        m_EnabledFeatures.fillModeNonSolid = availableFeatures.fillModeNonSolid;
        m_EnabledFeatures.imageCubeArray = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &m_EnabledFeatures;
        createInfo.enabledExtensionCount = static_cast<u32>(kDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
        if (kEnableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<u32>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device));

        vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);
    }

    void VulkanDevice::CreateCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;

        VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool));
    }

    void VulkanDevice::CreateDescriptorResources() {
        m_DescriptorAllocator = MakeIunique<VulkanDescriptorAllocator>(m_Device);

        VkDescriptorSetLayoutCreateInfo emptyLayoutInfo{};
        emptyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        VK_CHECK(vkCreateDescriptorSetLayout(m_Device, &emptyLayoutInfo, nullptr, &m_EmptySetLayout));
    }

    void VulkanDevice::QueryDeviceInfo() {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

        m_Limits.MinUniformBufferOffsetAlignment = static_cast<u32>(properties.limits.minUniformBufferOffsetAlignment);
        m_Limits.MaxPushConstantSize = properties.limits.maxPushConstantsSize;
        m_Limits.MaxTextureSize = properties.limits.maxImageDimension2D;
        m_Limits.MaxSamplerAnisotropy = m_EnabledFeatures.samplerAnisotropy ? properties.limits.maxSamplerAnisotropy : 1.0f;

        const VkFormat depthCandidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        for (VkFormat candidate : depthCandidates) {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, candidate, &formatProperties);
            if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                m_DepthFormat = FromVkFormat(candidate);
                break;
            }
        }
        VKE_CORE_ASSERT(m_DepthFormat != Format::Undefined, "No supported depth format");
    }

    Iunique<RHIDevice> RHIDevice::Create(Platform::IWindow& window) {
        return MakeIunique<VulkanDevice>(window);
    }

} // namespace VKE::RHI
