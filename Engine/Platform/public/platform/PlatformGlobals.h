#pragma once

// Engine-wide, non-owning access to the active Window/Input instances.
// Populated by PlatformSystem::OnInitialize, cleared in OnShutdown.

namespace VKE::Platform {
    class IWindow;
    class IInput;
} // namespace VKE::Platform

namespace VKE {

    extern Platform::IWindow* Window;
    extern Platform::IInput* Input;

} // namespace VKE
