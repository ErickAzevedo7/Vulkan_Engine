#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>

#include <functional>
#include <string>
#include <vector>

namespace VKE::Scene {
    struct EnumInfo;
    class GameObject;
} // namespace VKE::Scene

// The Inspector's property rows and the fields in them, after the design system's Field,
// Vec3Field, Checkbox, Dropdown, ColorField and AssetField. Each field takes ImGui's item width
// (ImGui::PushItemWidth) and returns true on the frames its value changes.

namespace VKE::Editor::Fields {

    // Drag-and-drop payload of an asset, carrying its AssetID. The AssetBrowser drags it.
    inline constexpr const char* kAssetPayload = "VKE_ASSET";
    // Drag-and-drop payload of an object, carrying its handle. The Hierarchy drags it.
    inline constexpr const char* kObjectPayload = "VKE_OBJECT";

    // Starts a property row: the label in the first `labelWidth` pixels, cut with an ellipsis
    // if it doesn't fit, and the rest of the line for the field that follows.
    void Label(const char* label, f32 labelWidth);

    // A number in mono, changed by dragging sideways or typed after a double click. Kept between
    // `min` and `max` when min < max.
    bool Float(const char* id, f32& value, f32 speed, f32 min, f32 max, const char* format);
    // A whole number, used the same way.
    bool Int(const char* id, i32& value);
    // An angle stored in radians, shown and edited in degrees.
    bool Angle(const char* id, f32& radians, f32 minRadians, f32 maxRadians);
    // Three numbers marked X, Y and Z; dragging a letter drags only its number.
    bool Vec3Field(const char* id, Vec3& value, f32 speed, const char* format);

    // The design system's checkbox: an amber square with a check while on.
    bool Checkbox(const char* id, bool& value);
    // One of `names`, by its index.
    bool Choice(const char* id, const std::vector<std::string>& names, int& current);
    // A value of a reflected enum, by its names in words.
    bool Enum(const char* id, const Scene::EnumInfo& info, void* value);
    // A linear RGB color, shown as its sRGB swatch and hex; a click opens a picker. Values over 1
    // show the HDR factor, and the picker edits the color divided by it.
    bool Color(const char* id, Vec3& linear);

    enum class AssetAction {
        None,
        // An accepted asset was dropped; its ID is in `dropped`.
        Drop,
        // "Clear" was picked in the field's menu.
        Clear,
    };

    // A reference to an asset: the type's icon and the asset's name, or "None (<typeName>)".
    // Takes assets dragged onto it that `accepts` takes, and shows a refusal for the others.
    AssetAction Asset(const char* id, const char* icon, const std::string& name, const char* typeName,
                      const std::function<bool(Assets::AssetID)>& accepts, Assets::AssetID& dropped);
    // The same for a reference to an object, with an object from the Hierarchy in `dropped`.
    AssetAction ObjectReference(const char* id, const std::string& name, Handle<Scene::GameObject>& dropped);

} // namespace VKE::Editor::Fields
