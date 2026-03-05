#pragma once

#include <cstdint>

namespace Renderer {

class Hdr {
public:
	virtual ~Hdr() = default;

	// In a fully abstracted renderer, texture handles would act as opaque pointers
	// For now we type-erase them as `void*` or keep them generic since Vulkan is our backend
	virtual void init(void* device, void* hdrResolveImageView, uint32_t width, uint32_t height) = 0;

	virtual void recordHdrCommandBuffer(uint32_t currentFrame, uint32_t imageIndex, float exposure) = 0;

	virtual void recreateHdr(void* hdrResolveImageView, uint32_t width, uint32_t height) = 0;

	virtual void cleanup() = 0;

	// Access to the backend-specific command buffers and target images for ImGui integration
	virtual void* getCommandBuffer(uint32_t currentFrame) const = 0;
	virtual void* getLdrImageView(uint32_t index) const = 0;
	virtual uint32_t getLdrImageViewCount() const = 0;
};

} // namespace Renderer
