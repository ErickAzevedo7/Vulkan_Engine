#pragma once

#include "LogCapture.h"

#include <core/Types.h>
#include <platform/Process.h>
#include <project/Project.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace VKE::Editor {

    // Keeps the game's DLL up to date with the project's code. A thread watches the scripts folder
    // and builds it when something changes, with CMake, Ninja and Visual Studio's compiler, so the
    // editor never waits. Its first look builds what changed while the editor was closed. A build
    // that fails leaves the last DLL as it was.
    class ScriptCompiler {
    public:
        enum class State {
            // There's nothing to build yet.
            Idle,
            Building,
            Built,
            Failed,
        };

        struct Message {
            LogLevel Level = LogLevel::Error;
            std::string Text;
        };

        struct Result {
            bool Succeeded = false;
            // There's a new DLL to load.
            bool LibraryChanged = false;
            f32 Seconds = 0.0f;
            // The compiler's, with paths relative to the scripts folder when they are inside it.
            std::vector<Message> Messages;
        };

        // `sdkFolder` is the SDK next to the editor.
        ScriptCompiler(const Project::Project& project, std::filesystem::path sdkFolder);
        ~ScriptCompiler();

        ScriptCompiler(const ScriptCompiler&) = delete;
        ScriptCompiler& operator=(const ScriptCompiler&) = delete;

        void Start();
        // Ends the thread, and the build if there's one.
        void Stop();

        State GetState() const { return m_State; }
        // What the last build gave, once; nothing if there was no build since the last call.
        std::optional<Result> TakeResult();

    private:
        // Every file that matters, with when it was written and its size.
        using Files = std::map<std::filesystem::path, std::pair<std::filesystem::file_time_type, uintmax_t>>;

        void Watch();
        // Waits a moment. False if the thread should end.
        bool Tick();
        Files ScanFiles() const;
        void Build(const Files& files);
        bool FindTools(Result& result);
        bool Configure(const Files& files, Result& result);
        // Runs a program to its end, adding what it prints to `output`. False if it failed.
        bool Run(const Platform::Process::Child::Desc& desc, std::string& output);
        // Turns what the tools printed into messages. When a build fails and none of it
        // reads as an error, its last lines are all there is to show.
        void ReadOutput(const std::string& output, bool failed, Result& result) const;
        // `text` with the scripts folder taken off the start of the path, if it's there.
        std::string ShortenPath(const std::string& text) const;

        std::filesystem::path m_ScriptsFolder;
        std::filesystem::path m_BuildFolder;
        std::filesystem::path m_Library;
        std::filesystem::path m_SdkFolder;

        // Found once, by the first build: the tools' environment takes seconds to get.
        std::vector<std::string> m_Environment;
        std::filesystem::path m_Studio;
        std::filesystem::path m_Cmake;
        std::filesystem::path m_Ninja;

        std::thread m_Thread;
        std::atomic<State> m_State{State::Idle};
        std::mutex m_Mutex;
        std::condition_variable m_Wake;
        bool m_Stopping = false;
        Platform::Process::Child* m_Running = nullptr;
        std::optional<Result> m_Result;
    };

} // namespace VKE::Editor
