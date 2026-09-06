#pragma once

#include <functional>
#include <unordered_map>
#include <core/Types.h>

namespace VKE {

    // Usage:
    //   Event<u32, u32> OnResize;
    //   u64 id = OnResize.Subscribe([](u32 w, u32 h) { ... });
    //   OnResize.Dispatch(800, 600);
    //   OnResize.Unsubscribe(id);
    template <typename... Args>
    class Event {
    public:
        using SlotID = u64;
        using Slot = std::function<void(Args...)>;

        Event() = default;
        ~Event() = default;

        // Subscribe to this event. Returns a unique ID to unsubscribe later.
        SlotID Subscribe(Slot slot) {
            SlotID id = ++m_NextID;
            m_Slots[id] = std::move(slot);
            return id;
        }

        // Unsubscribe from this event using the ID returned by Subscribe().
        void Unsubscribe(SlotID id) {
            m_Slots.erase(id);
        }

        // Remove all subscriptions.
        void UnsubscribeAll() {
            m_Slots.clear();
        }

        // Dispatch the event to all subscribers.
        void Dispatch(Args... args) const {
            for (auto& [id, slot] : m_Slots) {
                if (slot) {
                    slot(args...);
                }
            }
        }

        // Returns the current number of active subscribers.
        size_t GetSubscriberCount() const {
            return m_Slots.size();
        }

    private:
        std::unordered_map<SlotID, Slot> m_Slots;
        SlotID m_NextID = 0;
    };

} // namespace VKE
