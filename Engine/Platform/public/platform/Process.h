#pragma once

#include <core/Types.h>

#include <filesystem>
#include <string>
#include <vector>

// The running program's arguments, and starting other programs.

namespace VKE::Platform::Process {

    // The arguments the program was started with, without the program itself, in UTF-8.
    std::vector<std::string> GetCommandLineArguments();

    // Starts `executable` with `arguments` (UTF-8) and returns without waiting for it.
    // Returns false, logging why, if it couldn't be started.
    bool Launch(const std::filesystem::path& executable, const std::vector<std::string>& arguments);

    // A program this one started, whose output it reads. It runs with no window, and it ends with
    // this object, along with everything it started.
    class Child {
    public:
        struct Desc {
            std::filesystem::path Executable;
            // In UTF-8.
            std::vector<std::string> Arguments;
            // Where it runs; empty for where this program runs.
            std::filesystem::path Folder;
            // "NAME=value" in UTF-8; empty for this program's.
            std::vector<std::string> Environment;
        };

        // Null, logged, if it can't be started.
        static Iunique<Child> Start(const Desc& desc);

        ~Child();

        Child(const Child&) = delete;
        Child& operator=(const Child&) = delete;

        // Waits for what the program prints, to standard output or error, and adds it to `out`
        // as it came. False once the program ended and nothing is left to read.
        bool Read(std::string& out);
        // What the program returned, once Read gave false.
        int GetExitCode() const;
        // Ends the program and what it started, from any thread.
        void Stop();

    private:
        Child(void* process, void* job, void* output) : m_Process(process), m_Job(job), m_Output(output) {}

        void* m_Process;
        void* m_Job;
        void* m_Output;
    };

    // What a console program printed, in UTF-8: it comes in UTF-8 or in the system's console
    // encoding, depending on the program.
    std::string ConsoleTextToUtf8(const std::string& text);

} // namespace VKE::Platform::Process
