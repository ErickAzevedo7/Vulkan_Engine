#pragma once

#include <core/Types.h>

namespace VKE {

class RHIDevice;

// Slots the GPU writes its clock into as it reaches points in a command buffer, to time the
// work between them. A command buffer resets the slots it uses, then writes them; once that
// submission is done, Read() gives the ticks (see DeviceLimits::TimestampPeriod).
class RHITimestampPool {
public:
	virtual ~RHITimestampPool() = default;

	// Only when DeviceLimits::TimestampValidBits isn't 0.
	static Iunique<RHITimestampPool> Create(RHIDevice &device, u32 count);

	virtual u32 GetCount() const = 0;

	// Copies `count` ticks starting at slot `first`, without waiting. False when any of
	// them hasn't been written since its reset, or its submission isn't done.
	virtual bool Read(u32 first, u32 count, u64 *outTicks) = 0;
};

} // namespace VKE
