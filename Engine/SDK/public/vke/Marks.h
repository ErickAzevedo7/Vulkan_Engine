#pragma once

// Marks read by the tool that connects a game to the engine. The compiler ignores them.
//
//     class Door : public vke::Script {
//         VKE_SCRIPT(Door)
//
//         VKE_PROPERTY(Min = 0) float m_Speed = 2.0f;
//         VKE_PROPERTY() vke::ObjectRef m_Key;
//     };
//
// VKE_PROPERTY shows a field in the Inspector and saves it with the scene. It takes:
// - `Min = <number>` and `Max = <number>` on a float, to keep it between them in the Inspector.
// - `Display = Color` on a Vec3 holding a color, and `Display = Angle` on a float holding an angle.
// - `Name = "Saved"` to save the field under another name, like after renaming it.
//
// A field can be a bool, int, float, vke::Vec3, vke::Quat, vke::ObjectRef, vke::ScriptRef<T>,
// vke::PrefabRef, vke::MeshRef, vke::MaterialRef, vke::SceneRef, an `enum class` marked VKE_ENUM
// or a struct marked VKE_STRUCT, whose marked fields must be public.

#define VKE_ENUM(...)
#define VKE_STRUCT(...)
#define VKE_PROPERTY(...)

// Marks a function the engine implements. Only the SDK's own headers use it.
#define VKE_API
