#pragma once

#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float4.hpp>

namespace Renderer {

class ShadowMap {
public:
	virtual ~ShadowMap() = default;

	virtual void init(void* device, uint32_t width, uint32_t height) = 0;
	virtual void cleanup() = 0;

	virtual void* getRenderPass() const = 0;
	virtual void* getFramebuffer() const = 0;
	virtual void* getDepthImageView() const = 0;
	virtual void* getDepth2DView() const = 0;
	virtual void* getDepthCubeImageView() const = 0;
	virtual void* getDepthSampler() const = 0;

	virtual uint32_t getWidth() const = 0;
	virtual uint32_t getHeight() const = 0;

	// Math logic for generating shadow cascade and projection matrices
	virtual void calculateShadowMatrices(const glm::vec4& positionType,
										 const glm::vec4& direction,
										 float far_plane,
										 float outerCutOff,
										 glm::mat4 outMatrices[6],
										 glm::vec4& outLightPosFarPlane) = 0;
};

} // namespace Renderer
