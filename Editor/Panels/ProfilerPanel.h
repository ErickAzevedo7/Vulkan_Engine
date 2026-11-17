#pragma once

#include <core/Types.h>
#include <renderer/RenderTypes.h>

#include <string>
#include <vector>

namespace VKE::Editor {

// Where each frame's time goes on the GPU, part by part, next to the CPU's frame time. The
// numbers are averages over the last half second, so they are steady enough to read.
class ProfilerPanel {
public:
	// The window's title, with its icon.
	static const char *GetWindowName();

	// Takes the renderer's latest timings. Call every frame, shown or not.
	void Update(const std::vector<GpuTiming> &timings, f32 deltaTime);
	// `cpuFrameMs` is the editor's own average frame time.
	void Draw(bool *open, f32 cpuFrameMs);

private:
	struct Row {
		// The names of the parts it is inside and its own, joined by '/'.
		std::string Path;
		std::string Name;
		u32 Depth = 0;
		f64 Sum = 0.0;
		// Frames of this half second it was in; a part like picking isn't in every frame.
		u32 Count = 0;
		f32 Average = 0.0f;
	};

	void Publish();

	std::vector<Row> m_Rows;
	// What Draw shows: the rows of the last half second that ended.
	std::vector<Row> m_Shown;
	// The paths of the parts open while a frame's timings are walked.
	std::vector<std::string> m_Paths;
	f32 m_Elapsed = 0.0f;
};

} // namespace VKE::Editor
