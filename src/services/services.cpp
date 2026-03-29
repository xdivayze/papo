#include "services/services.hpp"

namespace Engine
{
    Root::Root()
    {
        logger = new Services::Logger();
    }

    Root::~Root()
    {
        delete logger;
    }
}