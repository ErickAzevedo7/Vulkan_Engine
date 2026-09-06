#include <core/ObjectID.h>
#include <atomic>

namespace VKE {

    // Starts at 1 — 0 is reserved for ObjectID::Invalid()
    static std::atomic<u64> s_Counter{ 1 };

    ObjectID::ObjectID()
        : m_ID(s_Counter.fetch_add(1, std::memory_order_relaxed))
    {
    }

    ObjectID::ObjectID(u64 id)
        : m_ID(id)
    {
    }

} // namespace VKE
