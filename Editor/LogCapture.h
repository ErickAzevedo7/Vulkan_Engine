#pragma once

#include <core/Types.h>

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace VKE::Editor {

    enum class LogLevel : u8 {
        Info,
        Warning,
        Error,
    };

    struct LogEntry {
        LogLevel Level = LogLevel::Info;
        std::chrono::system_clock::time_point Time;
        // The part before ": " when the message starts with a name ("AssetCatalog"), or the
        // logger's name ("Engine").
        std::string Category;
        std::string Message;
    };

    // Keeps the engine's log messages for the Console. Install() adds it to the engine's
    // loggers; from then on every message is also queued here, from any thread, until taken.
    class LogCapture {
    public:
        static void Install();

        // Moves the messages logged since the last call to the end of `out`.
        static void Take(std::vector<LogEntry>& out);

    private:
        static void Add(LogEntry entry);

        static std::mutex s_Mutex;
        static std::vector<LogEntry> s_Pending;

        friend class LogCaptureSink;
    };

} // namespace VKE::Editor
