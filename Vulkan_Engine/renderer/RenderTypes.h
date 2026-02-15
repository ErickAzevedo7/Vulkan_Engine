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

enum class BufferUsage {
	Vertex, // Vertex buffer
	Index, // Index buffer
	Uniform, // Uniform/constant buffer
	Storage, // Storage/structured buffer
	Staging // CPU-to-GPU transfer buffer
};

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
