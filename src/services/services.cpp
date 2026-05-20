#include "services/services.hpp"
#include "services/memory/memory_manager.hpp"
namespace Engine
{
    Root::Root()
    {
        logger_ = new Service::Logger();
        memoryManager_ = new Service::MemoryManager(nullptr);
    }

    Root::~Root()
    {
        delete logger_;
        delete memoryManager_;
    }

    Service::TimeManager &Root::getTimeManager()
    {
        // Function-local static: thread-safe lazy init, destroyed before
        // Root (reverse order of dynamic initialization) so the dtor's
        // eventManager_ access is still valid.
        static Service::TimeManager instance;
        return instance;
    }
}