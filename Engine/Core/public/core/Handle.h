#pragma once

#include <core/Types.h>
#include <functional>

namespace VKE {

    // Reference to an item stored in a SlotMap. Once the item is removed the
    // handle stays invalid, even if its slot is reused.
    template<typename T>
    struct Handle {
        static constexpr u32 kInvalidIndex = ~u32(0);

        u32 Index = kInvalidIndex;
        u32 Generation = 0;

        bool IsValid() const { return Index != kInvalidIndex; }

        bool operator==(const Handle& other) const { return Index == other.Index && Generation == other.Generation; }
        bool operator!=(const Handle& other) const { return !(*this == other); }
    };

} // namespace VKE

namespace std {

    template<typename T>
    struct hash<VKE::Handle<T>> {
        size_t operator()(const VKE::Handle<T>& handle) const {
            return hash<VKE::u64>()((static_cast<VKE::u64>(handle.Index) << 32) | handle.Generation);
        }
    };

} // namespace std
