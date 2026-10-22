#pragma once

#include <core/Types.h>
#include <functional>

namespace VKE {

    // 64-bit identifier that stays the same across runs, used to refer to objects in saved files.
    // A default-constructed ID is invalid; Generate makes a new one.
    class ObjectID {
    public:
        ObjectID() = default;
        explicit ObjectID(u64 id) : m_ID(id) {}

        // A new random ID, never zero, drawn from the operating system's random source so
        // separate runs don't repeat each other's IDs.
        static ObjectID Generate();
        static ObjectID Invalid() { return ObjectID(); }

        bool operator==(const ObjectID& other) const { return m_ID == other.m_ID; }
        bool operator!=(const ObjectID& other) const { return m_ID != other.m_ID; }

        operator u64() const { return m_ID; }

        bool IsValid() const { return m_ID != 0; }

    private:
        u64 m_ID = 0;
    };

} // namespace VKE

namespace std {
    template<>
    struct hash<VKE::ObjectID> {
        std::size_t operator()(const VKE::ObjectID& id) const {
            return static_cast<std::size_t>(static_cast<VKE::u64>(id));
        }
    };
}
