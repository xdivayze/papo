#pragma once
#include "../utils/singleton.hpp"
#include "event/papo_event_manager.hpp"
#include "logger.hpp"
#include "memory/memory_manager.hpp"
#include "time_manager.hpp"
namespace Engine
{
    //TODO the root has too many circular dependencies rn
    class Root : public Singleton<Root>
    {
    public:
        Service::Logger *logger_;
        Service::MemoryManager *memoryManager_;

        constexpr PapoEvent::PapoEventManager &getEventManager()
        {
            return eventManager_;
        }

        // Lazily constructs the TimeManager on first call (function-local
        // static). Done lazily because TimeManager's ctor calls Root::get(),
        // and constructing it inside Root::Root would re-enter the
        // Singleton init (caught at runtime as recursive_init_error).
        Service::TimeManager &getTimeManager();

        friend class Singleton<Root>;

    private:
        PapoEvent::PapoEventManager eventManager_;

        Root();
        ~Root();
    };
}