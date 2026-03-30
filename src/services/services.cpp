#include "services/services.hpp"
#include "services/memory/memory_manager.hpp"
namespace Engine
{
    Root::Root()
    {
        logger_ = new Services::Logger();
        memoryManager_ = new Services::MemoryManager(nullptr);
    }

    Root::~Root()
    {
        delete logger_;
        delete memoryManager_;
    }
}