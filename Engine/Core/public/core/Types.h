#pragma once

#include <cstdint>
#include <memory>

namespace VKE {

    // Unsigned integer types
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    // Signed integer types
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    // Floating point types
    using f32 = float;
    using f64 = double;

    // Byte type
    using Byte = u8;

    // Resource ID for the server architecture (opaque handle)
    using RID = u64;

    // -------------------------------------------------------------------------
    // Smart pointer aliases
    // Abstraction boundary: swap the underlying type here if a custom allocator
    // is introduced later — call sites stay unchanged.
    // -------------------------------------------------------------------------

    // Exclusive ownership — only one owner at a time
    template<typename T>
    using Iunique = std::unique_ptr<T>;

    template<typename T, typename... Args>
    constexpr Iunique<T> MakeIunique(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Shared ownership — multiple owners, destroyed when last owner is gone
    template<typename T>
    using Ishared = std::shared_ptr<T>;

    template<typename T, typename... Args>
    constexpr Ishared<T> MakeIshared(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    // Non-owning reference — observes an Ishared<T> without extending its lifetime
    template<typename T>
    using Iweak = std::weak_ptr<T>;

} // namespace VKE
