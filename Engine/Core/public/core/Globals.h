#pragma once

// Engine-wide global pointers — non-owning access shortcuts.
// Application owns the actual instances via Iunique<System>.
// Each system sets its global in OnInitialize() and clears it in OnShutdown().
//
// Convention: direct namespace access (e.g., VKE::Renderer, VKE::Input).
//
// Globals are populated per phase:
//   Phase 2: Window, Input
//   Phase 3: Renderer
//   Phase 4: Physics
//   Phase 5: Scripting

namespace VKE {

    // Request the engine to stop at the end of the current frame.
    void RequestExit();

} // namespace VKE
