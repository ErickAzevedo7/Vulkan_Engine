#include "platform/Process.h"

#include <core/Log.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <algorithm>

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

    Iunique<Child> Child::Start(const Desc& desc) {
        SECURITY_ATTRIBUTES inherited{sizeof(inherited), nullptr, TRUE};
        HANDLE readEnd = nullptr;
        HANDLE writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &inherited, 0)) {
            VKE_CORE_ERROR("Process::Child: couldn't make a pipe (error {})", GetLastError());
            return nullptr;
        }
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

        // Closing the job ends everything in it, even if this program dies first.
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

        std::wstring commandLine;
        AppendQuoted(commandLine, desc.Executable.wstring());
        for (const std::string& argument : desc.Arguments) {
            commandLine += L' ';
            AppendQuoted(commandLine, ToWide(argument));
        }

        // Each variable ends in a zero, and an empty one ends the block.
        std::wstring environment;
        for (const std::string& variable : desc.Environment) {
            environment += ToWide(variable);
            environment += L'\0';
        }
        environment += L'\0';

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writeEnd;
        startup.hStdError = writeEnd;
        PROCESS_INFORMATION process{};
        // Suspended until it's in the job, so nothing it starts escapes.
        BOOL started = CreateProcessW(desc.Executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                                      desc.Environment.empty() ? nullptr : environment.data(),
                                      desc.Folder.empty() ? nullptr : desc.Folder.c_str(), &startup, &process);
        DWORD error = GetLastError();
        CloseHandle(writeEnd);
        if (!started) {
            VKE_CORE_ERROR("Process::Child: couldn't start '{}' (error {})", desc.Executable.u8string(), error);
            CloseHandle(readEnd);
            CloseHandle(job);
            return nullptr;
        }
        AssignProcessToJobObject(job, process.hProcess);
        ResumeThread(process.hThread);
        CloseHandle(process.hThread);
        return Iunique<Child>(new Child(process.hProcess, job, readEnd));
    }

    Child::~Child() {
        CloseHandle(static_cast<HANDLE>(m_Job));
        CloseHandle(static_cast<HANDLE>(m_Process));
        CloseHandle(static_cast<HANDLE>(m_Output));
    }

    bool Child::Read(std::string& out) {
        HANDLE process = static_cast<HANDLE>(m_Process);
        HANDLE output = static_cast<HANDLE>(m_Output);
        char buffer[4096];
        while (true) {
            // Looks first and waits on the program, not on the pipe: something the program left
            // running, like the compiler's symbol server, can keep the pipe open long after it ended.
            bool ended = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
            DWORD available = 0;
            bool open = PeekNamedPipe(output, nullptr, 0, nullptr, &available, nullptr);
            if (open && available > 0) {
                DWORD read = 0;
                DWORD wanted = std::min<DWORD>(available, sizeof(buffer));
                if (!ReadFile(output, buffer, wanted, &read, nullptr)) return false;
                out.append(buffer, read);
                return true;
            }
            if (ended || !open) return false;
            WaitForSingleObject(process, 15);
        }
    }

    int Child::GetExitCode() const {
        DWORD code = 0;
        GetExitCodeProcess(static_cast<HANDLE>(m_Process), &code);
        return static_cast<int>(code);
    }

    void Child::Stop() {
        TerminateJobObject(static_cast<HANDLE>(m_Job), 1);
    }

    std::string ConsoleTextToUtf8(const std::string& text) {
        if (text.empty()) return text;
        int size = static_cast<int>(text.size());
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0) > 0) return text;

        int length = MultiByteToWideChar(CP_OEMCP, 0, text.data(), size, nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_OEMCP, 0, text.data(), size, wide.data(), length);
        return ToUtf8(wide.c_str());
    }

} // namespace VKE::Platform::Process
