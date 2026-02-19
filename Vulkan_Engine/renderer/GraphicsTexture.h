#pragma once

#include <cstdint>

#include "RenderTypes.h"


namespace Renderer {

/**
 * @brief Abstract interface for managing texture resources.
 * Implementations handle API-specific details (Vulkan, etc.).
 */
class GraphicsTexture {
public:
	virtual ~GraphicsTexture() = default;

	/**
	 * @brief Create a new texture resource.
	 * @param desc Description of the texture to create.
	 * @param initialData Optional pointer to initial pixel data.
	 *                    If provided, size is assumed to match desc dimensions/format.
	 * @return Handle to the created texture.
	 */
	virtual TextureHandle createTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;

	/**
	 * @brief Create a new sampler resource.
	 * @param desc Description of the sampler parameters.
	 * @return Handle to the created sampler.
	 */
	virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;

	/**
	 * @brief Destroy a texture resource.
	 * @param handle Handle of the texture to destroy.
	 */
	virtual void destroyTexture(TextureHandle handle) = 0;

	/**
	 * @brief Destroy a sampler resource.
	 * @param handle Handle of the sampler to destroy.
	 */
	virtual void destroySampler(SamplerHandle handle) = 0;

	/**
	 * @brief Create a thumbnail texture from a source texture.
	 * @param source Source texture handle.
	 * @param width Target width.
	 * @param height Target height.
	 * @return Handle to the created thumbnail texture.
	 */
	virtual TextureHandle createThumbnail(TextureHandle source, uint32_t width, uint32_t height) = 0;
};

} // namespace Renderer
