#include "services/time_manager.hpp"
#include "services/services.hpp"
namespace Service
{
    void TimeManager::updateRunningAverage(uint32_t new_period)
    {
        float alpha = calculateAlpha(new_period);
        avgFramePeriod_ = avgFramePeriod_ * (1 - alpha) + alpha * new_period;
    }

    void TimeManager::setLongestFramePeriod(uint32_t new_time)
    {
        if (new_time <= longestFramePeriod_)
            return;
        longestFramePeriod_ = new_time;
        auto &evtManager = Engine::Root::get().getEventManager();
        evtManager.pushEvent(LongestFrameTimeUpdatedEventID);
    }

    void TimeManager::processNewFramePeriod(uint32_t new_period)
    {
        updateRunningAverage(new_period);

        setLongestFramePeriod(new_period);

        lastFramePeriod_ = new_period;

        auto &evtManager = Engine::Root::get().getEventManager();
        evtManager.pushEvent(AverageFrameTimeUpdatedEventID);
        evtManager.pushEvent(LastFrameTimeUpdatedEventID);
    }

    void TimeFrameStartListener::eventCall(void *payload)
    {

        timestamp_start_ = std::chrono::high_resolution_clock::now();
    }

    void TimeManager::eventCall(void *payload)
    {
        auto raw_ticks = static_cast<uint32_t>((std::chrono::high_resolution_clock::now() - frameStartListener_.timestamp_start_).count());
        processNewFramePeriod(raw_ticks);
    }

    TimeManager::TimeManager() : frameStartListener_()
    {

        auto &evtManager = Engine::Root::get().getEventManager();
        auto &bus = evtManager.getEventBus();
        bus.subscribe(PapoEvent::PapoEventFrameStartID, frameStartListener_);
        bus.subscribe(PapoEvent::PapoEventFrameEndID, *this);
    }

}