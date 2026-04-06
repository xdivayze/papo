#pragma once
#include <unordered_map>
#include <cstdint>
namespace PapoEvent
{

    // a type that maps to one or more events in the event bus
    typedef uint32_t PapoEventTypeID;

    constexpr const PapoEventTypeID PapoEventQueueEnd = UINT32_MAX;

    struct PapoEventHeader
    {
        PapoEventTypeID type_;
        size_t headerSize_;
        size_t payloadSize;
    };

    struct PapoEventGeneric
    {
        PapoEventHeader *header;
        void *payload;
    };

    class IPapoEventListener
    {
    public:
        virtual void eventCall(void *payload);
    };

    class EventBus
    {
    public:
        void publish(PapoEventGeneric *evt);

    private:
        std::unordered_multimap<PapoEventTypeID, IPapoEventListener &> listeners;
    };

}
