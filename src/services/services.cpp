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
}