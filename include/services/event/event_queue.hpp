#pragma once
#include "../memory/arena_allocator.hpp"
#include "papo_event.hpp"
#include <cstring>
namespace PapoEvent
{

    /*
        queue that uses double buffered arena allocator to store events for the next frame
        it is guaranteed that the next element in the queue is aligned to PapoEventHeader
    */
    class EventQueue
    {
    public:
        explicit EventQueue(size_t capacity = 4096) : arena_(capacity) {}


        //due to this it is guaranteed that the next element will always be aligned to the header
        template <typename T>
        T *push(PapoEventTypeID evtID)
        {
            new (arena_.allocate(sizeof(PapoEventHeader), alignof(PapoEventHeader)))
                PapoEventHeader{evtID, sizeof(PapoEventHeader), sizeof(T)};

            return new (arena_.allocate(sizeof(T), alignof(T))) T;
        }

        template <typename T>
        void push(PapoEventTypeID evtID, const T &evt)
        {
            static_assert(std::is_trivially_copyable_v<T>, "event types must be trivially copyable");
            new (arena_.allocate(sizeof(PapoEventHeader), alignof(PapoEventHeader)))
                PapoEventHeader{evtID, sizeof(PapoEventHeader), sizeof(T)};

            std::memcpy(arena_.allocate(sizeof(T), alignof(T)), &evt, sizeof(T));
        }

                std::byte *getReadHandle()
        {
            return arena_.readBuffer();
        }

        void endFrame()
        {
            push(PapoEvent::PapoEventQueueEnd);
            arena_.swapBuffers();
        }

    private:
        Memory::DoubleBufferedArena arena_;

        void push(PapoEventTypeID evtID)
        {
            new (arena_.allocate(sizeof(PapoEventHeader), alignof(PapoEventHeader)))
                PapoEventHeader{evtID, sizeof(PapoEventHeader), 0};
        }
    };
}