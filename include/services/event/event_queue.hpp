#pragma once
#include "../memory/arena_allocator.hpp"
#include "papo_event.hpp"
#include <cstring>
namespace PapoEvent
{
    class EventQueue
    {
    public:
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

        std::byte* getReadHandle() {
            return arena_.readBuffer();
        }

        void endFrame()
        {
            arena_.swapBuffers();
        }

    private:
        Memory::DoubleBufferedArena arena_;
    };
}