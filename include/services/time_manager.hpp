#pragma once
#include "event/papo_event.hpp"
#include <cstdint>
#include <chrono>
#include <cmath>

// Forward declaration so TimeManager can grant test-fixture friendship.
class TimeManagerTest;

namespace Engine
{
    class Root;
}

namespace Service
{
    class TimeFrameStartListener : public PapoEvent::IPapoEventListener
    {
    public:
        std::chrono::time_point<std::chrono::high_resolution_clock> timestamp_start_;
        // starts the delta timer
        void eventCall(void *payload) override;
    };

    class TimeManager : public PapoEvent::IPapoEventListener
    {
    public:
        friend class Engine::Root;

        constexpr static const PapoEvent::PapoEventTypeID AverageFrameTimeUpdatedEventID = 1;
        constexpr static const PapoEvent::PapoEventTypeID LastFrameTimeUpdatedEventID = 2;
        constexpr static const PapoEvent::PapoEventTypeID LongestFrameTimeUpdatedEventID = 3;

        // frame end listener
        void eventCall(void *payload) override;

        enum TimeType
        {
            PAPO_TICK,
            SECOND,
            CYCLE,
        };
        typedef float time_papo_ticks_t; // 1/300th of a second

        typedef struct
        {
            TimeType type;
            float secs;
            time_papo_ticks_t ticks;
            uint32_t cycles;
        } time_unit_t;

        constexpr static const float cycle_period_ = static_cast<float>(std::chrono::high_resolution_clock::period::num) / std::chrono::high_resolution_clock::period::den;
        constexpr static const uint32_t cycle_frequency_ = 1 / cycle_period_;

        constexpr static inline float CyclesToSeconds(uint32_t time)
        {
            return time * cycle_period_;
        }

        constexpr static inline float CyclesToPapoTicks(uint32_t time)
        {
            return CyclesToSeconds(time) * 300;
        }

        constexpr static inline float SecondsToPapoTicks(float time)
        {
            return time * 300;
        }

        constexpr static inline uint32_t SecondsToCycles(float time)
        {
            return time * cycle_frequency_;
        }

        constexpr static inline float PapoTicksToSeconds(float time)
        {
            return time / 300;
        }

        constexpr static inline uint32_t PapoTicksToCycles(float time)
        {
            return SecondsToCycles(PapoTicksToSeconds(time));
        }

        /*
        this function takes in a time_unit_t struct and modifies the different time unit fields
        from the value of the field inferred from the type field of the struct
       */
        constexpr static inline void UpdateTimeStruct(time_unit_t &time) noexcept
        {
            switch (time.type)
            {
            case PAPO_TICK:
                time.secs = PapoTicksToSeconds(time.ticks);
                time.cycles = PapoTicksToCycles(time.ticks);
                break;
            case CYCLE:
                time.secs = CyclesToSeconds(time.cycles);
                time.ticks = CyclesToPapoTicks(time.cycles);
                break;
            case SECOND:
                time.ticks = SecondsToPapoTicks(time.secs);
                time.cycles = SecondsToCycles(time.secs);
                break;
            default:
                break;
            }
        }

        constexpr inline float getAvgFramePeriodSeconds() const
        {
            return CyclesToSeconds(avgFramePeriod_);
        }
        constexpr inline time_papo_ticks_t getAvgFramePeriodPapoTicks() const
        {
            return CyclesToPapoTicks(avgFramePeriod_);
        }
        constexpr inline uint32_t getAvgFramePeriodCycles() const
        {
            return avgFramePeriod_;
        }

        constexpr inline float getLastFramePeriodSeconds() const
        {
            return CyclesToSeconds(lastFramePeriod_);
        }
        constexpr inline time_papo_ticks_t getLastFramePeriodPapoTicks() const
        {
            return CyclesToPapoTicks(lastFramePeriod_);
        }
        constexpr inline uint32_t getLastFramePeriodCycles() const
        {
            return lastFramePeriod_;
        }

        constexpr inline float getLongestFramePeriodSeconds() const
        {
            return CyclesToSeconds(longestFramePeriod_);
        }
        constexpr inline time_papo_ticks_t getLongestFramePeriodPapoTicks() const
        {
            return CyclesToPapoTicks(longestFramePeriod_);
        }

        constexpr inline uint32_t getLongestFramePeriodCycles() const
        {
            return longestFramePeriod_;
        }

        // updates ema time constant based on the time constant
        constexpr inline void setEMATimeConstant(float time_constant)
        {
            EMATimeConstant_ = time_constant;
        }

        void setLongestFramePeriod(uint32_t new_time);

    private:
        friend class ::TimeManagerTest;

        // listener subscription
        TimeManager();
        ~TimeManager();

        uint32_t avgFramePeriod_; // running average of the frame period in cycles
        uint32_t longestFramePeriod_;
        uint32_t lastFramePeriod_;

        constexpr inline float calculateAlpha(uint32_t dt) const
        {
            return 1.0f - std::expf(-1.0f * static_cast<float>(dt) / EMATimeConstant_);
        }

        void updateRunningAverage(uint32_t new_period);

        void processNewFramePeriod(uint32_t new_period);

        float EMATimeConstant_ = SecondsToCycles(1.5f);

        TimeFrameStartListener frameStartListener_;
    };
}