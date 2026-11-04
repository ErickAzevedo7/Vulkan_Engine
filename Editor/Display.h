#pragma once

#include <scene/ComponentRegistry.h>

#include <string>
#include <string_view>

namespace VKE {
class Component;
class GameObject;
} // namespace VKE

// How the editor names and pictures the engine's types: words from code names, and an icon
// for each kind of component and object.

namespace VKE::Editor::Display {

// A code name in words, for labels: "InnerConeAngle" is "Inner cone angle".
std::string ToWords(std::string_view name);
// A component type's name in words, without the "Component" at its end: "Light".
std::string GetComponentLabel(const ComponentTypeInfo &type);

const char *GetComponentIcon(const Component &component);
// The icon of the object's first component after the transform. Objects with no other
// component show a group if they have children, or an empty object.
const char *GetObjectIcon(const GameObject &object);

} // namespace VKE::Editor::Display
