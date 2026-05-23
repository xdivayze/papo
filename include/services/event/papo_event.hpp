#pragma once
#include <unordered_map>
#include <cstdint>
namespace PapoEvent
{

    // a type that maps to one or more events in the event bus
    typedef uint32_t PapoEventTypeID;

    constexpr const PapoEventTypeID PapoEventFrameStartID = 5;
    constexpr const PapoEventTypeID PapoEventFrameEndID = 6;

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
        IPapoEventListener() = default;
        virtual ~IPapoEventListener() = default;

        // The bus stores listeners by reference; copying or moving a
        // listener would silently invalidate the bus's stored reference.
        IPapoEventListener(const IPapoEventListener &) = delete;
        IPapoEventListener &operator=(const IPapoEventListener &) = delete;
        IPapoEventListener(IPapoEventListener &&) = delete;
        IPapoEventListener &operator=(IPapoEventListener &&) = delete;

        virtual void eventCall(void *payload) = 0;
    };

    class EventBus
    {
    public:
        void publish(PapoEventGeneric *evt);

        void subscribe(PapoEventTypeID evtID, IPapoEventListener &listener);

        void unsubscribe(PapoEventTypeID evtID, IPapoEventListener &listener);

    private:
        std::unordered_multimap<PapoEventTypeID, IPapoEventListener &> listeners;
    };

}
