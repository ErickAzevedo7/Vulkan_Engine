#include "platform/Process.h"

#include <core/Log.h>

// Systems without a backend yet. Windows uses private/windows/WindowsProcess.cpp.

namespace VKE::Platform::Process {

    std::vector<std::string> GetCommandLineArguments() {
        VKE_CORE_WARN("Process::GetCommandLineArguments: not available on this system");
        return {};
    }

    bool Launch(const std::filesystem::path& executable, const std::vector<std::string>&) {
        VKE_CORE_WARN("Process::Launch: can't start '{}' on this system", executable.u8string());
        return false;
    }

} // namespace VKE::Platform::Process
