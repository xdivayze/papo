#pragma once
#include "../utils/singleton.hpp"
#include "logger.hpp"

namespace Engine
{
    class Root : public Singleton<Root>
    {
    public:
        Services::Logger* logger;

        friend class Singleton<Root>;

    private:
        Root();
        ~Root();
    };
}