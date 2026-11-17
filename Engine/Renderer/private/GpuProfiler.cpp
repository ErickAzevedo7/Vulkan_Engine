#include "GpuProfiler.h"

#include <core/Assert.h>

namespace VKE {

namespace {
// Two per part: enough for every pass of a few views.
constexpr u32 kSlotsPerFrame = 128;
// Marks a part in GpuProfiler::m_Open that was left out.
constexpr u32 kLeftOut = ~0u;
} // namespace

GpuProfiler::GpuProfiler(RHIDevice &device, u32 framesInFlight) {
	const DeviceLimits &limits = device.GetLimits();
	if (limits.TimestampValidBits == 0) {
		return;
	}
	m_MillisecondsPerTick = static_cast<f64>(limits.TimestampPeriod) / 1.0e6;
	m_TickMask = limits.TimestampValidBits >= 64 ? ~0ull : (1ull << limits.TimestampValidBits) - 1;

	m_Frames.resize(framesInFlight);
	for (Frame &frame : m_Frames) {
		frame.Pool = RHITimestampPool::Create(device, kSlotsPerFrame);
	}
	m_Ticks.resize(kSlotsPerFrame);
}

void GpuProfiler::BeginFrame(RHICommandBuffer &cmd, u32 frameIndex) {
	if (m_Frames.empty()) {
		return;
	}
	VKE_CORE_ASSERT(!m_Current, "GpuProfiler::BeginFrame called twice without EndFrame");
	Frame &frame = m_Frames[frameIndex];
	Collect(frame);

	cmd.ResetTimestamps(*frame.Pool, 0, kSlotsPerFrame);
	frame.PartCount = 0;
	frame.UsedSlots = 0;
	m_Current = &frame;
	Begin(cmd, "Frame");
}

void GpuProfiler::EndFrame(RHICommandBuffer &cmd) {
	if (!m_Current) {
		return;
	}
	End(cmd);
	VKE_CORE_ASSERT(m_Open.empty(), "GPU parts left open at the end of the frame");
	m_Open.clear();
	m_Current = nullptr;
}

void GpuProfiler::Begin(RHICommandBuffer &cmd, std::string_view name) {
	if (!m_Current) {
		return;
	}
	Frame &frame = *m_Current;
	// Room for this part's two slots, and for the end of every part open around it.
	if (frame.UsedSlots + 2 + m_Open.size() > kSlotsPerFrame || (!m_Open.empty() && m_Open.back() == kLeftOut)) {
		m_Open.push_back(kLeftOut);
		return;
	}

	if (frame.PartCount == frame.Parts.size()) {
		frame.Parts.emplace_back();
	}
	Part &part = frame.Parts[frame.PartCount];
	part.Name.assign(name);
	part.Depth = static_cast<u32>(m_Open.size());
	part.BeginSlot = frame.UsedSlots++;
	cmd.WriteTimestamp(*frame.Pool, part.BeginSlot);
	m_Open.push_back(frame.PartCount++);
}

void GpuProfiler::End(RHICommandBuffer &cmd) {
	if (!m_Current) {
		return;
	}
	VKE_CORE_ASSERT(!m_Open.empty(), "GpuProfiler::End without a part open");
	u32 index = m_Open.back();
	m_Open.pop_back();
	if (index == kLeftOut) {
		return;
	}
	Frame &frame = *m_Current;
	Part &part = frame.Parts[index];
	part.EndSlot = frame.UsedSlots++;
	cmd.WriteTimestamp(*frame.Pool, part.EndSlot);
}

void GpuProfiler::Collect(Frame &frame) {
	// Nothing was recorded in this slot yet, or its timestamps aren't in: keep the last timings.
	if (frame.UsedSlots == 0 || !frame.Pool->Read(0, frame.UsedSlots, m_Ticks.data())) {
		return;
	}

	m_Timings.resize(frame.PartCount);
	for (u32 i = 0; i < frame.PartCount; ++i) {
		const Part &part = frame.Parts[i];
		// Masked, so a clock that wrapped between the two still gives the right difference.
		u64 ticks = (m_Ticks[part.EndSlot] - m_Ticks[part.BeginSlot]) & m_TickMask;
		GpuTiming &timing = m_Timings[i];
		timing.Name = part.Name;
		timing.Depth = part.Depth;
		timing.Milliseconds = static_cast<f32>(static_cast<f64>(ticks) * m_MillisecondsPerTick);
	}
}

} // namespace VKE
