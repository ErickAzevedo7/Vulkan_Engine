#include "platform/Process.h"

#include <core/Log.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace VKE::Platform::Process {

    namespace {
        std::wstring ToWide(const std::string& text) {
            if (text.empty()) return {};
            int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring wide(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
            return wide;
        }

        std::string ToUtf8(const wchar_t* text) {
            int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 1) return {};
            std::string utf8(static_cast<size_t>(length - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), length, nullptr, nullptr);
            return utf8;
        }

        // Quotes an argument so the program's C runtime splits it back out unchanged: backslashes
        // only escape when a quote follows them.
        void AppendQuoted(std::wstring& commandLine, const std::wstring& argument) {
            if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) {
                commandLine += argument;
                return;
            }
            commandLine += L'"';
            size_t backslashes = 0;
            for (wchar_t c : argument) {
                if (c == L'\\') {
                    ++backslashes;
                    continue;
                }
                if (c == L'"') {
                    commandLine.append(backslashes * 2 + 1, L'\\');
                } else {
                    commandLine.append(backslashes, L'\\');
                }
                backslashes = 0;
                commandLine += c;
            }
            commandLine.append(backslashes * 2, L'\\');
            commandLine += L'"';
        }
    } // namespace

    std::vector<std::string> GetCommandLineArguments() {
        int count = 0;
        LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        std::vector<std::string> result;
        for (int i = 1; arguments && i < count; ++i) {
            result.push_back(ToUtf8(arguments[i]));
        }
        LocalFree(arguments);
        return result;
    }

    bool Launch(const std::filesystem::path& executable, const std::vector<std::string>& arguments) {
        std::wstring commandLine;
        AppendQuoted(commandLine, executable.wstring());
        for (const std::string& argument : arguments) {
            commandLine += L' ';
            AppendQuoted(commandLine, ToWide(argument));
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::wstring folder = executable.parent_path().wstring();
        if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            folder.empty() ? nullptr : folder.c_str(), &startup, &process)) {
            VKE_CORE_ERROR("Process::Launch: couldn't start '{}' (error {})", executable.u8string(), GetLastError());
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }

} // namespace VKE::Platform::Process
