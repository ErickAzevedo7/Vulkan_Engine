#pragma once

#include <core/Types.h>
#include <chrono>

namespace VKE::Platform {

    // General-purpose stopwatch backed by std::chrono::steady_clock.
    // Independent per instance: elapsed time is measured from this
    // object's own construction/Reset(), not from application start.
    class Timer {
    public:
        Timer();

        void Reset();
        f64 GetElapsedSeconds() const;

    private:
        std::chrono::steady_clock::time_point m_Start;
    };

} // namespace VKE::Platform
