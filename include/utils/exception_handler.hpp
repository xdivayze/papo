#pragma once
#include "../services/logger.hpp"
namespace Util
{
    void handleException(Service::Logger *logger, const std::exception &e);
    void installTerminateHandler(Service::Logger *logger);
}