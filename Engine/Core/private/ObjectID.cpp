#include <core/ObjectID.h>

#include <random>

namespace VKE {

    ObjectID ObjectID::Generate() {
        // Each call reads the system's random source, unlike a seeded generator, whose whole
        // sequence would repeat in any run that drew the same seed.
        thread_local std::random_device device;

        u64 id;
        do {
            id = (static_cast<u64>(device()) << 32) | static_cast<u64>(device());
        } while (id == 0);
        return ObjectID(id);
    }

} // namespace VKE
