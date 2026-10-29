#include "LogCapture.h"

#include <core/Log.h>

#include <spdlog/sinks/base_sink.h>

#include <cctype>

namespace VKE::Editor {

    std::mutex LogCapture::s_Mutex;
    std::vector<LogEntry> LogCapture::s_Pending;

    namespace {
        // The engine's messages start with who wrote them: "AssetCatalog: 916 assets ...".
        bool SplitCategory(const std::string& text, std::string& outCategory, std::string& outMessage) {
            size_t colon = text.find(": ");
            if (colon == std::string::npos || colon == 0 || colon > 48) return false;
            for (size_t i = 0; i < colon; ++i) {
                unsigned char c = static_cast<unsigned char>(text[i]);
                if (!std::isalnum(c) && c != '_' && c != ':') return false;
            }
            outCategory = text.substr(0, colon);
            outMessage = text.substr(colon + 2);
            return true;
        }

        // "ENGINE" reads as "Engine".
        std::string FormatLoggerName(const std::string& name) {
            std::string formatted = name;
            for (size_t i = 1; i < formatted.size(); ++i) {
                formatted[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(formatted[i])));
            }
            return formatted;
        }
    } // namespace

    class LogCaptureSink : public spdlog::sinks::base_sink<std::mutex> {
    protected:
        void sink_it_(const spdlog::details::log_msg& message) override {
            LogEntry entry;
            switch (message.level) {
            case spdlog::level::warn: entry.Level = LogLevel::Warning; break;
            case spdlog::level::err:
            case spdlog::level::critical: entry.Level = LogLevel::Error; break;
            default: entry.Level = LogLevel::Info; break;
            }
            entry.Time = message.time;

            std::string text(message.payload.data(), message.payload.size());
            if (!SplitCategory(text, entry.Category, entry.Message)) {
                entry.Category = FormatLoggerName(std::string(message.logger_name.data(), message.logger_name.size()));
                entry.Message = std::move(text);
            }
            LogCapture::Add(std::move(entry));
        }

        void flush_() override {}
    };

    void LogCapture::Install() {
        auto sink = std::make_shared<LogCaptureSink>();
        for (auto* logger : {&Log::GetCoreLogger(), &Log::GetClientLogger()}) {
            if (*logger) (*logger)->sinks().push_back(sink);
        }
    }

    void LogCapture::Take(std::vector<LogEntry>& out) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        out.insert(out.end(), std::make_move_iterator(s_Pending.begin()), std::make_move_iterator(s_Pending.end()));
        s_Pending.clear();
    }

    void LogCapture::Add(LogEntry entry) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Pending.push_back(std::move(entry));
    }

} // namespace VKE::Editor
