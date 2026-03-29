#pragma once
#include "../services/logger.hpp"
namespace Util
{
    void handleException(Services::Logger *logger, const std::exception &e);
    void installTerminateHandler(Services::Logger *logger);
}