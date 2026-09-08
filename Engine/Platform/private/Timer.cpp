#include "platform/Timer.h"

namespace VKE::Platform {

    Timer::Timer() {
        Reset();
    }

    void Timer::Reset() {
        m_Start = std::chrono::steady_clock::now();
    }

    f64 Timer::GetElapsedSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<f64>(now - m_Start).count();
    }

} // namespace VKE::Platform
