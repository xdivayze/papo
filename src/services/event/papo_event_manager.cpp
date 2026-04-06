#include "services/event/papo_event_manager.hpp"

namespace PapoEvent
{
    /*
        publish consumes the generic event struct and calls the listener function from the listeners map
    */
    void EventBus::publish(PapoEventGeneric *evt)
    {
        auto [begin, end] = listeners.equal_range(evt->header->type_);
        for (auto it = begin; it != end; ++begin)
        {
            it->second.eventCall(evt->payload);
        }
    }

    void PapoEventManager::publishAll()
    {

        auto cursor = queue_.getReadHandle();
        while (1)
        {
            auto *header = reinterpret_cast<PapoEventHeader *>(cursor);
            cursor += header->headerSize_;
            void *payload = cursor;

            auto evtGeneric = PapoEventGeneric{
                header,
                payload,
            };

            bus_.publish(&evtGeneric); // papo event generic consumed immediately

            cursor += header->payloadSize;
        }
    }
}