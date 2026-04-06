#include "utils/exception_handler.hpp"
#include "utils/exception.hpp"

constexpr const char *TAG = "EXCEPTION HANDLER";

static Service::Logger *s_logger = nullptr;

static void terminateHandler()
{
    if (auto exc = std::current_exception())
    {
        try
        {
            std::rethrow_exception(exc);
        }
        catch (const std::exception &e)
        {
            if (s_logger)
                Util::handleException(s_logger, e);
        }
        catch (...)
        {
            if (s_logger)
                s_logger->error(TAG, "unknown exception");
        }
    }
    std::abort();
}

namespace Util
{
    void handleException(Service::Logger *logger, const std::exception &e)
    {
        if (const auto *papo = dynamic_cast<const Util::PapoException *>(&e))
        {
            logger->error(std::string(papo->tag_), std::string(papo->msg_));
        }
        else
        {
            logger->error(TAG, e.what());
        }
    }

    void installTerminateHandler(Service::Logger *logger)
    {
        s_logger = logger;
        std::set_terminate(terminateHandler);
    }
}