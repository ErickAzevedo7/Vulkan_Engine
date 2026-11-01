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

    Iunique<Child> Child::Start(const Desc& desc) {
        VKE_CORE_WARN("Process::Child: can't start '{}' on this system", desc.Executable.u8string());
        return nullptr;
    }

    Child::~Child() = default;

    bool Child::Read(std::string&) {
        return false;
    }

    int Child::GetExitCode() const {
        return -1;
    }

    void Child::Stop() {
    }

    std::string ConsoleTextToUtf8(const std::string& text) {
        return text;
    }

} // namespace VKE::Platform::Process
