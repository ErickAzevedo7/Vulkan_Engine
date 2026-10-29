#include "ui/UISystem.h"
#include "ui/Icons.h"
#include "ui/Style.h"
#include "FontFile.h"
#include "KeyMap.h"
#include "Theme.h"

#include <core/Log.h>
#include <platform/FileSystem.h>
#include <platform/Input.h>
#include <platform/Window.h>
#include <renderer/Renderer.h>

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstring>

namespace VKE::UI {

    namespace {
        static_assert(sizeof(ImDrawVert) == sizeof(Renderer::OverlayVertex), "ImDrawVert must match OverlayVertex");
        static_assert(offsetof(ImDrawVert, pos) == offsetof(Renderer::OverlayVertex, Position));
        static_assert(offsetof(ImDrawVert, uv) == offsetof(Renderer::OverlayVertex, UV));
        static_assert(offsetof(ImDrawVert, col) == offsetof(Renderer::OverlayVertex, Color));
        static_assert(sizeof(ImDrawIdx) == sizeof(u16), "The 2D layer takes 16-bit indices");

        std::array<ImFont*, static_cast<size_t>(TextStyle::Count)> s_Fonts{};

        // One text style: which font file, at what size in CSS pixels (the em), and the
        // size of the icons merged into it, 0 for none.
        struct FontStyle {
            const char* File;
            f32 Size;
            f32 IconSize;
        };

        // Indexed by TextStyle. Sizes from the design system's type styles and icon tokens.
        constexpr FontStyle kFontStyles[] = {
            {"IBMPlexSans-Regular.ttf", 13.0f, 14.0f},  // UI: ui
            {"IBMPlexSans-Medium.ttf", 13.0f, 14.0f},   // UIMedium: ui-medium
            {"IBMPlexSans-SemiBold.ttf", 13.0f, 14.0f}, // UIStrong: ui-strong
            {"IBMPlexSans-Medium.ttf", 11.0f, 12.0f},   // UISmall: ui-small
            {"IBMPlexSans-SemiBold.ttf", 15.0f, 0.0f},  // Heading: heading
            {"IBMPlexSans-SemiBold.ttf", 22.0f, 0.0f},  // Title: title
            {"IBMPlexSans-SemiBold.ttf", 40.0f, 0.0f},  // Display: display
            {"JetBrainsMono-Regular.ttf", 12.0f, 0.0f}, // Mono: mono
            {"JetBrainsMono-Regular.ttf", 11.0f, 0.0f}, // MonoSmall: mono-small
            {"lucide.ttf", 16.0f, 0.0f},                // Icons16: icon-md
            {"lucide.ttf", 32.0f, 0.0f},                // Icons32: icon-lg
        };
        static_assert(std::size(kFontStyles) == static_cast<size_t>(TextStyle::Count));

        constexpr const char* kIconFile = "lucide.ttf";

        Platform::CursorShape ToCursorShape(ImGuiMouseCursor cursor) {
            switch (cursor) {
                case ImGuiMouseCursor_None: return Platform::CursorShape::Hidden;
                case ImGuiMouseCursor_TextInput: return Platform::CursorShape::TextInput;
                case ImGuiMouseCursor_ResizeAll: return Platform::CursorShape::ResizeAll;
                case ImGuiMouseCursor_ResizeNS: return Platform::CursorShape::ResizeVertical;
                case ImGuiMouseCursor_ResizeEW: return Platform::CursorShape::ResizeHorizontal;
                case ImGuiMouseCursor_ResizeNESW: return Platform::CursorShape::ResizeDiagonalUp;
                case ImGuiMouseCursor_ResizeNWSE: return Platform::CursorShape::ResizeDiagonalDown;
                case ImGuiMouseCursor_Hand: return Platform::CursorShape::Hand;
                case ImGuiMouseCursor_NotAllowed: return Platform::CursorShape::NotAllowed;
                default: return Platform::CursorShape::Arrow;
            }
        }

        UISystem& GetSystem() {
            return *static_cast<UISystem*>(ImGui::GetIO().BackendPlatformUserData);
        }
    } // namespace

    struct UISystem::FontStorage {
        std::unordered_map<std::string, FontFile> Files;
        ImVector<ImWchar> TextRanges;
        ImVector<ImWchar> IconRanges;
    };

    ImFont* GetFont(TextStyle style) {
        return s_Fonts[static_cast<size_t>(style)];
    }

    size_t UISystem::TextureKeyHash::operator()(const Renderer::OverlayTexture& texture) const {
        u64 key = (static_cast<u64>(texture.Index) << 32) | texture.Generation;
        return std::hash<u64>()(key) ^ (static_cast<size_t>(texture.Type) * 0x9E3779B97F4A7C15ull);
    }

    UISystem::UISystem(Platform::IWindow& window, Platform::IInput& input, Renderer::Renderer& renderer)
        : m_Window(window), m_Input(input), m_Renderer(renderer) {
    }

    UISystem::~UISystem() = default;

    void UISystem::OnInitialize() {
        IMGUI_CHECKVERSION();
        m_Context = ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.BackendPlatformName = "VKE";
        io.BackendRendererName = "VKE";
        io.BackendPlatformUserData = this;
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;
        // No keyboard navigation: with it, a focused panel takes every key and the scene loses
        // its shortcuts. Tab still moves between text fields.
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = nullptr;

        ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
        platformIO.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
            UISystem& self = GetSystem();
            self.m_ClipboardText = self.m_Window.GetClipboardText();
            return self.m_ClipboardText.c_str();
        };
        platformIO.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
            GetSystem().m_Window.SetClipboardText(text ? text : "");
        };

        BuildFonts();
        ApplyTheme(ImGui::GetStyle(), GetFont(TextStyle::UI) ? GetFont(TextStyle::UI)->FontSize : 13.0f);

        VKE_CORE_INFO("UISystem initialized (Dear ImGui {})", IMGUI_VERSION);
    }

    void UISystem::OnShutdown() {
        if (m_FontAtlas.IsValid()) {
            m_Renderer.DestroyOverlayImage(m_FontAtlas);
            m_FontAtlas = {};
        }
        if (m_Context) {
            // ImGui refuses to shut down while a backend still points at itself.
            ImGui::GetIO().BackendPlatformUserData = nullptr;
            ImGui::DestroyContext(m_Context);
            m_Context = nullptr;
        }
        s_Fonts = {};
        VKE_CORE_INFO("UISystem shutdown");
    }

    void UISystem::SetLayoutFile(const std::filesystem::path& file) {
        m_LayoutFile = file.empty() ? std::string() : file.u8string();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = m_LayoutFile.empty() ? nullptr : m_LayoutFile.c_str();
        if (!m_LayoutFile.empty() && std::filesystem::exists(file)) {
            ImGui::LoadIniSettingsFromDisk(m_LayoutFile.c_str());
        }
    }

    void UISystem::BuildFonts() {
        const auto fontDir = Platform::FileSystem::GetEngineContentDirectory() / "Fonts";
        ImFontAtlas& atlas = *ImGui::GetIO().Fonts;

        // Latin-1 covers Portuguese; a few punctuation marks the interface uses come on top.
        ImFontGlyphRangesBuilder textBuilder;
        textBuilder.AddRanges(atlas.GetGlyphRangesDefault());
        textBuilder.AddText("\xe2\x80\x94\xe2\x80\xa6\xe2\x97\x8f\xe2\x86\x92\xe2\x80\xa2"); // — … ● → •
        m_FontStorage = MakeIunique<FontStorage>();
        textBuilder.BuildRanges(&m_FontStorage->TextRanges);

        ImFontGlyphRangesBuilder iconBuilder;
        for (u32 codepoint : Icons::kCodepoints) {
            iconBuilder.AddChar(static_cast<ImWchar>(codepoint));
        }
        iconBuilder.BuildRanges(&m_FontStorage->IconRanges);

        auto load = [&](const char* name) -> const FontFile* {
            auto& files = m_FontStorage->Files;
            auto it = files.find(name);
            if (it == files.end()) {
                it = files.emplace(name, FontFile::Load(fontDir / name)).first;
            }
            return it->second.IsValid() ? &it->second : nullptr;
        };

        const FontFile* iconFile = load(kIconFile);
        for (size_t i = 0; i < std::size(kFontStyles); ++i) {
            const FontStyle& style = kFontStyles[i];
            const FontFile* file = load(style.File);
            if (!file) continue;
            bool iconsOnly = std::strcmp(style.File, kIconFile) == 0;

            ImFontConfig config;
            config.FontDataOwnedByAtlas = false;
            config.OversampleH = 2;
            config.OversampleV = 1;
            std::snprintf(config.Name, sizeof(config.Name), "%s %.0fpx", style.File, style.Size);
            f32 size = file->PixelHeightForEm(style.Size);
            ImFont* font = atlas.AddFontFromMemoryTTF(const_cast<Byte*>(file->Data.data()),
                                                      static_cast<int>(file->Data.size()), size, &config,
                                                      iconsOnly ? m_FontStorage->IconRanges.Data
                                                                : m_FontStorage->TextRanges.Data);

            if (style.IconSize > 0.0f && iconFile) {
                ImFontConfig iconConfig;
                iconConfig.FontDataOwnedByAtlas = false;
                iconConfig.MergeMode = true;
                iconConfig.OversampleH = 2;
                iconConfig.OversampleV = 1;
                iconConfig.GlyphMinAdvanceX = style.IconSize;
                f32 iconSize = iconFile->PixelHeightForEm(style.IconSize);
                // Centers the icon's em box on the text line: ImGui puts glyphs at the text's
                // rounded ascent, and an icon's box sits on the baseline.
                f32 ascent = std::round(file->AscentForEm(style.Size));
                iconConfig.GlyphOffset.y = std::round((size - iconSize) * 0.5f - ascent + iconSize);
                atlas.AddFontFromMemoryTTF(const_cast<Byte*>(iconFile->Data.data()),
                                           static_cast<int>(iconFile->Data.size()), iconSize, &iconConfig,
                                           m_FontStorage->IconRanges.Data);
            }
            s_Fonts[i] = font;
        }

        if (!s_Fonts[static_cast<size_t>(TextStyle::UI)]) {
            VKE_CORE_ERROR("UISystem: fonts missing from {}; using ImGui's built-in font", fontDir.string());
            ImFont* fallback = atlas.AddFontDefault();
            for (ImFont*& font : s_Fonts) {
                if (!font) font = fallback;
            }
        }

        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
        m_FontAtlas = m_Renderer.CreateOverlayImage(static_cast<u32>(width), static_cast<u32>(height), pixels);
        atlas.SetTexID(static_cast<ImTextureID>(GetTextureID(Renderer::OverlayTexture::FromImage(m_FontAtlas))));
        // The atlas texture lives on the GPU now; the font files stay for a rebuild.
        atlas.ClearTexData();
    }

    void UISystem::OnUpdate(FrameTime ft) {
        if (m_FrameOpen) {
            // Render() was not called last frame; close that frame before starting another.
            ImGui::EndFrame();
        }
        FeedInput(ft);
        ImGui::NewFrame();
        m_FrameOpen = true;
    }

    void UISystem::FeedInput(FrameTime ft) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<f32>(m_Window.GetWidth()), static_cast<f32>(m_Window.GetHeight()));
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        io.DeltaTime = std::max(ft.GetDeltaF(), 1e-5f);

        io.AddMousePosEvent(m_Input.GetMouseX(), m_Input.GetMouseY());
        constexpr Platform::MouseButton kButtons[] = {Platform::MouseButton::Left, Platform::MouseButton::Right,
                                                      Platform::MouseButton::Middle, Platform::MouseButton::Button4,
                                                      Platform::MouseButton::Button5};
        for (int i = 0; i < static_cast<int>(std::size(kButtons)); ++i) {
            if (m_Input.IsMouseButtonPressed(kButtons[i])) io.AddMouseButtonEvent(i, true);
            if (m_Input.IsMouseButtonReleased(kButtons[i])) io.AddMouseButtonEvent(i, false);
        }
        if (m_Input.GetScrollDeltaX() != 0.0f || m_Input.GetScrollDeltaY() != 0.0f) {
            io.AddMouseWheelEvent(m_Input.GetScrollDeltaX(), m_Input.GetScrollDeltaY());
        }

        using Platform::KeyCode;
        auto down = [&](KeyCode key) { return m_Input.IsKeyDown(key); };
        io.AddKeyEvent(ImGuiMod_Ctrl, down(KeyCode::LeftControl) || down(KeyCode::RightControl));
        io.AddKeyEvent(ImGuiMod_Shift, down(KeyCode::LeftShift) || down(KeyCode::RightShift));
        io.AddKeyEvent(ImGuiMod_Alt, down(KeyCode::LeftAlt) || down(KeyCode::RightAlt));
        io.AddKeyEvent(ImGuiMod_Super, down(KeyCode::LeftSuper) || down(KeyCode::RightSuper));

        for (u16 i = 1; i < static_cast<u16>(KeyCode::Count); ++i) {
            KeyCode key = static_cast<KeyCode>(i);
            bool pressed = m_Input.IsKeyPressed(key);
            bool released = m_Input.IsKeyReleased(key);
            if (!pressed && !released) continue;
            ImGuiKey imguiKey = ToImGuiKey(key);
            if (imguiKey != ImGuiKey_None) io.AddKeyEvent(imguiKey, pressed);
        }

        for (u32 codepoint : m_Input.GetTypedCharacters()) {
            io.AddInputCharacter(codepoint);
        }
    }

    void UISystem::Render() {
        if (!m_FrameOpen) return;
        m_FrameOpen = false;

        ImGui::Render();
        BuildDrawList();
        m_Renderer.DrawOverlay(m_DrawList);
        ApplyCursor();
    }

    void UISystem::ApplyCursor() {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) return;
        m_Window.SetCursorShape(io.MouseDrawCursor ? Platform::CursorShape::Hidden
                                                   : ToCursorShape(ImGui::GetMouseCursor()));
    }

    void UISystem::BuildDrawList() {
        m_DrawList.Clear();
        const ImDrawData* data = ImGui::GetDrawData();
        if (!data || data->TotalVtxCount == 0) return;

        m_DrawList.Vertices.resize(static_cast<size_t>(data->TotalVtxCount));
        m_DrawList.Indices.resize(static_cast<size_t>(data->TotalIdxCount));

        u32 vertexBase = 0;
        u32 indexBase = 0;
        for (const ImDrawList* list : data->CmdLists) {
            std::memcpy(m_DrawList.Vertices.data() + vertexBase, list->VtxBuffer.Data,
                        static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert));
            std::memcpy(m_DrawList.Indices.data() + indexBase, list->IdxBuffer.Data,
                        static_cast<size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx));

            for (const ImDrawCmd& cmd : list->CmdBuffer) {
                // The only callback ImGui itself issues; it asks for nothing here.
                if (cmd.UserCallback) continue;

                Renderer::OverlayCommand command;
                command.FirstIndex = indexBase + cmd.IdxOffset;
                command.IndexCount = cmd.ElemCount;
                command.VertexOffset = static_cast<i32>(vertexBase + cmd.VtxOffset);
                command.ClipRect = Vec4(cmd.ClipRect.x - data->DisplayPos.x, cmd.ClipRect.y - data->DisplayPos.y,
                                        cmd.ClipRect.z - data->DisplayPos.x, cmd.ClipRect.w - data->DisplayPos.y);
                u64 id = static_cast<u64>(cmd.GetTexID());
                if (id > 0 && id <= m_Textures.size()) {
                    command.Texture = m_Textures[id - 1];
                }
                m_DrawList.Commands.push_back(command);
            }
            vertexBase += static_cast<u32>(list->VtxBuffer.Size);
            indexBase += static_cast<u32>(list->IdxBuffer.Size);
        }
    }

    u64 UISystem::GetTextureID(const Renderer::OverlayTexture& texture) {
        if (texture.Type == Renderer::OverlayTexture::Kind::None) return 0;
        auto [it, inserted] = m_TextureIDs.emplace(texture, m_Textures.size() + 1);
        if (inserted) m_Textures.push_back(texture);
        return it->second;
    }

    bool UISystem::WantsMouse() const {
        return m_Context && ImGui::GetIO().WantCaptureMouse;
    }

    bool UISystem::WantsKeyboard() const {
        return m_Context && ImGui::GetIO().WantCaptureKeyboard;
    }

} // namespace VKE::UI
