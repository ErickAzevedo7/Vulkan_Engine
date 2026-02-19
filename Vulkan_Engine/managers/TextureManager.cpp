#include "TextureManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "renderer/GraphicsTexture.h"
#include "renderer/RenderTypes.h"


TextureManager::TextureManager() {
}

TextureManager::~TextureManager() {
	if (graphicsTexture) {
		for (auto& pair : textures) {
			graphicsTexture->destroySampler(pair.second.sampler);
			graphicsTexture->destroyTexture(pair.second.handle);
		}
		for (auto& pair : thumbnails) {
			graphicsTexture->destroySampler(pair.second.sampler);
			graphicsTexture->destroyTexture(pair.second.handle);
		}
	}
	textures.clear();
	thumbnails.clear();
}

void TextureManager::setGraphicsTexture(GraphicsTexture* graphicsTexture) {
	this->graphicsTexture = graphicsTexture;
}

void TextureManager::loadDefaults() {
	if (!graphicsTexture) {
		std::cerr << "TextureManager: GraphicsTexture not set!" << std::endl;
		return;
	}

	// load icons textures - UI icons should NOT be flipped
	Texture* texture = loadTexture("ui/icons/defaultFile.png", false);
	createTextureSampler(texture);
	textures["defaultFile"] = *texture;
	delete texture; // Map stores copy

	texture = loadTexture("ui/icons/folder.png", false);
	createTextureSampler(texture);
	textures["folder"] = *texture;
	delete texture;

	// load default object texture - 3D textures SHOULD be flipped
	texture = loadTexture(kDefaultTextureKey, true);
	createTextureSampler(texture);
	textures[kDefaultTextureKey] = *texture;
	delete texture;
}

void TextureManager::loadAllFromAssets(const std::string& assetsRoot) {
	namespace fs = std::filesystem;

	fs::path root(assetsRoot);
	if (!fs::exists(root) || !fs::is_directory(root)) {
		return;
	}

	for (auto const& entry : fs::recursive_directory_iterator(root)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		fs::path p = entry.path();
		std::string ext = p.extension().string();
		for (char& c : ext) {
			c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		}

		// supported texture extensions
		if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga" && ext != ".bmp" && ext != ".hdr") {
			continue;
		}

		const std::string fullPath = p.string();
		const std::string normFullPath = std::filesystem::path(fullPath).generic_string();
		if (textures.find(normFullPath) != textures.end()) {
			continue;
		}

		// Check if it's a UI icon (heuristic: path contains "ui/icons" or similar?)
		// For now, assume everything in assets/ is a 3D texture, EXCEPT if user organizes otherwise.
		// Detailed check: if path contains "ui/icons", don't flip.
		bool flip = true;
		if (normFullPath.find("ui/icons/") != std::string::npos) {
			flip = false;
		}

		Texture* texture = loadTexture(fullPath, flip);
		createTextureSampler(texture);
		textures[normFullPath] = *texture;

		// Also create a small thumbnail texture for UI from this source
		createThumbnail(normFullPath, *texture);
		delete texture;
	}
}

const ThumbnailTexture* TextureManager::getThumbnail(const std::string& key) {
	std::string k;

	k = std::filesystem::path(key).generic_string();

	auto it = thumbnails.find(k);
	if (it != thumbnails.end()) {
		return &it->second;
	}
	return nullptr;
}

ThumbnailTexture& TextureManager::createThumbnail(const std::string& key, const Texture& sourceTexture) {
	// Fixed thumbnail size (square) - adjust as needed
	const uint32_t thumbSize = 96;
	ThumbnailTexture thumb;
	thumb.width = thumbSize;
	thumb.height = thumbSize;

	// Create thumbnail via GraphicsTexture (blit)
	thumb.handle = graphicsTexture->createThumbnail(sourceTexture.handle, thumbSize, thumbSize);

	// Small, no-mipmap sampler tuned for UI
	createThumbnailSampler(&thumb);

	std::string k;
	k = std::filesystem::path(key).generic_string();

	thumbnails[k] = thumb;

	return thumbnails[k];
}

Texture* TextureManager::loadTexture(const std::string& path, bool flipV) {
	int texWidth, texHeight, texChannels;

	stbi_set_flip_vertically_on_load(flipV);
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		std::cerr << "TextureManager::loadTexture: failed to load texture image from scan! path=" << path << std::endl;
		// Return a default/error texture? Or throw?
		// For now, throw to match previous behavior
		throw std::runtime_error("failed to load texture image!");
	}

	Texture* texture = new Texture();
	texture->width = static_cast<uint32_t>(texWidth);
	texture->height = static_cast<uint32_t>(texHeight);
	texture->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	TextureDesc desc{};
	desc.width = texture->width;
	desc.height = texture->height;
	desc.mipLevels = texture->mipLevels;
	desc.format = TextureFormat::R8G8B8A8_SRGB; // Assuming SRGB for standard textures
	desc.usage = BufferUsage::TransferDst | BufferUsage::Uniform; // Mapping to Vulkan usages in backend

	texture->handle = graphicsTexture->createTexture(desc, pixels);

	stbi_image_free(pixels);

	return texture;
}

void TextureManager::createTextureSampler(Texture* texture) {
	SamplerDesc desc{};
	desc.enableAnisotropy = true;
	desc.maxAnisotropy = 16.0f; // Could come from config
	desc.minFilter = Filter::Linear;
	desc.magFilter = Filter::Linear;
	desc.mipmapMode = SamplerMipmapMode::Linear;
	desc.addressModeU = SamplerAddressMode::Repeat;
	desc.addressModeV = SamplerAddressMode::Repeat;
	desc.addressModeW = SamplerAddressMode::Repeat;
	desc.minLod = 0.0f;
	desc.maxLod = 1000.0f; // VK_LOD_CLAMP_NONE

	texture->sampler = graphicsTexture->createSampler(desc);
}

void TextureManager::createThumbnailSampler(ThumbnailTexture* texture) {
	SamplerDesc desc{};
	desc.enableAnisotropy = false;
	desc.maxAnisotropy = 1.0f;
	desc.minFilter = Filter::Linear;
	desc.magFilter = Filter::Linear;
	desc.mipmapMode = SamplerMipmapMode::Nearest;
	desc.addressModeU = SamplerAddressMode::ClampToEdge;
	desc.addressModeV = SamplerAddressMode::ClampToEdge;
	desc.addressModeW = SamplerAddressMode::ClampToEdge;
	desc.minLod = 0.0f;
	desc.maxLod = 0.0f;

	texture->sampler = graphicsTexture->createSampler(desc);
}

Texture* TextureManager::getTexture(const std::string& path) {
	// Normalize the file path key to a consistent form for storage/lookups.
	std::string key;

	key = std::filesystem::path(path).generic_string();

	auto it = textures.find(key);
	if (it != textures.end()) {
		return &it->second;
	}
	std::cerr << "TextureManager::getTexture: texture not found for path='" << key << "' (original='" << path << "')"
			  << std::endl;

	// Fallback to default if not found?
	if (key != kDefaultTextureKey) {
		auto defIt = textures.find(kDefaultTextureKey);
		if (defIt != textures.end()) {
			return &defIt->second;
		}
	}

	throw std::runtime_error("Texture not found: " + key);
}

const std::unordered_map<std::string, Texture>& TextureManager::getAllTextures() {
	return textures;
}

void TextureManager::registerTexture(const std::string& path, const Texture& texture) {
	std::string key;

	key = std::filesystem::path(path).generic_string();

	textures[key] = texture;
	std::cerr << "TextureManager::registerTexture: registered texture path='" << key << "'" << std::endl;
}
