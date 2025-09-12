#pragma once

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <stb_image.h>

#include <tiny_obj_loader.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <cstring>
#include <limits> 
#include <algorithm> 
#include <fstream>
#include <array>
#include <unordered_map>

extern const int MAX_FRAMES_IN_FLIGHT;

extern const uint32_t WIDTH;
extern const uint32_t HEIGHT;

extern const std::string MODEL_PATH;
extern const std::string TEXTURE_PATH;

extern const std::vector<const char*> validationLayers;

extern const std::vector<const char*> deviceExtensions;


extern const bool enableValidationLayers;


struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;


    bool isComplete();
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

    bool operator==(const Vertex& other) const;
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const;
    };
}

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

extern std::vector<Vertex> vertices;
extern std::vector<uint32_t> indices;

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

class VulkanCore {
public:
    void initWindow();

    void initVulkan();

    void cleanup();

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void updateUniformBuffer(uint32_t currentImage);

    void recreateSwapChain();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkFormat findDepthFormat();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	VkPipelineLayout getPipelineLayout();

    VkPipeline getPipeline();

    VkInstance getInstance();

	VkBuffer getVertexBuffer();

	VkBuffer getIndexBuffer();

    VkPhysicalDevice getPhysicalDevice();

    VkDevice getDevice();

    uint32_t getGraphicsQueueFamily();

    VkQueue getGraphicsQueue();

    VkQueue getPresentQueue();

    GLFWwindow* getWindow();

    VkFormat getSwapChainImageFormat();

    VkSampleCountFlagBits getmsaaSamples();

    bool getSwapChainRecreated();

    bool getFramebufferResized();

    void setFramebufferResized(bool value);

    void setSwapChainRecreated(bool value);

    std::vector<VkImageView> getSwapChainImageViews();

	VkImageView getDepthImageView();

	VkImageView getColorResolveImageView();

    VkExtent2D getSwapChainExtent();

    std::vector<VkSemaphore> getImageAvailableSemaphores();

    std::vector<VkSemaphore> getRenderFinishedSemaphores();

    std::vector<VkFence> getInFlightFences();

    uint32_t getCurrentFrame();

    void setCurrentFrame(uint32_t frame);

    VkSwapchainKHR getSwapChain();

    std::vector<VkCommandBuffer> getCommandBuffers();

	std::vector<VkDescriptorSet> getDescriptorSets();

	VkImageView getTextureImageView();

	VkSampler getTextureSampler();

	std::vector<void*> getUniformBuffersMapped();

private:
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame;
    bool framebufferResized;
    bool swapChainRecreated;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    uint32_t mipLevels;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkSampleCountFlagBits msaaSamples;
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    static std::vector<char> readFile(const std::string& filename);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void createColorResources();

    VkSampleCountFlagBits getMaxUsableSampleCount();

    void loadModel();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    bool hasStencilComponent(VkFormat format);

    void createDepthResources();

    void createTextureSampler();

    void createTextureImageView();

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void createTextureImage();

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    void createDescriptorSets();

    void createDescriptorPool();

    void createUniformBuffers();

    void createDescriptorSetLayout();

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void createVertexBuffer();

    void createIndexBuffer();

    void createSyncObjects();

    void createCommandBuffers();

    void createCommandPool();

    void createFramebuffers();

    void createRenderPass();

    void createGraphicsPipeline();

    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createImageViews();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void setupDebugMessenger();

    void createSwapChain();

    void createSurface();

    void createLogicalDevice();

    void pickPhysicalDevice();

    int rateDeviceSuitability(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    void createInstance();

    bool checkValidationLayerSupport();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    std::vector<const char*> getRequiredExtensions();

    void cleanupSwapChain();
};
