#include "ConsolePanel.h"
#include "DevTools.h"
#include "Widgets.h"

#include <ui/Icons.h>
#include <ui/Style.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>

namespace VKE::Editor {

    using UI::GetFont;
    using UI::TextStyle;
    namespace Colors = UI::Colors;
    namespace Sizes = UI::Sizes;

    namespace {
        constexpr size_t kMaxRows = 5000;

        const char* GetIcon(LogLevel level) {
            switch (level) {
            case LogLevel::Warning: return UI::Icons::TriangleAlert;
            case LogLevel::Error: return UI::Icons::CircleX;
            default: return UI::Icons::Info;
            }
        }

        u32 GetColor(LogLevel level) {
            switch (level) {
            case LogLevel::Warning: return Colors::Warning;
            case LogLevel::Error: return Colors::Danger;
            default: return Colors::Info;
            }
        }

        // Local time, to the millisecond: "14:06:02.118".
        std::string FormatTime(std::chrono::system_clock::time_point time) {
            std::time_t seconds = std::chrono::system_clock::to_time_t(time);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &seconds);
#else
            localtime_r(&seconds, &local);
#endif
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count() % 1000;
            char text[16];
            std::snprintf(text, sizeof(text), "%02d:%02d:%02d.%03d", local.tm_hour, local.tm_min, local.tm_sec,
                          static_cast<int>(millis));
            return text;
        }

        bool ContainsIgnoringCase(const std::string& text, const std::string& part) {
            auto it = std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return it != text.end();
        }

        // The file and line of "file(line): text" or "file(line,column): text". A path can have
        // parentheses of its own, so this looks for the ones that end the location.
        bool ParseSource(const std::string& message, std::string& outFile, u32& outLine) {
            for (size_t close = message.find("):"); close != std::string::npos; close = message.find("):", close + 1)) {
                size_t open = message.rfind('(', close);
                if (open == std::string::npos || open == 0 || open + 1 == close) continue;
                size_t digits = open + 1;
                while (digits < close && std::isdigit(static_cast<unsigned char>(message[digits]))) ++digits;
                if (digits == open + 1 || (digits != close && message[digits] != ',')) continue;
                outFile = message.substr(0, open);
                outLine = static_cast<u32>(std::strtoul(message.c_str() + open + 1, nullptr, 10));
                return true;
            }
            return false;
        }
    } // namespace

    const char* ConsolePanel::GetWindowName() {
        static const std::string name = std::string(UI::Icons::Terminal) + "  Console###Console";
        return name.c_str();
    }

    void ConsolePanel::Update() {
        m_Incoming.clear();
        LogCapture::Take(m_Incoming);
        for (LogEntry& entry : m_Incoming) {
            ++m_Counts[static_cast<size_t>(entry.Level)];
            if (!m_Rows.empty()) {
                Row& last = m_Rows.back();
                if (last.Entry.Level == entry.Level && last.Entry.Category == entry.Category &&
                    last.Entry.Message == entry.Message) {
                    ++last.Count;
                    last.Entry.Time = entry.Time;
                    continue;
                }
            }
            Row row{std::move(entry), 1};
            if (row.Entry.Category == kCompilerCategory) ParseSource(row.Entry.Message, row.File, row.Line);
            m_Rows.push_back(std::move(row));
            if (m_Rows.size() > kMaxRows) {
                m_Rows.pop_front();
                if (m_Selected >= 0) --m_Selected;
            }
        }
        if (!m_Incoming.empty()) m_ScrollToBottom = true;
    }

    const LogEntry* ConsolePanel::GetLast() const {
        return m_Rows.empty() ? nullptr : &m_Rows.back().Entry;
    }

    void ConsolePanel::RemoveCategory(const std::string& category) {
        auto removed = std::remove_if(m_Rows.begin(), m_Rows.end(), [&](const Row& row) {
            if (row.Entry.Category != category) return false;
            m_Counts[static_cast<size_t>(row.Entry.Level)] -= row.Count;
            return true;
        });
        if (removed != m_Rows.end()) m_Selected = -1;
        m_Rows.erase(removed, m_Rows.end());
    }

    bool ConsolePanel::Passes(const Row& row) const {
        if (!m_ShowLevel[static_cast<size_t>(row.Entry.Level)]) return false;
        if (m_Filter.empty()) return true;
        return ContainsIgnoringCase(row.Entry.Message, m_Filter) || ContainsIgnoringCase(row.Entry.Category, m_Filter);
    }

    void ConsolePanel::Draw(bool* open) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool visible = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (visible) {
            DrawToolbar();
            DrawRows();
        }
        ImGui::End();
    }

    void ConsolePanel::DrawToolbar() {
        // Below the tab bar, which is part of the window.
        ImGui::SetCursorPos(ImVec2(Sizes::Space4, ImGui::GetCursorPosY() + Sizes::Space2));
        if (Widgets::GhostButton("Clear")) {
            m_Rows.clear();
            m_Counts = {};
            m_Selected = -1;
        }

        const LogLevel levels[] = {LogLevel::Info, LogLevel::Warning, LogLevel::Error};
        for (LogLevel level : levels) {
            size_t index = static_cast<size_t>(level);
            ImGui::SameLine(0.0f, Sizes::Space2);
            // The icon in its level's color, then the count.
            std::string label = std::string("##level") + std::to_string(index);
            std::string text = std::string(GetIcon(level)) + " " + std::to_string(m_Counts[index]);
            if (Widgets::ToggleButton((text + label).c_str(), m_ShowLevel[index])) {
                m_ShowLevel[index] = !m_ShowLevel[index];
            }
            // Repaint the icon over the button's text in the level's color.
            ImFont* font = GetFont(TextStyle::UISmall);
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text.c_str());
            ImVec2 textPos(min.x + (max.x - min.x - textSize.x) * 0.5f, min.y + (max.y - min.y - textSize.y) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(font, font->FontSize, textPos, UI::ToImColor(GetColor(level)),
                                                GetIcon(level));
        }

        f32 searchWidth = 240.0f;
        f32 right = ImGui::GetWindowContentRegionMax().x - Sizes::Space4;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), right - searchWidth));
        Widgets::SearchField("##ConsoleFilter", m_Filter, "Filter messages", searchWidth);
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space1));
        ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y - 1.0f),
                                            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                                                   ImGui::GetCursorScreenPos().y - 1.0f),
                                            UI::ToImColor(Colors::Line));
    }

    void ConsolePanel::DrawRows() {
        std::vector<size_t> visible;
        visible.reserve(m_Rows.size());
        for (size_t i = 0; i < m_Rows.size(); ++i) {
            if (Passes(m_Rows[i])) visible.push_back(i);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        if (ImGui::BeginChild("##ConsoleRows", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None)) {
            bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
            ImFont* mono = GetFont(TextStyle::Mono);
            ImFont* monoSmall = GetFont(TextStyle::MonoSmall);
            ImFont* small = GetFont(TextStyle::UISmall);
            ImDrawList* draw = ImGui::GetWindowDrawList();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(visible.size()), Sizes::ConsoleRowHeight);
            while (clipper.Step()) {
                for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
                    size_t index = visible[static_cast<size_t>(line)];
                    const Row& row = m_Rows[index];
                    const LogEntry& entry = row.Entry;

                    ImVec2 min = ImGui::GetCursorScreenPos();
                    f32 width = ImGui::GetContentRegionAvail().x;
                    ImVec2 max(min.x + width, min.y + Sizes::ConsoleRowHeight);
                    if (line % 2 == 1) draw->AddRectFilled(min, max, UI::ToImColor(Colors::BgHeader & 0xFFFFFF73));

                    ImGui::PushID(static_cast<int>(index));
                    bool selected = m_Selected == static_cast<i64>(index);
                    if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_None,
                                          ImVec2(width, Sizes::ConsoleRowHeight))) {
                        m_Selected = static_cast<i64>(index);
                    }
                    if (!row.File.empty() && ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetTooltip("Double-click to open %s at line %u", row.File.c_str(), row.Line);
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            OpenInCodeEditor(m_SourceFolder / std::filesystem::u8path(row.File), row.Line);
                        }
                    }
                    std::string time = FormatTime(entry.Time);
                    if (ImGui::BeginPopupContextItem("##rowmenu")) {
                        m_Selected = static_cast<i64>(index);
                        if (ImGui::MenuItem("Copy")) {
                            ImGui::SetClipboardText(
                                (time + " [" + entry.Category + "] " + entry.Message).c_str());
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();

                    // Icon, time, category and message, each centered on the row.
                    auto centerY = [&](ImFont* font) { return min.y + std::floor((max.y - min.y - font->FontSize) * 0.5f); };
                    f32 x = min.x + Sizes::Space4;
                    draw->AddText(small, small->FontSize, ImVec2(x, centerY(small)), UI::ToImColor(GetColor(entry.Level)),
                                  GetIcon(entry.Level));
                    x += small->FontSize + Sizes::Space4;
                    draw->AddText(monoSmall, monoSmall->FontSize, ImVec2(x, centerY(monoSmall)),
                                  UI::ToImColor(Colors::TextFaint), time.c_str());
                    x += monoSmall->CalcTextSizeA(monoSmall->FontSize, FLT_MAX, 0.0f, time.c_str()).x + Sizes::Space4;
                    std::string category = "[" + entry.Category + "]";
                    draw->AddText(mono, mono->FontSize, ImVec2(x, centerY(mono)), UI::ToImColor(Colors::TextMuted),
                                  category.c_str());
                    x += mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, category.c_str()).x + Sizes::Space4;

                    f32 messageEnd = max.x - Sizes::Space4;
                    if (row.Count > 1) {
                        std::string count = std::to_string(row.Count);
                        ImVec2 countSize = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, count.c_str());
                        ImVec2 pillMax(max.x - Sizes::Space4, min.y + (max.y - min.y + 14.0f) * 0.5f);
                        ImVec2 pillMin(pillMax.x - countSize.x - 10.0f, pillMax.y - 14.0f);
                        draw->AddRectFilled(pillMin, pillMax, UI::ToImColor(Colors::BgRaised), 7.0f);
                        draw->AddText(small, small->FontSize, ImVec2(pillMin.x + 5.0f, centerY(small)),
                                      UI::ToImColor(Colors::TextMuted), count.c_str());
                        messageEnd = pillMin.x - Sizes::Space4;
                    }
                    ImVec4 clip(x, min.y, messageEnd, max.y);
                    u32 messageColor = entry.Level == LogLevel::Error ? Colors::Danger : Colors::Text;
                    draw->AddText(mono, mono->FontSize, ImVec2(x, centerY(mono)), UI::ToImColor(messageColor),
                                  entry.Message.c_str(), nullptr, 0.0f, &clip);
                }
            }
            clipper.End();

            if (m_ScrollToBottom && atBottom) ImGui::SetScrollHereY(1.0f);
            m_ScrollToBottom = false;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

} // namespace VKE::Editor
