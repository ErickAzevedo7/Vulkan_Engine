#pragma once

#include <string>

namespace VKE::Platform {

    // Runs `call(data)` and catches what would bring the program down: a bad pointer, a whole
    // number divided by zero, a stack overflow, a C++ exception nobody caught. False if one
    // happened, with what and where in `outFault`.
    //
    // It doesn't undo anything: after a fault the program's memory may be damaged. It can't catch
    // a call that never returns, one that ends the process, or memory overwritten without a fault.
    bool GuardedCall(void (*call)(void* data), void* data, std::string& outFault);

} // namespace VKE::Platform
