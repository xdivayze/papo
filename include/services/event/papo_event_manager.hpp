#pragma once
#include <queue>
#include "papo_event.hpp"
#include "event_queue.hpp"

namespace Engine
{
    class Root;
}

class PapoEventManagerTest;

namespace PapoEvent
{
    class PapoEventManager
    {
    public:
        friend class Engine::Root;
        friend class ::PapoEventManagerTest;

        // returns a write handle to write the payload to the event queue
        template <typename T>
        T *getEventWriteHandle(PapoEventTypeID evtID)
        {
            return queue_.push<T>(evtID);
        }
        template <typename T>
        void pushEvent(PapoEventTypeID evtID, const T &payload)
        {
            return queue_.push<T>(evtID, payload);
        }

        // publishes all events in the queue and notifies all listeners in the bus listening for that event type
        void publishAll();

    private:
        PapoEventManager();
        ~PapoEventManager();

        EventBus bus_;

        // events to be published in the next frame, reset at the beginning of each frame
        EventQueue queue_;
    };
}