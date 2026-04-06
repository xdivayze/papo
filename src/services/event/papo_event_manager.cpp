#include "services/event/papo_event_manager.hpp"

namespace PapoEvent
{
    PapoEventManager::PapoEventManager() = default;
    PapoEventManager::~PapoEventManager() = default;

    /*
        publish consumes the generic event struct and calls the listener function from the listeners map
    */
    void EventBus::publish(PapoEventGeneric *evt)
    {
        auto [begin, end] = listeners.equal_range(evt->header->type_);
        for (auto it = begin; it != end; ++it)
        {
            it->second.eventCall(evt->payload);
        }
    }

    void EventBus::subscribe(PapoEventTypeID evtID, IPapoEventListener &listener)
    {
        listeners.emplace(evtID, listener);
    }

    void EventBus::unsubscribe(PapoEventTypeID evtID, IPapoEventListener &listener)
    {
        auto [begin, end] = listeners.equal_range(evtID);
        for (auto it = begin; it != end;)
        {
            if (&it->second == &listener)
                it = listeners.erase(it);
            else
                ++it;
        }
    }

    void PapoEventManager::publishAll()
    {

        auto cursor = queue_.getReadHandle();
        while (1)
        {
            auto *header = reinterpret_cast<PapoEventHeader *>(cursor);
            cursor += header->headerSize_;

            if (header->type_ == PapoEvent::PapoEventQueueEnd)
                break;

            void *payload = cursor;

            auto evtGeneric = PapoEventGeneric{
                header,
                payload,
            };

            bus_.publish(&evtGeneric); // papo event generic consumed immediately

            cursor += header->payloadSize;
            constexpr uintptr_t hdrAlign = alignof(PapoEventHeader);
            cursor = reinterpret_cast<std::byte *>(
                (reinterpret_cast<uintptr_t>(cursor) + hdrAlign - 1) & ~(hdrAlign - 1));
        }
    }
}