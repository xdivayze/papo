#pragma once
#include "../utils/singleton.hpp"
#include "logger.hpp"
#include "memory/memory_manager.hpp"

namespace Engine
{
    class Root : public Singleton<Root>
    {
    public:
        Services::Logger *logger_;
        Services::MemoryManager *memoryManager_;

        friend class Singleton<Root>;

    private:
        Root();
        ~Root();
    };
}