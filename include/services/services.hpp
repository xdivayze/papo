#pragma once
#include "../utils/singleton.hpp"
#include "logger.hpp"
#include "memory/memory_manager.hpp"
#include "event/papo_event_manager.hpp"
namespace Engine
{
    class Root : public Singleton<Root>
    {
    public:
        Service::Logger *logger_;
        Service::MemoryManager *memoryManager_;

        constexpr PapoEvent::PapoEventManager &getEventManager()
        {
            return eventManager_;
        }

        friend class Singleton<Root>;

    private:
        PapoEvent::PapoEventManager eventManager_;

        Root();
        ~Root();
    };
}