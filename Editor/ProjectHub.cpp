#include "ProjectHub.h"
#include "Widgets.h"

#include <core/Globals.h>
#include <core/Log.h>
#include <core/Version.h>
#include <platform/FileDialog.h>
#include <platform/FileSystem.h>
#include <platform/Process.h>
#include <project/Project.h>
#include <renderer/Renderer.h>
#include <ui/Icons.h>
#include <ui/Style.h>
#include <ui/UISystem.h>

#include <imgui.h>
#include <imgui_stdlib.h>

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
        constexpr f32 kSidebarWidth = 280.0f;
        constexpr f32 kRowHeight = 56.0f;
        constexpr f32 kWhenWidth = 120.0f;
        constexpr f32 kDialogWidth = 600.0f;
        constexpr f32 kFieldLabelWidth = 80.0f;

        std::tm ToLocal(std::time_t time) {
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &time);
#else
            localtime_r(&time, &local);
#endif
            return local;
        }

        // "Just now", "5 minutes ago", "2 hours ago", "Yesterday", "Oct 20", "Oct 20, 2025".
        std::string FormatWhen(std::time_t when) {
            if (when <= 0) return "Never";
            std::time_t now = std::time(nullptr);
            double seconds = std::difftime(now, when);
            auto plural = [](long count, const char* unit) {
                return std::to_string(count) + " " + unit + (count == 1 ? "" : "s") + " ago";
            };
            if (seconds < 60.0) return "Just now";
            if (seconds < 3600.0) return plural(static_cast<long>(seconds / 60.0), "minute");

            std::tm then = ToLocal(when);
            std::tm today = ToLocal(now);
            bool sameDay = then.tm_year == today.tm_year && then.tm_yday == today.tm_yday;
            if (sameDay) return plural(static_cast<long>(seconds / 3600.0), "hour");
            std::tm yesterday = ToLocal(now - 24 * 3600);
            if (then.tm_year == yesterday.tm_year && then.tm_yday == yesterday.tm_yday) return "Yesterday";

            char text[32];
            std::strftime(text, sizeof(text), then.tm_year == today.tm_year ? "%b %d" : "%b %d, %Y", &then);
            return text;
        }

        bool ContainsIgnoringCase(const std::string& text, const std::string& part) {
            auto it = std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return it != text.end();
        }

        // Text cut with an ellipsis to fit `width`.
        void DrawClippedText(ImDrawList* draw, ImFont* font, ImVec2 position, f32 width, u32 color,
                             const std::string& text) {
            ImVec4 clip(position.x, position.y, position.x + width, position.y + font->FontSize * 2.0f);
            if (font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text.c_str()).x <= width) {
                draw->AddText(font, font->FontSize, position, UI::ToImColor(color), text.c_str());
                return;
            }
            const char* ellipsis = "\xe2\x80\xa6";
            f32 ellipsisWidth = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, ellipsis).x;
            std::string cut = text;
            while (!cut.empty() && font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, cut.c_str()).x + ellipsisWidth > width) {
                cut.pop_back();
                // Never split a UTF-8 sequence.
                while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) cut.pop_back();
            }
            cut += ellipsis;
            draw->AddText(font, font->FontSize, position, UI::ToImColor(color), cut.c_str(), nullptr, 0.0f, &clip);
        }

        // A button as wide as the sidebar, its label on the left.
        bool SidebarButton(const char* label, bool primary) {
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Sizes::Space5, ImGui::GetStyle().FramePadding.y));
            ImVec2 size(-FLT_MIN, 30.0f);
            bool clicked = false;
            if (primary) {
                clicked = Widgets::PrimaryButton(label, size);
            } else {
                ImGui::PushFont(GetFont(TextStyle::UIMedium));
                clicked = ImGui::Button(label, size);
                ImGui::PopFont();
            }
            ImGui::PopStyleVar(2);
            return clicked;
        }
    } // namespace

    ProjectHub::ProjectHub(Platform::IInput& input, Renderer::Renderer& renderer, UI::UISystem& ui, std::string notice)
        : m_Input(input), m_Renderer(renderer), m_UI(ui), m_Notice(std::move(notice)), m_NoticeIsError(!m_Notice.empty()) {
    }

    void ProjectHub::OnInitialize() {
        m_Recent.Load();
        RefreshMissing();
        m_Templates = FindProjectTemplates();
        if (!m_Recent.Get().empty()) m_Selected = 0;
        // The hub keeps no window layout.
        m_UI.SetLayoutFile({});
    }

    void ProjectHub::OnUpdate(FrameTime /*ft*/) {
        Draw();
        if (m_Renderer.BeginFrame()) {
            m_UI.Render();
            m_Renderer.EndFrame();
        } else {
            m_UI.Render();
        }
    }

    void ProjectHub::RefreshMissing() {
        m_Missing.clear();
        for (const RecentProject& project : m_Recent.Get()) {
            m_Missing.push_back(Project::Project::FindProjectFile(project.Folder).empty());
        }
    }

    void ProjectHub::SetNotice(std::string notice, bool isError) {
        m_Notice = std::move(notice);
        m_NoticeIsError = isError;
    }

    void ProjectHub::Draw() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Sizes::Space4, Sizes::Space4));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoDocking;
        ImGui::Begin("##ProjectHub", nullptr, flags);
        ImGui::PopStyleVar();

        DrawSidebar();
        ImGui::SameLine(0.0f, 0.0f);
        DrawRecentProjects();
        DrawNewProjectDialog();

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ProjectHub::DrawSidebar() {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Widgets::ToVec4(Colors::BgFrame));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Sizes::Space7, Sizes::Space8));
        ImGui::BeginChild("##Sidebar", ImVec2(kSidebarWidth, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        Widgets::Text("VKE", Colors::Text, TextStyle::Display);
        std::string version = std::string("Engine ") + kEngineVersion;
        Widgets::Text(version.c_str(), Colors::TextMuted, TextStyle::MonoSmall);
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space6));

        std::string newLabel = std::string(UI::Icons::FolderPlus) + "  New project";
        if (SidebarButton(newLabel.c_str(), true)) BeginNewProject();
        std::string openLabel = std::string(UI::Icons::FolderOpen) + "  Open folder\xe2\x80\xa6";
        if (SidebarButton(openLabel.c_str(), false)) OpenFolder();

        // A note at the bottom about what a project is.
        const char* note = "A project is a folder with a .vkproject file and its assets/.";
        ImFont* font = GetFont(TextStyle::UI);
        f32 wrap = ImGui::GetContentRegionAvail().x;
        ImVec2 noteSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, wrap, note);
        f32 bottom = ImGui::GetWindowHeight() - Sizes::Space7 - noteSize.y;
        if (bottom > ImGui::GetCursorPosY()) ImGui::SetCursorPosY(bottom);
        ImGui::PushStyleColor(ImGuiCol_Text, Widgets::ToVec4(Colors::TextMuted));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(note);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // The line between the sidebar and the list.
        ImVec2 min = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x + kSidebarWidth - 1.0f, min.y),
                                            ImVec2(min.x + kSidebarWidth - 1.0f, min.y + ImGui::GetWindowHeight()),
                                            UI::ToImColor(Colors::Line));
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void ProjectHub::DrawRecentProjects() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Sizes::Space8, Sizes::Space8));
        ImGui::BeginChild("##Recent", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PushFont(GetFont(TextStyle::Title));
        ImGui::TextUnformatted("Recent projects");
        f32 titleHeight = ImGui::GetItemRectSize().y;
        ImGui::PopFont();
        f32 searchWidth = 240.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - searchWidth);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::floor((titleHeight - Sizes::ControlHeight) * 0.5f));
        Widgets::SearchField("##SearchProjects", m_Search, "Search projects", searchWidth);
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));

        // Column names over a line.
        f32 width = ImGui::GetContentRegionAvail().x;
        ImVec2 header = ImGui::GetCursorScreenPos();
        ImFont* small = GetFont(TextStyle::UISmall);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        f32 nameX = header.x + 13.0f + 40.0f + Sizes::Space5;
        draw->AddText(small, small->FontSize, ImVec2(nameX, header.y), UI::ToImColor(Colors::TextMuted), "NAME");
        const char* whenLabel = "LAST OPENED";
        f32 whenWidth = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, whenLabel).x;
        draw->AddText(small, small->FontSize, ImVec2(header.x + width - 13.0f - whenWidth, header.y),
                      UI::ToImColor(Colors::TextMuted), whenLabel);
        f32 lineY = header.y + small->FontSize + Sizes::Space3;
        draw->AddLine(ImVec2(header.x, lineY), ImVec2(header.x + width, lineY), UI::ToImColor(Colors::Line));
        ImGui::Dummy(ImVec2(width, lineY - header.y + Sizes::Space3 - ImGui::GetStyle().ItemSpacing.y));

        // The list, leaving room for the footer.
        f32 footerHeight = Sizes::ControlHeight + Sizes::Space6 + 1.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, Sizes::Space1));
        ImGui::BeginChild("##RecentList", ImVec2(0.0f, -footerHeight - Sizes::Space4));
        const std::vector<RecentProject>& projects = m_Recent.Get();
        std::vector<size_t> shown;
        for (size_t i = 0; i < projects.size(); ++i) {
            if (m_Search.empty() || ContainsIgnoringCase(projects[i].Name, m_Search) ||
                ContainsIgnoringCase(projects[i].Folder.u8string(), m_Search)) {
                shown.push_back(i);
            }
        }
        for (size_t index : shown) {
            DrawProjectRow(index, ImGui::GetContentRegionAvail().x);
        }
        if (projects.empty()) {
            ImGui::Dummy(ImVec2(0.0f, Sizes::Space4));
            Widgets::Text("No projects yet. Create one, or open a project's folder.", Colors::TextMuted);
        } else if (shown.empty()) {
            ImGui::Dummy(ImVec2(0.0f, Sizes::Space4));
            Widgets::Text("No project matches the search.", Colors::TextMuted);
        }

        // Up and down move the selection, Enter opens it, when no field takes the keys.
        if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup) && !shown.empty()) {
            auto position = std::find(shown.begin(), shown.end(), static_cast<size_t>(std::max<i64>(m_Selected, 0)));
            i64 place = position == shown.end() ? -1 : position - shown.begin();
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) place = std::min<i64>(place + 1, static_cast<i64>(shown.size()) - 1);
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) place = std::max<i64>(place - 1, 0);
            if (place >= 0) m_Selected = static_cast<i64>(shown[static_cast<size_t>(place)]);
            if ((ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) && m_Selected >= 0) {
                OpenProject(projects[static_cast<size_t>(m_Selected)].Folder);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        // Footer: a line, the hint or notice, and the buttons for the selected project.
        ImVec2 footer = ImGui::GetCursorScreenPos();
        draw->AddLine(ImVec2(footer.x, footer.y), ImVec2(footer.x + width, footer.y), UI::ToImColor(Colors::Line));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Sizes::Space6);
        ImGui::AlignTextToFramePadding();
        if (m_Notice.empty()) {
            Widgets::Text("Double-click or press Enter to open.", Colors::TextMuted);
        } else {
            Widgets::Text(m_Notice.c_str(), m_NoticeIsError ? Colors::Danger : Colors::TextMuted);
        }

        bool hasSelection = m_Selected >= 0 && static_cast<size_t>(m_Selected) < projects.size();
        bool canOpen = hasSelection && !m_Missing[static_cast<size_t>(m_Selected)];
        ImGui::PushFont(GetFont(TextStyle::UIMedium));
        f32 removeWidth = ImGui::CalcTextSize("Remove from list").x + 2.0f * ImGui::GetStyle().FramePadding.x;
        f32 openWidth = ImGui::CalcTextSize("Open").x + 2.0f * ImGui::GetStyle().FramePadding.x;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - removeWidth - openWidth - Sizes::Space4);
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("Remove from list") && hasSelection) {
            m_Recent.Remove(projects[static_cast<size_t>(m_Selected)].Folder);
            m_Recent.Save();
            RefreshMissing();
            m_Selected = std::min<i64>(m_Selected, static_cast<i64>(m_Recent.Get().size()) - 1);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!canOpen);
        if (ImGui::Button("Open") && canOpen) OpenProject(projects[static_cast<size_t>(m_Selected)].Folder);
        ImGui::EndDisabled();
        ImGui::PopFont();

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    void ProjectHub::DrawProjectRow(size_t index, f32 width) {
        const RecentProject& project = m_Recent.Get()[index];
        bool missing = m_Missing[index];
        bool selected = m_Selected == static_cast<i64>(index);

        ImGui::PushID(static_cast<int>(index));
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max(min.x + width, min.y + kRowHeight);
        ImGui::PushStyleColor(ImGuiCol_Header, Widgets::ToVec4(0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Widgets::ToVec4(0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, Widgets::ToVec4(0));
        if (ImGui::Selectable("##project", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, kRowHeight))) {
            m_Selected = static_cast<i64>(index);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !missing) OpenProject(project.Folder);
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (selected) {
            draw->AddRectFilled(min, max, UI::ToImColor(Colors::AccentSoft), Sizes::RadiusMd);
            draw->AddRect(min, max, UI::ToImColor(Colors::Accent), Sizes::RadiusMd);
        } else if (hovered) {
            draw->AddRectFilled(min, max, UI::ToImColor(Colors::BgHover), Sizes::RadiusMd);
        }

        // The first letter of the name in a square.
        ImVec2 iconMin(min.x + Sizes::Space5, min.y + Sizes::Space4);
        ImVec2 iconMax(iconMin.x + 40.0f, iconMin.y + 40.0f);
        draw->AddRectFilled(iconMin, iconMax, UI::ToImColor(Colors::BgRaised), Sizes::RadiusMd);
        draw->AddRect(iconMin, iconMax, UI::ToImColor(Colors::Line), Sizes::RadiusMd);
        std::string letter = project.Name.empty() ? "?" : std::string(1, static_cast<char>(std::toupper(
                                                                          static_cast<unsigned char>(project.Name[0]))));
        ImFont* heading = GetFont(TextStyle::Heading);
        ImVec2 letterSize = heading->CalcTextSizeA(heading->FontSize, FLT_MAX, 0.0f, letter.c_str());
        draw->AddText(heading, heading->FontSize,
                      ImVec2(std::floor(iconMin.x + (40.0f - letterSize.x) * 0.5f),
                             std::floor(iconMin.y + (40.0f - letterSize.y) * 0.5f)),
                      UI::ToImColor(Colors::TextMuted), letter.c_str());

        // Name and folder, then when it was opened, on the right.
        ImFont* strong = GetFont(TextStyle::UIStrong);
        ImFont* mono = GetFont(TextStyle::MonoSmall);
        f32 textX = iconMax.x + Sizes::Space5;
        f32 textWidth = max.x - Sizes::Space5 - kWhenWidth - Sizes::Space5 - textX;
        f32 textTop = std::floor(min.y + (kRowHeight - strong->FontSize - mono->FontSize - 2.0f) * 0.5f);
        u32 nameColor = missing ? Colors::TextFaint : Colors::Text;
        DrawClippedText(draw, strong, ImVec2(textX, textTop), textWidth, nameColor, project.Name);
        if (missing) {
            f32 nameWidth = strong->CalcTextSizeA(strong->FontSize, FLT_MAX, 0.0f, project.Name.c_str()).x;
            ImVec2 saved = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(textX + nameWidth + Sizes::Space4, textTop));
            Widgets::Badge("MISSING", Colors::Danger);
            ImGui::SetCursorScreenPos(saved);
        }
        DrawClippedText(draw, mono, ImVec2(textX, textTop + strong->FontSize + 2.0f), textWidth,
                        missing ? Colors::TextFaint : Colors::TextMuted, project.Folder.generic_u8string());

        std::string when = FormatWhen(project.LastOpened);
        f32 whenWidth = mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, when.c_str()).x;
        draw->AddText(mono, mono->FontSize,
                      ImVec2(max.x - 13.0f - whenWidth, std::floor(min.y + (kRowHeight - mono->FontSize) * 0.5f)),
                      UI::ToImColor(Colors::TextMuted), when.c_str());
    }

    void ProjectHub::BeginNewProject() {
        SetNotice({}, false);
        m_CreateError.clear();
        m_Template = 0;
        std::filesystem::path location = Platform::FileSystem::GetDocumentsDirectory() / "VKE";
        m_NewLocation = location.generic_u8string();
        // A name whose folder isn't taken yet.
        m_NewName = "NewProject";
        std::error_code ec;
        for (int suffix = 2; std::filesystem::exists(location / std::filesystem::u8path(m_NewName), ec); ++suffix) {
            m_NewName = "NewProject" + std::to_string(suffix);
        }
        m_OpenNewProjectDialog = true;
    }

    std::filesystem::path ProjectHub::GetNewProjectFolder() const {
        return std::filesystem::u8path(m_NewLocation) / std::filesystem::u8path(m_NewName);
    }

    std::string ProjectHub::CheckNewProject() const {
        if (m_Templates.empty()) return "No templates were found next to the editor.";
        std::string problem = CheckProjectName(m_NewName);
        if (!problem.empty()) return problem;
        if (m_NewLocation.empty()) return "Choose where to create the project.";
        std::error_code ec;
        std::filesystem::path folder = GetNewProjectFolder();
        if (std::filesystem::exists(folder, ec) && !std::filesystem::is_empty(folder, ec)) {
            return "A folder with that name exists there and isn't empty.";
        }
        return {};
    }

    void ProjectHub::CreateNewProject() {
        std::optional<Project::Project> project =
            CreateProject(m_Templates[m_Template], GetNewProjectFolder(), m_NewName);
        if (!project) {
            m_CreateError = "The project couldn't be created. The log says why.";
            return;
        }
        ImGui::CloseCurrentPopup();
        OpenProject(project->GetFolder());
    }

    void ProjectHub::DrawTemplateCard(size_t index) {
        const ProjectTemplate& projectTemplate = m_Templates[index];
        constexpr f32 kCardWidth = 200.0f;
        constexpr f32 kArtHeight = 72.0f;
        f32 padding = Sizes::Space5;

        ImFont* strong = GetFont(TextStyle::UIStrong);
        ImFont* text = GetFont(TextStyle::UI);
        f32 innerWidth = kCardWidth - 2.0f * padding;
        ImVec2 descriptionSize =
            text->CalcTextSizeA(text->FontSize, FLT_MAX, innerWidth, projectTemplate.Description.c_str());
        f32 height = padding + kArtHeight + Sizes::Space3 + strong->FontSize + Sizes::Space1 + descriptionSize.y + padding;

        ImGui::PushID(static_cast<int>(index));
        ImVec2 min = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##template", ImVec2(kCardWidth, height))) m_Template = index;
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 max(min.x + kCardWidth, min.y + height);
        bool selected = m_Template == index;
        draw->AddRectFilled(min, max, UI::ToImColor(Colors::BgRaised), Sizes::RadiusLg);
        u32 border = selected ? Colors::Accent : hovered ? Colors::ControlBorder : Colors::Line;
        draw->AddRect(min, max, UI::ToImColor(border), Sizes::RadiusLg, 0, selected ? 2.0f : 1.0f);

        ImVec2 artMin(min.x + padding, min.y + padding);
        ImVec2 artMax(max.x - padding, artMin.y + kArtHeight);
        draw->AddRectFilled(artMin, artMax, UI::ToImColor(Colors::BgField), Sizes::RadiusMd);
        ImFont* icons = GetFont(TextStyle::Icons32);
        ImVec2 iconSize = icons->CalcTextSizeA(icons->FontSize, FLT_MAX, 0.0f, UI::Icons::Box);
        draw->AddText(icons, icons->FontSize,
                      ImVec2(std::floor((artMin.x + artMax.x - iconSize.x) * 0.5f),
                             std::floor((artMin.y + artMax.y - iconSize.y) * 0.5f)),
                      UI::ToImColor(Colors::TextMuted), UI::Icons::Box);

        f32 y = artMax.y + Sizes::Space3;
        draw->AddText(strong, strong->FontSize, ImVec2(artMin.x, y), UI::ToImColor(Colors::Text),
                      projectTemplate.Name.c_str());
        y += strong->FontSize + Sizes::Space1;
        draw->AddText(text, text->FontSize, ImVec2(artMin.x, y), UI::ToImColor(Colors::TextMuted),
                      projectTemplate.Description.c_str(), nullptr, innerWidth);
    }

    void ProjectHub::DrawNewProjectDialog() {
        const char* id = "New project##Dialog";
        if (m_OpenNewProjectDialog) {
            ImGui::OpenPopup(id);
            m_OpenNewProjectDialog = false;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 96.0f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(kDialogWidth, 0.0f));
        Widgets::PushDialogStyle();
        bool open = ImGui::BeginPopupModal(id, nullptr,
                                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        Widgets::PopDialogStyle();
        if (!open) return;

        if (Widgets::DialogHeader("New project") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        Widgets::Text("TEMPLATE", Colors::TextMuted, TextStyle::UISmall);
        for (size_t i = 0; i < m_Templates.size(); ++i) {
            if (i > 0) ImGui::SameLine(0.0f, Sizes::Space5);
            DrawTemplateCard(i);
        }
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space4));

        auto label = [](const char* text) {
            ImGui::AlignTextToFramePadding();
            Widgets::Text(text, Colors::TextMuted);
            ImGui::SameLine(kFieldLabelWidth);
        };
        label("Name");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool submitted = ImGui::InputText("##Name", &m_NewName, ImGuiInputTextFlags_EnterReturnsTrue);

        label("Location");
        std::string browse = std::string(UI::Icons::FolderOpen) + "  Browse\xe2\x80\xa6";
        f32 browseWidth = ImGui::CalcTextSize(browse.c_str()).x + 2.0f * ImGui::GetStyle().FramePadding.x;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - Sizes::Space3);
        ImGui::PushFont(GetFont(TextStyle::Mono));
        submitted |= ImGui::InputText("##Location", &m_NewLocation, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopFont();
        ImGui::SameLine(0.0f, Sizes::Space3);
        if (ImGui::Button(browse.c_str())) {
            std::error_code ec;
            std::filesystem::path start = std::filesystem::u8path(m_NewLocation);
            if (!std::filesystem::is_directory(start, ec)) start = Platform::FileSystem::GetDocumentsDirectory();
            if (auto folder = Platform::FileDialog::PickFolder(start)) m_NewLocation = folder->generic_u8string();
        }

        // Where the project goes, or why it can't be created.
        std::string problem = CheckNewProject();
        ImGui::Dummy(ImVec2(kFieldLabelWidth - ImGui::GetStyle().ItemSpacing.x, 0.0f));
        ImGui::SameLine(kFieldLabelWidth);
        if (!m_CreateError.empty()) {
            Widgets::Text(m_CreateError.c_str(), Colors::Danger, TextStyle::MonoSmall);
        } else if (!problem.empty()) {
            Widgets::Text(problem.c_str(), Colors::Danger, TextStyle::MonoSmall);
        } else {
            std::string file = (GetNewProjectFolder() / std::filesystem::u8path(m_NewName + Project::Project::kExtension))
                                   .generic_u8string();
            Widgets::Text(("Creates " + file).c_str(), Colors::TextMuted, TextStyle::MonoSmall);
        }

        ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));
        ImGui::PushFont(GetFont(TextStyle::UIMedium));
        f32 cancelWidth = ImGui::CalcTextSize("Cancel").x + 2.0f * ImGui::GetStyle().FramePadding.x;
        f32 createWidth = ImGui::CalcTextSize("Create").x + 2.0f * ImGui::GetStyle().FramePadding.x;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - cancelWidth - createWidth - Sizes::Space4);
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::PopFont();
        ImGui::SameLine(0.0f, Sizes::Space4);
        bool valid = problem.empty();
        if ((Widgets::PrimaryButton("Create", ImVec2(0.0f, 0.0f), valid) || (submitted && valid))) {
            m_CreateError.clear();
            CreateNewProject();
        }

        ImGui::EndPopup();
    }

    void ProjectHub::OpenFolder() {
        SetNotice({}, false);
        std::optional<std::filesystem::path> folder = Platform::FileDialog::PickFolder();
        if (!folder) return;
        OpenProject(*folder);
    }

    bool ProjectHub::OpenProject(const std::filesystem::path& folder) {
        if (Project::Project::FindProjectFile(folder).empty()) {
            SetNotice("'" + folder.filename().u8string() + "' has no .vkproject file.", true);
            return false;
        }
        std::filesystem::path editor = Platform::FileSystem::GetExecutablePath();
        if (!Platform::Process::Launch(editor, {"--project", folder.u8string()})) {
            SetNotice("The editor couldn't be started. The log says why.", true);
            return false;
        }
        VKE_CORE_INFO("ProjectHub: opening '{}'", folder.u8string());
        RequestExit();
        return true;
    }

} // namespace VKE::Editor
