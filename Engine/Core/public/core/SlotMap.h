#pragma once

#include <core/Handle.h>
#include <core/Types.h>
#include <utility>
#include <vector>

namespace VKE {

    // Stores values in a contiguous array and hands out stable handles to them.
    // Removing swaps the last value into the freed position, so iteration order changes.
    template<typename T>
    class SlotMap {
    public:
        using HandleType = Handle<T>;

        template<typename... Args>
        HandleType Emplace(Args&&... args) {
            u32 slotIndex;
            if (!m_FreeSlots.empty()) {
                slotIndex = m_FreeSlots.back();
                m_FreeSlots.pop_back();
            } else {
                slotIndex = static_cast<u32>(m_Slots.size());
                m_Slots.emplace_back();
            }

            Slot& slot = m_Slots[slotIndex];
            slot.DenseIndex = static_cast<u32>(m_Values.size());
            m_Values.emplace_back(std::forward<Args>(args)...);
            m_DenseToSlot.push_back(slotIndex);
            return {slotIndex, slot.Generation};
        }

        bool Remove(HandleType handle) {
            if (!Contains(handle)) return false;

            Slot& slot = m_Slots[handle.Index];
            u32 denseIndex = slot.DenseIndex;
            u32 lastIndex = static_cast<u32>(m_Values.size() - 1);
            if (denseIndex != lastIndex) {
                m_Values[denseIndex] = std::move(m_Values[lastIndex]);
                m_DenseToSlot[denseIndex] = m_DenseToSlot[lastIndex];
                m_Slots[m_DenseToSlot[denseIndex]].DenseIndex = denseIndex;
            }
            m_Values.pop_back();
            m_DenseToSlot.pop_back();

            slot.DenseIndex = kInvalidIndex;
            ++slot.Generation;
            m_FreeSlots.push_back(handle.Index);
            return true;
        }

        bool Contains(HandleType handle) const {
            return handle.Index < m_Slots.size() && m_Slots[handle.Index].Generation == handle.Generation &&
                   m_Slots[handle.Index].DenseIndex != kInvalidIndex;
        }

        T* Get(HandleType handle) { return Contains(handle) ? &m_Values[m_Slots[handle.Index].DenseIndex] : nullptr; }
        const T* Get(HandleType handle) const {
            return Contains(handle) ? &m_Values[m_Slots[handle.Index].DenseIndex] : nullptr;
        }

        size_t Size() const { return m_Values.size(); }
        bool Empty() const { return m_Values.empty(); }

        const std::vector<T>& Values() const { return m_Values; }

        // Handle of the value at `denseIndex` in Values().
        HandleType HandleAt(size_t denseIndex) const {
            u32 slotIndex = m_DenseToSlot[denseIndex];
            return {slotIndex, m_Slots[slotIndex].Generation};
        }

    private:
        static constexpr u32 kInvalidIndex = ~u32(0);

        struct Slot {
            u32 DenseIndex = kInvalidIndex;
            u32 Generation = 0;
        };

        std::vector<T> m_Values;
        std::vector<u32> m_DenseToSlot;
        std::vector<Slot> m_Slots;
        std::vector<u32> m_FreeSlots;
    };

} // namespace VKE
