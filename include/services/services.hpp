#pragma once
#include "../util/singleton.hpp"
#include "logger.hpp"

namespace Engine
{
    class Root : public Singleton<Root>
    {
    public:
        Logger logger;

        friend class Singleton<Root>;

    private:
        Root() = default;
        ~Root() = default;
    };
}