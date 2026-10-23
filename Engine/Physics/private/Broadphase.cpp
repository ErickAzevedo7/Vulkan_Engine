#include "Broadphase.h"

#include <algorithm>

namespace VKE::Physics {

    const std::vector<std::pair<u32, u32>>& Broadphase::FindPairs(std::vector<Entry>& entries) {
        m_Pairs.clear();

        // Sorted from scratch each step. Bodies move little between steps, so keeping the order
        // and re-sorting with insertion sort would be cheaper if the body count grows.
        std::sort(entries.begin(), entries.end(),
                  [](const Entry& a, const Entry& b) { return a.Bounds.Min.x < b.Bounds.Min.x; });

        for (size_t i = 0; i < entries.size(); ++i) {
            const Entry& a = entries[i];
            // Every later entry starts at or after a's start; past a's end, none can overlap it.
            for (size_t j = i + 1; j < entries.size() && entries[j].Bounds.Min.x <= a.Bounds.Max.x; ++j) {
                const Entry& b = entries[j];
                if (a.Static && b.Static) continue;
                if (a.Bounds.Intersects(b.Bounds)) m_Pairs.emplace_back(a.Body, b.Body);
            }
        }
        return m_Pairs;
    }

} // namespace VKE::Physics
