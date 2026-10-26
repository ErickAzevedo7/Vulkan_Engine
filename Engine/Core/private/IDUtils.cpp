#include <core/IDUtils.h>

#include <cinttypes>
#include <cstdio>
#include <random>

namespace VKE {

    u64 GenerateID() {
        // Each call reads the system's random source, unlike a seeded generator, whose whole
        // sequence would repeat in any run that drew the same seed.
        thread_local std::random_device device;

        u64 id;
        do {
            id = (static_cast<u64>(device()) << 32) | static_cast<u64>(device());
        } while (id == 0);
        return id;
    }

    std::string FormatID(u64 id) {
        char text[17];
        std::snprintf(text, sizeof(text), "%016" PRIx64, id);
        return text;
    }

    u64 ParseID(const std::string& text) {
        if (text.size() != 16) return 0;

        u64 value = 0;
        for (char c : text) {
            int digit = (c >= '0' && c <= '9')   ? c - '0'
                        : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                        : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                 : -1;
            if (digit < 0) return 0;
            value = (value << 4) | static_cast<u64>(digit);
        }
        return value;
    }

} // namespace VKE
