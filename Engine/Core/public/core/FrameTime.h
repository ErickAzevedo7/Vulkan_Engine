#pragma once

#include <core/Types.h>

namespace VKE {

    // Immutable time snapshot passed to every update call.
    // delta = time since last frame, total = time since application start.
    // Uses double to avoid floating-point drift in long sessions.
    class FrameTime {
    public:
        explicit FrameTime(double delta, double total)
            : m_Delta(delta), m_Total(total) {}

        // Delta time (time since last frame)
        double GetDelta()   const { return m_Delta; }
        double GetDeltaMs() const { return m_Delta * 1000.0; }
        float  GetDeltaF()  const { return static_cast<float>(m_Delta); }

        // Absolute time since application start
        double GetTotal()   const { return m_Total; }
        float  GetTotalF()  const { return static_cast<float>(m_Total); }

    private:
        double m_Delta;
        double m_Total;
    };

} // namespace VKE
