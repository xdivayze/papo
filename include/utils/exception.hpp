#pragma once
#include <stdexcept>

namespace Util
{
    class PapoException : public std::runtime_error
    {
    public:
        std::string tag_;
        std::string msg_;
        PapoException(std::string_view TAG, std::string_view msg) : std::runtime_error(std::string(TAG) + ": " + std::string(msg)),
                                                                    tag_(TAG),
                                                                    msg_(msg)
        {
        }
    };

    class MemoryException : public PapoException
    {
    public:
        MemoryException(std::string_view TAG, std::string_view msg) : PapoException(TAG, msg)
        {
        }
    };

    class LogicException : public PapoException
    {
    public:
        LogicException(std::string_view TAG, std::string_view msg) : PapoException(TAG, msg) {}
    };
}