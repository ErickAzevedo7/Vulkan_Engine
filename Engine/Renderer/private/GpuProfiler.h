#pragma once

#include <core/Types.h>
#include <renderer/RenderTypes.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHITimestampPool.h>

#include <string>
#include <string_view>
#include <vector>

namespace VKE {

// Times the parts of each frame on the GPU. The parts recorded in a frame are read back when
// its frame-in-flight slot comes round again, so the timings shown are a few frames old. Does
// nothing on a device that can't write timestamps.
class GpuProfiler {
public:
	GpuProfiler(RHIDevice &device, u32 framesInFlight);

	// Right after `cmd` begins slot `frameIndex`, once the GPU is done with the slot's last
	// frame: takes that frame's timings, and opens the part that covers the whole frame.
	void BeginFrame(RHICommandBuffer &cmd, u32 frameIndex);
	// Closes the whole frame's part, before `cmd` ends.
	void EndFrame(RHICommandBuffer &cmd);

	// Opens a part inside the one open now. A part that doesn't fit in the frame's slots is
	// left out, with the parts inside it.
	void Begin(RHICommandBuffer &cmd, std::string_view name);
	void End(RHICommandBuffer &cmd);

	// The last frame read back, in the order its parts began.
	const std::vector<GpuTiming> &GetTimings() const { return m_Timings; }

private:
	struct Part {
		std::string Name;
		u32 Depth = 0;
		u32 BeginSlot = 0;
		u32 EndSlot = 0;
	};

	// One frame in flight's timestamps and the parts they bound.
	struct Frame {
		Iunique<RHITimestampPool> Pool;
		// The first PartCount are this frame's; all keep their storage between frames.
		std::vector<Part> Parts;
		u32 PartCount = 0;
		u32 UsedSlots = 0;
	};

	void Collect(Frame &frame);

	std::vector<Frame> m_Frames;
	// The frame being recorded; null outside BeginFrame/EndFrame, or without timestamps.
	Frame *m_Current = nullptr;
	// Indices into m_Current->Parts of the parts open now, innermost last.
	std::vector<u32> m_Open;
	std::vector<u64> m_Ticks;
	std::vector<GpuTiming> m_Timings;
	f64 m_MillisecondsPerTick = 0.0;
	u64 m_TickMask = 0;
};

// Times the commands recorded while it lives as a part named `name`.
class GpuScope {
public:
	GpuScope(GpuProfiler &profiler, RHICommandBuffer &cmd, std::string_view name) : m_Profiler(profiler), m_Cmd(cmd) {
		m_Profiler.Begin(m_Cmd, name);
	}
	~GpuScope() { m_Profiler.End(m_Cmd); }

	GpuScope(const GpuScope &) = delete;
	GpuScope &operator=(const GpuScope &) = delete;

private:
	GpuProfiler &m_Profiler;
	RHICommandBuffer &m_Cmd;
};

} // namespace VKE
