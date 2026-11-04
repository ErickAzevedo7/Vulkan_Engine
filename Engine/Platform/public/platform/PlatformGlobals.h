#pragma once

// Engine-wide, non-owning access to the active Window/Input instances.
// Populated by PlatformSystem::OnInitialize, cleared in OnShutdown.

namespace VKE {
class IWindow;
class IInput;
} // namespace VKE

namespace VKE {

extern IWindow *Window;
extern IInput *Input;

} // namespace VKE
