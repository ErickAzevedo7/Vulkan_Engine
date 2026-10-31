#include <core/Log.h>

#include <vke/detail/ApiBindings.h>

#include <string_view>

namespace vke::api {

    void Log_Info(const char* message) {
        VKE_CORE_INFO("{}", std::string_view(message ? message : ""));
    }

    void Log_Warning(const char* message) {
        VKE_CORE_WARN("{}", std::string_view(message ? message : ""));
    }

    void Log_Error(const char* message) {
        VKE_CORE_ERROR("{}", std::string_view(message ? message : ""));
    }

} // namespace vke::api
