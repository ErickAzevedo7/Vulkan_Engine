#pragma once

// External libraries - required for interface
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <cstdint>

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

// C++ standard library (only what's needed in the interface)
#include <optional>
#include <string>
#include <vector>

// GLM (needed for UniformBufferObject in interface)
#include <glm/glm.hpp>

// Forward declarations
struct Vertex;

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

struct UniformBufferObject {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 normal;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::vec3 viewPos;
};

extern std::vector<Vertex> vertices;
extern std::vector<uint32_t> indices;

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
									  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
									  const VkAllocationCallbacks* pAllocator,
									  VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
								   VkDebugUtilsMessengerEXT debugMessenger,
								   const VkAllocationCallbacks* pAllocator);

class VulkanCore {
public:
	void initWindow();

	void initVulkan();

	void cleanup();

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	void recreateSwapChain();

	VkFormat findDepthFormat();

	VkPipelineLayout getPipelineLayout();

	VkPipeline getPipeline();

	VkInstance getInstance();

	static VkPhysicalDevice getPhysicalDevice();

	static VkDevice getDevice();

	uint32_t getGraphicsQueueFamily();

	static VkQueue getGraphicsQueue();

	static VkQueue getPresentQueue();

	GLFWwindow* getWindow();

	VkFormat getSwapChainImageFormat();

	static VkSampleCountFlagBits getmsaaSamples();

	bool getSwapChainRecreated();

	bool getFramebufferResized();

	void setFramebufferResized(bool value);

	static VkDescriptorSetLayout getDescriptorSetLayout();

	void setSwapChainRecreated(bool value);

	static std::vector<VkImageView> getSwapChainImageViews();

	static VkExtent2D getSwapChainExtent();

	std::vector<VkSemaphore> getImageAvailableSemaphores();

	std::vector<VkSemaphore> getRenderFinishedSemaphores();

	std::vector<VkFence> getInFlightFences();

	static uint32_t getCurrentFrame();

	void setCurrentFrame(uint32_t frame);

	VkSwapchainKHR getSwapChain();

	std::vector<VkCommandBuffer> getCommandBuffers();

	std::vector<VkDescriptorSet> getDescriptorSets();

	VkImageView getTextureImageView();

	VkSampler getTextureSampler();

	std::vector<void*> getUniformBuffersMapped();

	static VkCommandPool getCommandPool();

	static std::vector<VkBuffer> getUniformBuffers();

	static VkDeviceSize getDynamicAlignment();

private:
	GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	static VkPhysicalDevice physicalDevice;
	static VkDevice device;
	static VkQueue graphicsQueue;
	static VkQueue presentQueue;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	static VkExtent2D swapChainExtent;
	static std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	static VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	static VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	static uint32_t currentFrame;
	bool framebufferResized;
	bool swapChainRecreated;
	static std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	uint32_t mipLevels;
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	static VkSampleCountFlagBits msaaSamples;

	static VkDeviceSize dynamicAlignment;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														VkDebugUtilsMessageTypeFlagsEXT messageType,
														const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
														void* pUserData);

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	VkSampleCountFlagBits getMaxUsableSampleCount();

	void loadModel();

	VkFormat
	findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	void createUniformBuffers();

	void createTextureSampler();

	void createSyncObjects();

	void createCommandBuffers();

	void createCommandPool();

	void createRenderPass();

	void createDescriptorSetLayout();

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
