#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace VKE {

    class Log {
    public:
        static void Init();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

} // namespace VKE

// Core log macros
#define VKE_CORE_TRACE(...)    ::VKE::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define VKE_CORE_INFO(...)     ::VKE::Log::GetCoreLogger()->info(__VA_ARGS__)
#define VKE_CORE_WARN(...)     ::VKE::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define VKE_CORE_ERROR(...)    ::VKE::Log::GetCoreLogger()->error(__VA_ARGS__)
#define VKE_CORE_CRITICAL(...) ::VKE::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define VKE_TRACE(...)         ::VKE::Log::GetClientLogger()->trace(__VA_ARGS__)
#define VKE_INFO(...)          ::VKE::Log::GetClientLogger()->info(__VA_ARGS__)
#define VKE_WARN(...)          ::VKE::Log::GetClientLogger()->warn(__VA_ARGS__)
#define VKE_ERROR(...)         ::VKE::Log::GetClientLogger()->error(__VA_ARGS__)
#define VKE_CRITICAL(...)      ::VKE::Log::GetClientLogger()->critical(__VA_ARGS__)
