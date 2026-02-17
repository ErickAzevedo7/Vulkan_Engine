#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Renderer {

// ============================================================================
// Handle Types (Opaque identifiers for GPU resources)
// ============================================================================

struct BufferHandle {
	uint64_t id = 0;

	bool isValid() const {
		return id != 0;
	}
	bool operator==(const BufferHandle& other) const {
		return id == other.id;
	}
	bool operator!=(const BufferHandle& other) const {
		return id != other.id;
	}
};

// Resource set handle (abstracts descriptor sets, descriptor tables, etc.)
struct ResourceSetHandle {
	uint64_t id = 0;

	bool isValid() const {
		return id != 0;
	}
	bool operator==(const ResourceSetHandle& other) const {
		return id == other.id;
	}
	bool operator!=(const ResourceSetHandle& other) const {
		return id != other.id;
	}
};

// Resource set layout handle (defines what bindings are in a resource set)
struct ResourceSetLayoutHandle {
	uint64_t id = 0;

	bool isValid() const {
		return id != 0;
	}
	bool operator==(const ResourceSetLayoutHandle& other) const {
		return id == other.id;
	}
	bool operator!=(const ResourceSetLayoutHandle& other) const {
		return id != other.id;
	}
};

// ============================================================================
// Enums
// ============================================================================

enum class BufferUsage : uint32_t {
	Vertex = 0x01, // Vertex buffer
	Index = 0x02, // Index buffer
	Uniform = 0x04, // Uniform/constant buffer
	Storage = 0x08, // Storage/structured buffer
	TransferSrc = 0x10, // Source for transfer (staging)
	TransferDst = 0x20 // Destination for transfer
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
	return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
	return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasFlag(BufferUsage flags, BufferUsage flag) {
	return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

enum class MemoryType {
	GpuOnly, // GPU-only memory (best performance)
	CpuToGpu, // CPU-writable, GPU-readable (upload)
	GpuToCpu // GPU-writable, CPU-readable (readback)
};

// ============================================================================
// Descriptor Structures
// ============================================================================

struct BufferDesc {
	size_t size = 0;
	BufferUsage usage = BufferUsage::Vertex;
	MemoryType memory = MemoryType::GpuOnly;
	const char* debugName = nullptr; // Optional debug name
};

// ============================================================================
// Resource Set Types (for binding resources to shaders)
// ============================================================================

// Types of resources that can be bound to shaders
enum class ResourceType {
	UniformBuffer, // Constant/uniform buffer
	UniformBufferDynamic, // Dynamic uniform buffer (offset at bind time)
	StorageBuffer, // Read/write structured buffer
	Sampler, // Texture sampler only
	Texture, // Texture (sampled image)
	CombinedTextureSampler // Texture + sampler combined
};

// Shader stages that can access a resource (bitflags)
enum class ShaderStage : uint32_t {
	Vertex = 0x01,
	Fragment = 0x02,
	Compute = 0x04,
	Geometry = 0x08,
	AllGraphics = Vertex | Fragment | Geometry
};

inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
	return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ShaderStage operator&(ShaderStage a, ShaderStage b) {
	return static_cast<ShaderStage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasFlag(ShaderStage flags, ShaderStage flag) {
	return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// Describes a single resource binding in a resource set
struct ResourceBinding {
	uint32_t binding; // Shader binding index (e.g., binding = 0 in GLSL)
	ResourceType type; // Type of resource
	uint32_t count = 1; // Array size (for texture arrays, etc.)
	ShaderStage stages; // Which shader stages can access this
};

// Describes the layout of a resource set (what bindings it contains)
struct ResourceSetLayoutDesc {
	std::vector<ResourceBinding> bindings;
	const char* debugName = nullptr;
};

// Buffer resource binding for updates
struct ResourceBufferBinding {
	uint32_t binding; // Which binding slot (matches shader)
	BufferHandle buffer; // Abstract buffer handle
	size_t offset = 0; // Offset into buffer
	size_t range = ~0ull; // Size to bind (whole buffer by default)
};

// Image/Texture resource binding for updates (temporary until texture abstraction)
struct ResourceImageBinding {
	uint32_t binding; // Which binding slot (matches shader)
	void* imageView; // Temporary: API-specific image view
	void* sampler; // Temporary: API-specific sampler
};

} // namespace Renderer
