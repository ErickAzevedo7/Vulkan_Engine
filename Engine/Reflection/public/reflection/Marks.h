#pragma once

// Marks read by the reflection generator (tools/reflect), which writes the property tables the
// engine saves and edits types with. The compiler ignores them: they expand to nothing.
//
//     VKE_ENUM()
//     enum class LightType : u32 { Directional, Point, Spot };
//
//     VKE_STRUCT()
//     struct LightSettings {
//         VKE_PROPERTY() LightType Type = LightType::Directional;
//         VKE_PROPERTY(Min = 0) f32 Intensity = 1.0f;
//     };
//
// Components are marked by VKE_COMPONENT, which goes first in the class body, and mark their
// members with VKE_PROPERTY like a struct does.
//
// The generator reads only what follows a mark, and refuses what it doesn't understand:
// - VKE_ENUM goes right before an `enum class`, whose values are non-negative integers.
// - VKE_STRUCT goes right before a `struct` or `class`; its marked members must be public.
// - VKE_PROPERTY goes right before one member declaration: a type, a name and an optional
//   initializer. A leading "m_" is dropped from the name the property is saved under.
// - A property is a bool, f32, Vec3, Quat, Handle<Assets::MeshData>,
//   std::vector<Handle<Assets::MaterialData>>, a marked enum or a marked struct.
// - VKE_PROPERTY takes `Name = "Saved"` to save the property under another name, and
//   `Min = <number>` / `Max = <number>` on f32 properties, for editors.
// - VKE_PROPERTY also takes `Display = Color` on a Vec3 holding a color and `Display = Angle` on an
//   f32 holding an angle in radians, so editors show a color picker or degrees.

#define VKE_ENUM(...)
#define VKE_STRUCT(...)
#define VKE_PROPERTY(...)
