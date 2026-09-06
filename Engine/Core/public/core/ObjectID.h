#pragma once

#include <core/Types.h>
#include <functional>

namespace VKE {

    // Unique 64-bit identifier for engine objects.
    class ObjectID {
    public:
        ObjectID();                             // generates a random non-zero ID
        explicit ObjectID(u64 id);
        ObjectID(const ObjectID&) = default;
        ObjectID& operator=(const ObjectID&) = default;

        bool operator==(const ObjectID& other) const { return m_ID == other.m_ID; }
        bool operator!=(const ObjectID& other) const { return m_ID != other.m_ID; }

        operator u64() const { return m_ID; }

        bool IsValid() const { return m_ID != 0; }
        static ObjectID Invalid() { return ObjectID(0); }

    private:
        u64 m_ID;
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
