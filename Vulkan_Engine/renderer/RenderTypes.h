#pragma once

#include <cstddef>
#include <cstdint>

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

} // namespace Renderer
