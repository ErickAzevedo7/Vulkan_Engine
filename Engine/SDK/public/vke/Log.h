#pragma once

#include <vke/Marks.h>

namespace vke::Log {

    // Writes a line in the Console:
    //
    //     vke::Log::Info("The door is open");
    VKE_API void Info(const char* message);
    // The same, marked as something to look at.
    VKE_API void Warning(const char* message);
    // The same, marked as something that went wrong.
    VKE_API void Error(const char* message);

} // namespace vke::Log
