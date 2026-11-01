#include "platform/GuardedCall.h"

// Systems without a backend yet: nothing is caught. Windows uses
// private/windows/WindowsGuardedCall.cpp.

namespace VKE::Platform {

    bool GuardedCall(void (*call)(void* data), void* data, std::string&) {
        call(data);
        return true;
    }

} // namespace VKE::Platform
