#pragma once

#include <core/Log.h>

#ifdef _MSC_VER
    #define VKE_DEBUGBREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
    #define VKE_DEBUGBREAK() __builtin_trap()
#else
    #define VKE_DEBUGBREAK()
#endif

#ifdef VKE_ENABLE_ASSERTS

    // Alteratively we could use C++20 std::source_location, but __FILE__ and __LINE__ work well for C++17
    #define VKE_ASSERT(x, ...) { if(!(x)) { VKE_ERROR("Assertion Failed: {0}", __VA_ARGS__); VKE_DEBUGBREAK(); } }
    #define VKE_CORE_ASSERT(x, ...) { if(!(x)) { VKE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); VKE_DEBUGBREAK(); } }

#else

    #define VKE_ASSERT(x, ...)
    #define VKE_CORE_ASSERT(x, ...)

#endif
