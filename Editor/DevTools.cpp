#include "DevTools.h"

#include <core/Log.h>
#include <platform/Process.h>

#include <cstdlib>
#include <sstream>

namespace VKE::Editor {

    std::filesystem::path FindVisualStudio() {
        const char* programs = std::getenv("ProgramFiles(x86)");
        if (!programs) return {};

        // Comes with every Visual Studio, at this fixed place.
        std::error_code ec;
        std::filesystem::path locator =
            std::filesystem::path(programs) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
        if (!std::filesystem::exists(locator, ec)) return {};

        Platform::Process::Child::Desc desc;
        desc.Executable = locator;
        desc.Arguments = {"-latest",   "-prerelease", "-products", "*", "-utf8", "-property", "installationPath",
                          "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"};
        Iunique<Platform::Process::Child> child = Platform::Process::Child::Start(desc);
        if (!child) return {};

        std::string output;
        while (child->Read(output)) {
        }
        size_t end = output.find_last_not_of("\r\n ");
        if (child->GetExitCode() != 0 || end == std::string::npos) return {};
        return std::filesystem::u8path(output.substr(0, end + 1));
    }

    std::filesystem::path FindOnPath(const std::string& pathVariable, const std::string& program) {
        std::istringstream folders(pathVariable);
        std::string folder;
        while (std::getline(folders, folder, ';')) {
            if (folder.empty()) continue;
            std::error_code ec;
            std::filesystem::path file = std::filesystem::u8path(folder) / program;
            if (std::filesystem::exists(file, ec)) return file;
        }
        return {};
    }

    void OpenInCodeEditor(const std::filesystem::path& file, u32 line) {
        std::string lineText = std::to_string(line);
        const char* path = std::getenv("PATH");
        // VS Code puts its bin folder on the PATH, with the program one folder up.
        std::filesystem::path code = FindOnPath(path ? path : "", "code.cmd");
        if (!code.empty()) {
            std::filesystem::path program = code.parent_path().parent_path() / "Code.exe";
            Platform::Process::Launch(program, {"--goto", file.u8string() + ":" + lineText});
            return;
        }

        std::filesystem::path studio = FindVisualStudio();
        if (studio.empty()) {
            VKE_CORE_WARN("Editor: there's no code editor to open '{}'; VS Code or Visual Studio can", file.u8string());
            return;
        }
        std::filesystem::path program = studio / "Common7" / "IDE" / "devenv.exe";
        Platform::Process::Launch(program, {"/edit", file.u8string(), "/command", "Edit.GoTo " + lineText});
    }

} // namespace VKE::Editor
