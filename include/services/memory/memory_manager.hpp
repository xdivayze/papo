#pragma once
#include "../../util/singleton.hpp"
#include <cstdint>
#include "stack_allocater.hpp"
namespace Services
{
    class MemoryManager : public Singleton<MemoryManager>
    {
    public:
        StackAllocater *stackAllocater;

        friend class Root;

    private:
        MemoryManager();
        ~MemoryManager();
    };
}