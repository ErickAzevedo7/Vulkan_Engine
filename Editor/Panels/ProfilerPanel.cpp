#include "ProfilerPanel.h"

#include "Widgets.h"

#include <ui/Icons.h>
#include <ui/Style.h>

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace VKE::Editor {

namespace {
constexpr f32 kInterval = 0.5f;
constexpr f32 kMillisecondsWidth = 64.0f;

// `text` right-aligned in the rest of the current cell.
void TextRight(const char *text, u32 color) {
	f32 width = ImGui::CalcTextSize(text).x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
	Widgets::Text(text, color, TextStyle::Mono);
}

// A bar filling `share` of the rest of the current cell.
void ShareBar(f32 share, u32 color) {
	ImVec2 min = ImGui::GetCursorScreenPos();
	f32 width = ImGui::GetContentRegionAvail().x;
	f32 height = ImGui::GetTextLineHeight();
	ImDrawList *draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, ImVec2(min.x + width, min.y + height), ToImColor(Colors::BgField), Sizes::RadiusSm);
	f32 filled = width * std::clamp(share, 0.0f, 1.0f);
	if (filled >= 1.0f) {
		draw->AddRectFilled(min, ImVec2(min.x + filled, min.y + height), ToImColor(color), Sizes::RadiusSm);
	}
	ImGui::Dummy(ImVec2(width, height));
}
} // namespace

const char *ProfilerPanel::GetWindowName() {
	static const std::string name = std::string(Icons::Layers) + "  Profiler###Profiler";
	return name.c_str();
}

void ProfilerPanel::Update(const std::vector<GpuTiming> &timings, f32 deltaTime) {
	// Rows a frame has that the last didn't go right after the row before them in this frame,
	// so each part stays under the one it is inside.
	size_t insertAt = 0;
	for (const GpuTiming &timing : timings) {
		if (m_Paths.size() <= timing.Depth) {
			m_Paths.resize(timing.Depth + 1);
		}
		std::string &path = m_Paths[timing.Depth];
		if (timing.Depth == 0) {
			path = timing.Name;
		} else {
			path = m_Paths[timing.Depth - 1];
			path += '/';
			path += timing.Name;
		}

		auto it = std::find_if(m_Rows.begin(), m_Rows.end(), [&](const Row &row) { return row.Path == path; });
		if (it == m_Rows.end()) {
			Row row;
			row.Path = path;
			row.Name = timing.Name;
			row.Depth = timing.Depth;
			it = m_Rows.insert(
					m_Rows.begin() + static_cast<std::ptrdiff_t>(std::min(insertAt, m_Rows.size())), std::move(row));
		}
		it->Sum += timing.Milliseconds;
		++it->Count;
		insertAt = static_cast<size_t>(it - m_Rows.begin()) + 1;
	}

	m_Elapsed += deltaTime;
	if (m_Elapsed >= kInterval) {
		Publish();
	}
}

void ProfilerPanel::Publish() {
	m_Elapsed = 0.0f;
	m_Rows.erase(
			std::remove_if(m_Rows.begin(), m_Rows.end(), [](const Row &row) { return row.Count == 0; }), m_Rows.end());
	for (Row &row : m_Rows) {
		row.Average = static_cast<f32>(row.Sum / static_cast<f64>(row.Count));
		row.Sum = 0.0;
		row.Count = 0;
	}
	m_Shown = m_Rows;
}

void ProfilerPanel::Draw(bool *open, f32 cpuFrameMs) {
	ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(GetWindowName(), open)) {
		ImGui::End();
		return;
	}

	// The part covering the whole frame comes first.
	f32 gpuFrameMs = !m_Shown.empty() && m_Shown.front().Depth == 0 ? m_Shown.front().Average : 0.0f;

	char text[64];
	std::snprintf(text, sizeof(text), "CPU frame  %.2f ms", cpuFrameMs);
	Widgets::Text(text, Colors::TextMuted, TextStyle::Mono);
	if (m_Shown.empty()) {
		Widgets::Text("No GPU timings yet. The GPU may not be able to time its work.", Colors::TextFaint);
		ImGui::End();
		return;
	}
	std::snprintf(text, sizeof(text), "GPU frame  %.2f ms", gpuFrameMs);
	Widgets::Text(text, Colors::TextMuted, TextStyle::Mono);
	ImGui::Spacing();

	ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY;
	if (ImGui::BeginTable("##GpuParts", 3, flags)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Part", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, kMillisecondsWidth);
		ImGui::TableSetupColumn("Share of frame", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableHeadersRow();

		for (const Row &row : m_Shown) {
			u32 color = row.Depth == 0 ? Colors::Text : Colors::TextMuted;
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + static_cast<f32>(row.Depth) * Sizes::Indent);
			Widgets::Text(row.Name.c_str(), color);

			ImGui::TableNextColumn();
			std::snprintf(text, sizeof(text), "%.2f", row.Average);
			TextRight(text, color);

			ImGui::TableNextColumn();
			ShareBar(gpuFrameMs > 0.0f ? row.Average / gpuFrameMs : 0.0f,
					row.Depth <= 1 ? Colors::Accent : Colors::Info);
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

} // namespace VKE::Editor
