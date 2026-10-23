#pragma once

#include <core/Types.h>
#include <math/AABB.h>

#include <utility>
#include <vector>

namespace VKE::Physics {

    // Finds the pairs of bodies whose bounds overlap by sorting them along X and sweeping,
    // so only bodies that share a stretch of X are compared.
    class Broadphase {
    public:
        struct Entry {
            AABB Bounds;
            // Passed back in the pairs.
            u32 Body = 0;
            // Pairs of two static entries are skipped: neither ever moves.
            bool Static = false;
        };

        // Pairs of Entry::Body values, each pair once.
        const std::vector<std::pair<u32, u32>>& FindPairs(std::vector<Entry>& entries);

    private:
        std::vector<std::pair<u32, u32>> m_Pairs;
    };

} // namespace VKE::Physics
