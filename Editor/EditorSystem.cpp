#include "EditorSystem.h"
#include "RecentProjects.h"
#include "Widgets.h"

#include <assets/AssetManager.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <platform/FileDialog.h>
#include <platform/FileSystem.h>
#include <platform/PlatformSystem.h>
#include <platform/Process.h>
#include <platform/Window.h>
#include <renderer/Renderer.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <ui/Icons.h>
#include <ui/Style.h>
#include <ui/UISystem.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <fstream>

namespace VKE::Editor {

    using UI::GetFont;
    using UI::TextStyle;
    namespace Colors = UI::Colors;
    namespace Icons = UI::Icons;
    namespace Sizes = UI::Sizes;

    namespace {
        const std::vector<Platform::FileDialog::Filter> kSceneFilter = {{"Scene", "*.scene"}};

        std::string WithIcon(const char* icon, const char* label) {
            return std::string(icon) + "  " + label;
        }
    } // namespace

    EditorSystem::EditorSystem(Project::Project project, Platform::PlatformSystem& platform,
                               Assets::AssetManager& assets, Renderer::Renderer& renderer, UI::UISystem& ui)
        : m_Project(std::move(project)),
          m_Platform(platform),
          m_Assets(assets),
          m_Renderer(renderer),
          m_UI(ui),
          m_Scene(assets),
          m_Viewport(renderer, ui) {
    }

    EditorSystem::~EditorSystem() = default;

    void EditorSystem::OnInitialize() {
        // The editor decides about closing, to ask about unsaved changes first.
        m_Platform.SetExitOnClose(false);
        m_Viewport.Create();

        std::error_code ec;
        std::filesystem::create_directories(m_Project.GetSettingsFolder(), ec);
        m_UI.SetLayoutFile(m_Project.GetSettingsFolder() / "Layout.ini");
        LoadEditorState();

        RecentProjects recent;
        recent.Load();
        recent.Add(m_Project);
        recent.Save();

        VKE_CORE_INFO("Editor: opened project '{}' in '{}'", m_Project.GetName(), m_Project.GetFolder().u8string());
        bool opened = false;
        for (Assets::AssetID scene : {m_LastScene, m_Project.GetStartScene()}) {
            if (!opened && scene.IsValid() && m_Assets.GetType(scene) == Assets::AssetType::Scene) opened = OpenScene(scene);
        }
        if (!opened) m_Scene.New();
    }

    void EditorSystem::OnShutdown() {
        SaveEditorState();
        m_Viewport.Destroy();
    }

    void EditorSystem::LoadEditorState() {
        std::ifstream stream(m_Project.GetSettingsFolder() / "Editor.yaml", std::ios::binary);
        if (!stream) return;
        try {
            YAML::Node root = YAML::Load(stream);
            if (root["LastScene"] && root["LastScene"].IsScalar()) {
                m_LastScene = Assets::AssetID::Parse(root["LastScene"].Scalar());
            }
        } catch (const YAML::Exception& error) {
            VKE_CORE_WARN("Editor: the editor's settings can't be read: {}", error.what());
        }
    }

    void EditorSystem::SaveEditorState() const {
        YAML::Emitter out;
        out << YAML::Comment("The editor's own settings for this project.");
        out << YAML::BeginMap;
        if (m_Scene.GetID().IsValid()) {
            out << YAML::Key << "LastScene" << YAML::Value << YAML::DoubleQuoted << m_Scene.GetID().ToString();
        }
        out << YAML::EndMap;
        std::ofstream stream(m_Project.GetSettingsFolder() / "Editor.yaml", std::ios::binary | std::ios::trunc);
        stream << out.c_str() << '\n';
    }

    void EditorSystem::OnUpdate(FrameTime ft) {
        m_Console.Update();
        UpdateFrameStats(ft.GetDeltaF());

        Platform::IWindow& window = m_Platform.GetWindow();
        if (window.ShouldClose()) {
            window.CancelClose();
            Request(Action::Quit);
        }

        HandleShortcuts();
        DrawMenuBar();
        DrawStatusBar();
        DrawDockSpace();

        char stats[96];
        std::snprintf(stats, sizeof(stats), "%.1f ms \xc2\xb7 %.0f fps \xc2\xb7 %zu objects", 1000.0f * m_AverageFrameTime,
                      m_AverageFrameTime > 0.0f ? 1.0f / m_AverageFrameTime : 0.0f,
                      m_Scene.GetScene().GetObjectCount());
        if (m_ShowViewport) m_Viewport.Draw(&m_ShowViewport, stats, m_Scene.GetScene().GetMainCamera() != nullptr);
        if (m_ShowConsole) m_Console.Draw(&m_ShowConsole);
        DrawUnsavedDialog();
        UpdateTitle();

        // Only now: the dialogs can replace the scene.
        Scene::Scene& scene = m_Scene.GetScene();
        scene.Update();

        if (m_Renderer.BeginFrame()) {
            if (m_ShowViewport && m_Viewport.IsVisible()) {
                m_Renderer.DrawScene(scene.GetRenderScene(), scene.GetView(), m_Viewport.GetTexture());
            }
            m_UI.Render();
            m_Renderer.EndFrame();
        } else {
            m_UI.Render();
        }
    }

    void EditorSystem::UpdateFrameStats(f32 deltaTime) {
        constexpr f32 kInterval = 0.5f;
        m_FrameTimeSum += deltaTime;
        ++m_FrameCount;
        if (m_FrameTimeSum >= kInterval) {
            m_AverageFrameTime = m_FrameTimeSum / static_cast<f32>(m_FrameCount);
            m_FrameTimeSum = 0.0f;
            m_FrameCount = 0;
        }
    }

    void EditorSystem::Request(Action action, Assets::AssetID scene) {
        if (m_Scene.IsDirty()) {
            m_PendingAction = action;
            m_PendingScene = scene;
            m_OpenUnsavedDialog = true;
            return;
        }
        Perform(action, scene);
    }

    void EditorSystem::Perform(Action action, Assets::AssetID scene) {
        switch (action) {
        case Action::None: break;
        case Action::NewScene: m_Scene.New(); break;
        case Action::OpenScene: OpenScene(scene); break;
        case Action::CloseProject:
            // Back to the project screen, which is the editor started without a project.
            if (!Platform::Process::Launch(Platform::FileSystem::GetExecutablePath(), {})) return;
            RequestExit();
            break;
        case Action::Quit: RequestExit(); break;
        }
    }

    bool EditorSystem::SaveScene() {
        if (m_Scene.GetFile().empty()) return SaveSceneAs();
        return m_Scene.Save();
    }

    bool EditorSystem::SaveSceneAs() {
        std::filesystem::path folder =
            m_Scene.GetFile().empty() ? m_Project.GetAssetsFolder() : m_Scene.GetFile().parent_path();
        std::string name = m_Scene.GetFile().empty() ? std::string("NewScene") : m_Scene.GetFile().stem().u8string();
        std::optional<std::filesystem::path> file = Platform::FileDialog::SaveFile(kSceneFilter, name, folder);
        if (!file) return false;
        if (!m_Scene.SaveAs(*file)) return false;
        SaveEditorState();
        // A project without a start scene starts in the first one saved.
        if (!m_Project.GetStartScene().IsValid()) SetStartScene();
        return true;
    }

    void EditorSystem::ChooseSceneToOpen() {
        std::filesystem::path folder =
            m_Scene.GetFile().empty() ? m_Project.GetAssetsFolder() : m_Scene.GetFile().parent_path();
        std::optional<std::filesystem::path> file = Platform::FileDialog::OpenFile(kSceneFilter, folder);
        if (!file) return;
        Assets::AssetID scene = m_Assets.FindAsset(file->u8string());
        if (!scene.IsValid() || m_Assets.GetType(scene) != Assets::AssetType::Scene) {
            VKE_CORE_ERROR("Editor: '{}' isn't a scene in the project's assets", file->u8string());
            return;
        }
        Request(Action::OpenScene, scene);
    }

    bool EditorSystem::OpenScene(Assets::AssetID scene) {
        if (!m_Scene.Open(scene)) return false;
        VKE_CORE_INFO("Editor: opened {} ({} objects)", m_Scene.GetFileName(), m_Scene.GetScene().GetObjectCount());
        SaveEditorState();
        return true;
    }

    void EditorSystem::SetStartScene() {
        m_Project.SetStartScene(m_Scene.GetID());
        if (m_Project.Save()) VKE_CORE_INFO("Editor: the project now starts in {}", m_Scene.GetFileName());
    }

    void EditorSystem::HandleShortcuts() {
        // Not while a dialog is open; they work over any panel and text field.
        if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
        ImGuiInputFlags flags = ImGuiInputFlags_RouteGlobal;
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, flags)) Request(Action::NewScene);
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, flags)) ChooseSceneToOpen();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, flags)) SaveScene();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, flags)) SaveSceneAs();
    }

    void EditorSystem::DrawMenuBar() {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(Sizes::Space4, std::floor((Sizes::MenuBarHeight - ImGui::GetFontSize()) * 0.5f)));
        bool open = ImGui::BeginMainMenuBar();
        ImGui::PopStyleVar();
        if (!open) return;

        ImGui::PushFont(GetFont(TextStyle::UIStrong));
        ImGui::TextUnformatted("VKE");
        ImGui::PopFont();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(WithIcon(Icons::FilePlus, "New scene").c_str(), "Ctrl+N")) Request(Action::NewScene);
            if (ImGui::MenuItem(WithIcon(Icons::FolderOpen, "Open scene\xe2\x80\xa6").c_str(), "Ctrl+O")) {
                ChooseSceneToOpen();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(WithIcon(Icons::Save, "Save scene").c_str(), "Ctrl+S")) SaveScene();
            if (ImGui::MenuItem(WithIcon(Icons::Copy, "Save scene as\xe2\x80\xa6").c_str(), "Ctrl+Shift+S")) {
                SaveSceneAs();
            }
            bool isStart = m_Scene.GetID().IsValid() && m_Scene.GetID() == m_Project.GetStartScene();
            if (ImGui::MenuItem(WithIcon(Icons::Play, "Start scene").c_str(), nullptr, isStart,
                                m_Scene.GetID().IsValid() && !isStart)) {
                SetStartScene();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(WithIcon(Icons::LogOut, "Close project").c_str())) Request(Action::CloseProject);
            if (ImGui::MenuItem(WithIcon(Icons::X, "Exit").c_str(), "Alt+F4")) Request(Action::Quit);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GameObject")) {
            auto created = [](Scene::GameObject* object) {
                if (object) VKE_CORE_INFO("Editor: created '{}'", object->GetName());
            };
            if (ImGui::MenuItem(WithIcon(Icons::CircleDashed, "Create empty").c_str())) created(&m_Scene.CreateEmpty());
            if (ImGui::BeginMenu(WithIcon(Icons::Box, "3D object").c_str())) {
                if (ImGui::MenuItem("Cube")) created(m_Scene.CreateShape(Shape::Cube));
                if (ImGui::MenuItem("Sphere")) created(m_Scene.CreateShape(Shape::Sphere));
                if (ImGui::MenuItem("Plane")) created(m_Scene.CreateShape(Shape::Plane));
                if (ImGui::MenuItem("Cylinder")) created(m_Scene.CreateShape(Shape::Cylinder));
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(WithIcon(Icons::Video, "Camera").c_str())) created(&m_Scene.CreateCamera());
            if (ImGui::BeginMenu(WithIcon(Icons::Lightbulb, "Light").c_str())) {
                if (ImGui::MenuItem(WithIcon(Icons::Sun, "Directional").c_str())) {
                    created(&m_Scene.CreateLight(Renderer::LightType::Directional));
                }
                if (ImGui::MenuItem(WithIcon(Icons::Lightbulb, "Point").c_str())) {
                    created(&m_Scene.CreateLight(Renderer::LightType::Point));
                }
                if (ImGui::MenuItem(WithIcon(Icons::Flashlight, "Spot").c_str())) {
                    created(&m_Scene.CreateLight(Renderer::LightType::Spot));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem(WithIcon(Icons::Box, "Viewport").c_str(), nullptr, &m_ShowViewport);
            ImGui::MenuItem(WithIcon(Icons::Terminal, "Console").c_str(), nullptr, &m_ShowConsole);
            ImGui::Separator();
            if (ImGui::MenuItem(WithIcon(Icons::LayoutGrid, "Reset layout").c_str())) {
                m_ShowViewport = true;
                m_ShowConsole = true;
                m_ResetLayout = true;
            }
            ImGui::EndMenu();
        }

        // The project and scene on the right, with a dot while there are unsaved changes.
        std::string title = m_Project.GetName() + "  \xe2\x80\x94  " + m_Scene.GetFileName();
        f32 dotWidth = m_Scene.IsDirty() ? 7.0f + Sizes::Space3 : 0.0f;
        f32 titleWidth = ImGui::CalcTextSize(title.c_str()).x + dotWidth;
        f32 x = ImGui::GetWindowWidth() - titleWidth - Sizes::Space5;
        if (x > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(x);
            ImGui::PushStyleColor(ImGuiCol_Text, Widgets::ToVec4(Colors::TextMuted));
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();
            if (m_Scene.IsDirty()) {
                ImGui::SameLine(0.0f, Sizes::Space3);
                Widgets::Dot(Colors::Accent);
            }
        }
        ImGui::EndMainMenuBar();
    }

    void EditorSystem::DrawStatusBar() {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Widgets::ToVec4(Colors::BgFrame));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Sizes::Space4, 0.0f));
        bool open = ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down,
                                                Sizes::StatusBarHeight, flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (!open) {
            ImGui::End();
            return;
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetWindowPos();
        f32 width = ImGui::GetWindowWidth();
        draw->AddLine(min, ImVec2(min.x + width, min.y), UI::ToImColor(Colors::Line));

        // The newest log message on the left.
        ImFont* small = GetFont(TextStyle::UISmall);
        f32 textY = min.y + std::floor((Sizes::StatusBarHeight - small->FontSize) * 0.5f);
        f32 x = min.x + Sizes::Space4;
        ImFont* mono = GetFont(TextStyle::MonoSmall);
        char frame[32];
        std::snprintf(frame, sizeof(frame), "%.1f ms", 1000.0f * m_AverageFrameTime);
        std::string objects = std::to_string(m_Scene.GetScene().GetObjectCount()) + " objects";
        const char* mode = "Edit";
        f32 gap = Sizes::Space5;
        f32 rightWidth = mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, frame).x +
                         mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, objects.c_str()).x +
                         mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, mode).x + 2.0f * gap;

        if (const LogEntry* last = m_Console.GetLast()) {
            const char* icon = last->Level == LogLevel::Error     ? Icons::CircleX
                               : last->Level == LogLevel::Warning ? Icons::TriangleAlert
                                                                  : Icons::Info;
            u32 color = last->Level == LogLevel::Error     ? Colors::Danger
                        : last->Level == LogLevel::Warning ? Colors::Warning
                                                           : Colors::TextMuted;
            draw->AddText(small, small->FontSize, ImVec2(x, textY), UI::ToImColor(color), icon);
            x += small->FontSize + Sizes::Space2;
            ImVec4 clip(x, min.y, min.x + width - rightWidth - 2.0f * gap, min.y + Sizes::StatusBarHeight);
            draw->AddText(small, small->FontSize, ImVec2(x, textY), UI::ToImColor(Colors::TextMuted),
                          last->Message.c_str(), nullptr, 0.0f, &clip);
        }

        // Frame time, object count and mode on the right.
        f32 right = min.x + width - Sizes::Space4 - rightWidth;
        f32 monoY = min.y + std::floor((Sizes::StatusBarHeight - mono->FontSize) * 0.5f);
        for (const char* text : {static_cast<const char*>(frame), objects.c_str(), mode}) {
            draw->AddText(mono, mono->FontSize, ImVec2(right, monoY), UI::ToImColor(Colors::TextMuted), text);
            right += mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, text).x + gap;
        }
        ImGui::End();
    }

    void EditorSystem::DrawDockSpace() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace = ImHashStr("EditorDockSpace");

        // No saved layout yet, or a reset: the Viewport in the middle, the Console under it.
        bool build = m_ResetLayout || (!m_LayoutChecked && !ImGui::DockBuilderGetNode(dockspace));
        m_LayoutChecked = true;
        m_ResetLayout = false;
        if (build) {
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);
            ImGuiID center = dockspace;
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.3f, nullptr, &center);
            ImGui::DockBuilderDockWindow(ViewportPanel::GetWindowName(), center);
            ImGui::DockBuilderDockWindow(ConsolePanel::GetWindowName(), bottom);
            ImGui::DockBuilderFinish(dockspace);
        }
        ImGui::DockSpaceOverViewport(dockspace, viewport);
    }

    void EditorSystem::DrawUnsavedDialog() {
        const char* id = "Unsaved changes##Dialog";
        if (m_OpenUnsavedDialog) {
            ImGui::OpenPopup(id);
            m_OpenUnsavedDialog = false;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                       viewport->WorkPos.y + viewport->WorkSize.y * 0.35f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));
        Widgets::PushDialogStyle();
        bool open = ImGui::BeginPopupModal(id, nullptr,
                                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        Widgets::PopDialogStyle();
        if (!open) return;

        std::string title = "Save changes to " + m_Scene.GetFileName() + "?";
        bool cancel = Widgets::DialogHeader(title.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        ImGui::PushTextWrapPos(0.0f);
        Widgets::Text("The changes are lost if you don't save them.", Colors::TextMuted);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space5));

        ImGui::PushFont(GetFont(TextStyle::UIMedium));
        const ImGuiStyle& style = ImGui::GetStyle();
        f32 discardWidth = ImGui::CalcTextSize("Don't save").x + 2.0f * style.FramePadding.x;
        f32 cancelWidth = ImGui::CalcTextSize("Cancel").x + 2.0f * style.FramePadding.x;
        f32 saveWidth = ImGui::CalcTextSize("Save").x + 2.0f * style.FramePadding.x;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - discardWidth - cancelWidth - saveWidth -
                             2.0f * Sizes::Space4);
        bool discard = ImGui::Button("Don't save");
        ImGui::SameLine(0.0f, Sizes::Space4);
        cancel |= ImGui::Button("Cancel");
        ImGui::PopFont();
        ImGui::SameLine(0.0f, Sizes::Space4);
        bool save = Widgets::PrimaryButton("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter);

        Action action = m_PendingAction;
        Assets::AssetID scene = m_PendingScene;
        if (cancel || discard || save) {
            ImGui::CloseCurrentPopup();
            m_PendingAction = Action::None;
        }
        ImGui::EndPopup();

        // A save that didn't happen, like a cancelled file dialog, cancels the action too.
        if (save && !SaveScene()) return;
        if (save || discard) Perform(action, scene);
    }

    void EditorSystem::UpdateTitle() {
        std::string title = m_Scene.GetFileName() + (m_Scene.IsDirty() ? " \xe2\x97\x8f" : "") + " \xe2\x80\x94 " +
                            m_Project.GetName() + " \xe2\x80\x94 VKE Editor";
        if (title == m_Title) return;
        m_Title = title;
        m_Platform.GetWindow().SetTitle(title);
    }

} // namespace VKE::Editor
