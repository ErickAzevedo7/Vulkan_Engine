#pragma once

#include <cstdint>

namespace Renderer {

/**
 * @brief Abstract interface for accessing the graphics device and related core resources.
 * Implementations provide API-specific handles (Vulkan, DirectX, etc.).
 */
class GraphicsDevice {
public:
	virtual ~GraphicsDevice() = default;

	// --- Device Handles (returned as void* for API-agnosticism) ---

	/** @brief Returns the native logical device handle (e.g. VkDevice). */
	virtual void* getNativeDevice() const = 0;

	/** @brief Returns the native physical device handle (e.g. VkPhysicalDevice). */
	virtual void* getNativePhysicalDevice() const = 0;

	/** @brief Returns the native graphics queue handle (e.g. VkQueue). */
	virtual void* getNativeGraphicsQueue() const = 0;

	/** @brief Returns the native present queue handle (e.g. VkQueue). */
	virtual void* getNativePresentQueue() const = 0;

	/** @brief Returns the native command pool handle (e.g. VkCommandPool). */
	virtual void* getNativeCommandPool() const = 0;

	// --- Typed Accessors ---

	/** @brief Returns the graphics queue family index. */
	virtual uint32_t getGraphicsQueueFamily() const = 0;

	/** @brief Returns the minimum UBO alignment for dynamic offsets. */
	virtual uint64_t getDynamicAlignment() const = 0;

	/** @brief Returns the max usable MSAA sample count. */
	virtual uint32_t getMsaaSamples() const = 0;

	// --- Operations ---

	/** @brief Blocks until the device is idle. */
	virtual void waitIdle() = 0;
};

} // namespace Renderer
