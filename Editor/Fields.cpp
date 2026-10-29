#include "Fields.h"
#include "Display.h"

#include <scene/Properties.h>
#include <ui/Icons.h>
#include <ui/Style.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace VKE::Editor::Fields {

    using UI::GetFont;
    using UI::TextStyle;
    using UI::ToImColor;
    namespace Colors = UI::Colors;
    namespace Icons = UI::Icons;
    namespace Sizes = UI::Sizes;

    namespace {
        constexpr f32 kPi = 3.14159265358979f;
        // The axis letter's cell in a vector field.
        constexpr f32 kAxisWidth = 16.0f;
        // The color swatch.
        constexpr f32 kSwatchWidth = 36.0f;
        constexpr f32 kSwatchHeight = 12.0f;

        // Numbers in mono, in a frame as tall as the others.
        void PushMono() {
            ImFont* mono = GetFont(TextStyle::Mono);
            ImGui::PushFont(mono);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                ImVec2(Sizes::Space3, std::floor((Sizes::ControlHeight - mono->FontSize) * 0.5f)));
        }

        void PopMono() {
            ImGui::PopStyleVar();
            ImGui::PopFont();
        }

        // The resize cursor over a number, unless it is being typed into.
        void ShowDragCursor() {
            if (ImGui::IsItemHovered() && !ImGui::TempInputIsActive(ImGui::GetItemID())) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
        }

        // The focus border around the last item while it is typed into.
        void DrawFocusBorder() {
            if (ImGui::TempInputIsActive(ImGui::GetItemID())) {
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                    ToImColor(Colors::Focus), Sizes::RadiusSm);
            }
        }

        // A field's well, for fields drawn by hand.
        void DrawWell(const ImVec2& min, const ImVec2& max, u32 fill, u32 border) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ToImColor(fill), Sizes::RadiusSm);
            draw->AddRect(min, max, ToImColor(border), Sizes::RadiusSm);
        }

        // `text` in `style`, centered vertically in a row that starts at `top`.
        void DrawText(TextStyle style, f32 x, f32 top, u32 color, const char* text, f32 clipX = FLT_MAX) {
            ImFont* font = GetFont(style);
            f32 y = top + std::floor((Sizes::ControlHeight - font->FontSize) * 0.5f);
            ImVec4 clip(x, top, clipX, top + Sizes::ControlHeight);
            ImGui::GetWindowDrawList()->AddText(font, font->FontSize, ImVec2(x, y), ToImColor(color), text, nullptr,
                                                0.0f, clipX < FLT_MAX ? &clip : nullptr);
        }

        f32 ToSrgb(f32 linear) {
            linear = std::clamp(linear, 0.0f, 1.0f);
            return linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
        }

        f32 ToLinear(f32 srgb) {
            srgb = std::clamp(srgb, 0.0f, 1.0f);
            return srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
        }

        u32 ToRGBA(const f32 srgb[3]) {
            auto byte = [](f32 value) { return static_cast<u32>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
            return (byte(srgb[0]) << 24) | (byte(srgb[1]) << 16) | (byte(srgb[2]) << 8) | 0xFF;
        }

        // One number of a vector field: the axis letter, which drags it, and the number.
        bool AxisNumber(int axis, f32& value, f32 width, f32 speed, const char* format) {
            static constexpr const char* kLetters[] = {"X", "Y", "Z"};
            static constexpr u32 kColors[] = {Colors::AxisX, Colors::AxisY, Colors::AxisZ};

            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 min = ImGui::GetCursorScreenPos();
            ImVec2 max(min.x + width, min.y + Sizes::ControlHeight);
            bool changed = false;

            // The widgets go above the well, which is drawn after them, once their state is known.
            draw->ChannelsSplit(2);
            draw->ChannelsSetCurrent(1);
            ImGui::PushID(axis);

            ImGui::InvisibleButton("##Axis", ImVec2(kAxisWidth, Sizes::ControlHeight));
            bool hovered = ImGui::IsItemHovered();
            if (hovered || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.x != 0.0f) {
                value += ImGui::GetIO().MouseDelta.x * speed;
                changed = true;
            }

            ImGui::SameLine(0.0f, 0.0f);
            PushMono();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::SetNextItemWidth(width - kAxisWidth);
            changed |= ImGui::DragFloat("##Value", &value, speed, 0.0f, 0.0f, format);
            ShowDragCursor();
            hovered |= ImGui::IsItemHovered();
            bool typing = ImGui::TempInputIsActive(ImGui::GetItemID());
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            PopMono();
            ImGui::PopID();

            draw->ChannelsSetCurrent(0);
            DrawWell(min, max, hovered ? Colors::BgFieldHover : Colors::BgField,
                     typing ? Colors::Focus : Colors::ControlBorder);
            draw->AddLine(ImVec2(min.x + kAxisWidth, min.y + 1.0f), ImVec2(min.x + kAxisWidth, max.y - 1.0f),
                          ToImColor(Colors::Line));
            ImFont* font = GetFont(TextStyle::UISmall);
            ImVec2 letter = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, kLetters[axis]);
            draw->AddText(font, font->FontSize,
                          ImVec2(std::floor(min.x + (kAxisWidth - letter.x) * 0.5f),
                                 std::floor(min.y + (Sizes::ControlHeight - letter.y) * 0.5f)),
                          ToImColor(kColors[axis]), kLetters[axis]);
            draw->ChannelsMerge();
            return changed;
        }
    } // namespace

    void Label(const char* label, f32 labelWidth) {
        ImVec2 position = ImGui::GetCursorScreenPos();
        ImVec2 textSize = ImGui::CalcTextSize(label);
        f32 top = position.y + std::floor((Sizes::ControlHeight - ImGui::GetFontSize()) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ToImColor(Colors::TextMuted)));
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImVec2(position.x, top),
                                  ImVec2(position.x + labelWidth, position.y + Sizes::ControlHeight),
                                  position.x + labelWidth, position.x + labelWidth, label, nullptr, &textSize);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(labelWidth, Sizes::ControlHeight));
        // The full name of a label cut short.
        if (textSize.x > labelWidth && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label);
        ImGui::SameLine(0.0f, Sizes::Space4);
    }

    bool Float(const char* id, f32& value, f32 speed, f32 min, f32 max, const char* format) {
        PushMono();
        bool clamped = min < max;
        bool changed = ImGui::DragFloat(id, &value, speed, clamped ? min : 0.0f, clamped ? max : 0.0f, format,
                                        clamped ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
        ShowDragCursor();
        DrawFocusBorder();
        PopMono();
        return changed;
    }

    bool Angle(const char* id, f32& radians, f32 minRadians, f32 maxRadians) {
        constexpr f32 kToDegrees = 180.0f / kPi;
        f32 degrees = radians * kToDegrees;
        // Bounds that aren't set stay unset: FLT_MAX in degrees would overflow.
        f32 min = minRadians == Scene::kNoMinimum ? Scene::kNoMinimum : minRadians * kToDegrees;
        f32 max = maxRadians == Scene::kNoMaximum ? Scene::kNoMaximum : maxRadians * kToDegrees;
        if (!Float(id, degrees, 0.5f, min, max, "%.1f\xc2\xb0")) return false;
        radians = degrees / kToDegrees;
        return true;
    }

    bool Vec3Field(const char* id, Vec3& value, f32 speed, const char* format) {
        ImGui::PushID(id);
        f32 width = std::floor((ImGui::CalcItemWidth() - 2.0f * Sizes::Space2) / 3.0f);
        bool changed = false;
        for (int axis = 0; axis < 3; ++axis) {
            if (axis > 0) ImGui::SameLine(0.0f, Sizes::Space2);
            changed |= AxisNumber(axis, value[axis], width, speed, format);
        }
        ImGui::PopID();
        return changed;
    }

    bool Checkbox(const char* id, bool& value) {
        constexpr f32 kSize = 14.0f;
        ImVec2 position = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton(id, ImVec2(kSize, Sizes::ControlHeight));
        bool hovered = ImGui::IsItemHovered();
        if (clicked) value = !value;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 min(position.x, position.y + std::floor((Sizes::ControlHeight - kSize) * 0.5f));
        ImVec2 max(min.x + kSize, min.y + kSize);
        constexpr f32 kRadius = 2.0f;
        if (value) {
            u32 fill = hovered ? Colors::AccentHover : Colors::Accent;
            draw->AddRectFilled(min, max, ToImColor(fill), kRadius);
            ImFont* font = GetFont(TextStyle::UISmall);
            ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, Icons::Check);
            ImVec2 checkPosition(std::floor(min.x + (kSize - size.x) * 0.5f), std::floor(min.y + (kSize - size.y) * 0.5f));
            draw->AddText(font, font->FontSize, checkPosition, ToImColor(Colors::OnAccent), Icons::Check);
        } else {
            draw->AddRectFilled(min, max, ToImColor(hovered ? Colors::BgFieldHover : Colors::BgField), kRadius);
            draw->AddRect(min, max, ToImColor(Colors::ControlBorder), kRadius);
        }
        return clicked;
    }

    bool Enum(const char* id, const Scene::EnumInfo& info, void* value) {
        u32 current = info.Read(value);
        std::string preview = std::to_string(current);
        for (u32 i = 0; i < info.Count; ++i) {
            if (info.Values[i].Value == current) preview = Display::ToWords(info.Values[i].Name);
        }

        ImVec2 position = ImGui::GetCursorScreenPos();
        f32 width = ImGui::CalcItemWidth();
        bool changed = false;
        ImGui::SetNextItemWidth(width);
        if (ImGui::BeginCombo(id, preview.c_str(), ImGuiComboFlags_NoArrowButton)) {
            for (u32 i = 0; i < info.Count; ++i) {
                const Scene::EnumValue& item = info.Values[i];
                bool selected = item.Value == current;
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable("##Item", false, ImGuiSelectableFlags_None, ImVec2(0.0f, Sizes::RowHeight))) {
                    info.Write(value, item.Value);
                    changed = !selected;
                }
                if (selected) ImGui::SetItemDefaultFocus();
                ImVec2 min = ImGui::GetItemRectMin();
                f32 top = min.y + std::floor((Sizes::RowHeight - Sizes::ControlHeight) * 0.5f);
                // The current value's check, in a column of its own.
                if (selected) DrawText(TextStyle::UI, min.x + Sizes::Space1, top, Colors::Accent, Icons::Check);
                std::string name = Display::ToWords(item.Name);
                DrawText(TextStyle::UI, min.x + Sizes::Space1 + 14.0f + Sizes::Space3, top, Colors::Text, name.c_str());
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        // The chevron where ImGui's arrow button would be.
        ImFont* font = GetFont(TextStyle::UI);
        f32 chevronWidth = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, Icons::ChevronDown).x;
        DrawText(TextStyle::UI, position.x + width - Sizes::Space3 - chevronWidth, position.y, Colors::TextMuted,
                 Icons::ChevronDown);
        return changed;
    }

    bool Color(const char* id, Vec3& linear) {
        f32 brightest = std::max({linear.r, linear.g, linear.b});
        f32 hdr = std::max(1.0f, brightest);
        f32 srgb[3] = {ToSrgb(linear.r / hdr), ToSrgb(linear.g / hdr), ToSrgb(linear.b / hdr)};

        ImGui::PushID(id);
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = ImGui::CalcItemWidth();
        ImVec2 max(min.x + width, min.y + Sizes::ControlHeight);
        if (ImGui::InvisibleButton("##Field", ImVec2(width, Sizes::ControlHeight))) ImGui::OpenPopup("##Picker");
        bool hovered = ImGui::IsItemHovered();
        bool open = ImGui::IsPopupOpen("##Picker");

        DrawWell(min, max, hovered ? Colors::BgFieldHover : Colors::BgField,
                 open ? Colors::Focus : Colors::ControlBorder);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 swatchMin(min.x + Sizes::Space3, min.y + std::floor((Sizes::ControlHeight - kSwatchHeight) * 0.5f));
        ImVec2 swatchMax(swatchMin.x + kSwatchWidth, swatchMin.y + kSwatchHeight);
        u32 rgba = ToRGBA(srgb);
        draw->AddRectFilled(swatchMin, swatchMax, ToImColor(rgba), 2.0f);
        draw->AddRect(swatchMin, swatchMax, ToImColor(Colors::Line), 2.0f);

        char hex[16];
        std::snprintf(hex, sizeof(hex), "#%06X", rgba >> 8);
        DrawText(TextStyle::Mono, swatchMax.x + Sizes::Space3, min.y, Colors::Text, hex);
        if (hdr > 1.0f) {
            char factor[24];
            std::snprintf(factor, sizeof(factor), "HDR \xc3\x97%.1f", hdr);
            ImFont* small = GetFont(TextStyle::UISmall);
            f32 factorWidth = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, factor).x;
            DrawText(TextStyle::UISmall, max.x - Sizes::Space3 - factorWidth, min.y, Colors::TextMuted, factor);
        }

        bool changed = false;
        if (ImGui::BeginPopup("##Picker")) {
            ImGui::SetNextItemWidth(220.0f);
            ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
                                        ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
                                        ImGuiColorEditFlags_DisplayHex;
            if (ImGui::ColorPicker3("##Color", srgb, flags)) {
                linear = Vec3(ToLinear(srgb[0]), ToLinear(srgb[1]), ToLinear(srgb[2])) * hdr;
                changed = true;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
        return changed;
    }

    AssetAction Asset(const char* id, const char* icon, const std::string& name, const char* typeName,
                      const std::function<bool(Assets::AssetID)>& accepts, Assets::AssetID& dropped) {
        enum class Drop { None, Accepted, Refused };

        ImGui::PushID(id);
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = ImGui::CalcItemWidth();
        ImVec2 max(min.x + width, min.y + Sizes::ControlHeight);
        ImGui::InvisibleButton("##Field", ImVec2(width, Sizes::ControlHeight));
        bool hovered = ImGui::IsItemHovered();
        AssetAction action = AssetAction::None;

        Drop drop = Drop::None;
        if (ImGui::BeginDragDropTarget()) {
            ImGuiDragDropFlags flags =
                ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetPayload, flags)) {
                Assets::AssetID asset = *static_cast<const Assets::AssetID*>(payload->Data);
                drop = accepts(asset) ? Drop::Accepted : Drop::Refused;
                if (drop == Drop::Accepted && payload->IsDelivery()) {
                    dropped = asset;
                    action = AssetAction::Drop;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##Menu")) {
            if (ImGui::MenuItem("Clear", nullptr, false, !name.empty())) action = AssetAction::Clear;
            ImGui::EndPopup();
        }

        u32 fill = drop == Drop::Accepted ? Colors::AccentSoft
                   : drop == Drop::Refused ? Colors::DangerSoft
                   : hovered               ? Colors::BgFieldHover
                                           : Colors::BgField;
        u32 border = drop == Drop::Accepted  ? Colors::Accent
                     : drop == Drop::Refused ? Colors::Danger
                                             : Colors::ControlBorder;
        DrawWell(min, max, fill, border);

        f32 x = min.x + Sizes::Space3;
        DrawText(TextStyle::UI, x, min.y, drop == Drop::Refused ? Colors::Danger : Colors::TextMuted,
                 drop == Drop::Refused ? Icons::Ban : icon);
        x += 14.0f + Sizes::Space3;
        std::string text;
        u32 color = Colors::Text;
        if (drop == Drop::Refused) {
            text = std::string("Not a ") + typeName;
            color = Colors::Danger;
        } else if (name.empty()) {
            text = std::string("None (") + typeName + ")";
            color = Colors::TextFaint;
        } else {
            text = name;
        }
        DrawText(TextStyle::UI, x, min.y, color, text.c_str(), max.x - Sizes::Space3);
        if (hovered && drop == Drop::None && !name.empty()) ImGui::SetTooltip("%s", name.c_str());

        ImGui::PopID();
        return action;
    }

} // namespace VKE::Editor::Fields
