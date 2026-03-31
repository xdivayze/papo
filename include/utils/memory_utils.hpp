#include "exception.hpp"
#include <cstdint>

namespace Util
{
    constexpr size_t align_up(size_t size, size_t alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    inline uintptr_t AlignAddr(uintptr_t addr, size_t align)
    {
        const size_t mask = align - 1;
        if ((align & mask) != 0)
        {
            throw Util::PapoException("Stack Allocater", "alignment must be a power of 2");
        }

        return (addr + mask) & ~mask;
    }

    template <typename T>
    inline T *AlignPointer(T *ptr, size_t align)
    {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t addrAligned = AlignAddr(addr, align);
        return reinterpret_cast<T *>(addrAligned);
    }

}