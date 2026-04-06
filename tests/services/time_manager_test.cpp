#include <gtest/gtest.h>
#include "services/time_manager.hpp"
#include "services/services.hpp"
#include <cmath>

using namespace Service;

// ============================================================
// Static conversion tests — no instance needed
// ============================================================

TEST(TimeManagerConversions, CyclesToSeconds_ZeroIsZero)
{
    EXPECT_FLOAT_EQ(TimeManager::CyclesToSeconds(0), 0.0f);
}

TEST(TimeManagerConversions, CyclesToSeconds_OneCycle_IsOnePeriod)
{
    EXPECT_FLOAT_EQ(TimeManager::CyclesToSeconds(1), TimeManager::cycle_period_);
}

TEST(TimeManagerConversions, CyclesToSeconds_OneFrequency_IsOneSecond)
{
    EXPECT_NEAR(TimeManager::CyclesToSeconds(TimeManager::cycle_frequency_), 1.0f, 1e-5f);
}

TEST(TimeManagerConversions, CyclesToPapoTicks_ZeroIsZero)
{
    EXPECT_FLOAT_EQ(TimeManager::CyclesToPapoTicks(0), 0.0f);
}

TEST(TimeManagerConversions, CyclesToPapoTicks_OneFrequency_Is300Ticks)
{
    EXPECT_NEAR(TimeManager::CyclesToPapoTicks(TimeManager::cycle_frequency_), 300.0f, 1e-3f);
}

TEST(TimeManagerConversions, SecondsToPapoTicks_ZeroIsZero)
{
    EXPECT_FLOAT_EQ(TimeManager::SecondsToPapoTicks(0.0f), 0.0f);
}

TEST(TimeManagerConversions, SecondsToPapoTicks_OneSecond_Is300Ticks)
{
    EXPECT_FLOAT_EQ(TimeManager::SecondsToPapoTicks(1.0f), 300.0f);
}

TEST(TimeManagerConversions, SecondsToPapoTicks_HalfSecond_Is150Ticks)
{
    EXPECT_FLOAT_EQ(TimeManager::SecondsToPapoTicks(0.5f), 150.0f);
}

TEST(TimeManagerConversions, SecondsToCycles_ZeroIsZero)
{
    EXPECT_EQ(TimeManager::SecondsToCycles(0.0f), 0u);
}

TEST(TimeManagerConversions, SecondsToCycles_OneSecond_IsCycleFrequency)
{
    EXPECT_EQ(TimeManager::SecondsToCycles(1.0f), TimeManager::cycle_frequency_);
}

TEST(TimeManagerConversions, PapoTicksToSeconds_ZeroIsZero)
{
    EXPECT_FLOAT_EQ(TimeManager::PapoTicksToSeconds(0.0f), 0.0f);
}

TEST(TimeManagerConversions, PapoTicksToSeconds_300Ticks_IsOneSecond)
{
    EXPECT_FLOAT_EQ(TimeManager::PapoTicksToSeconds(300.0f), 1.0f);
}

TEST(TimeManagerConversions, PapoTicksToSeconds_150Ticks_IsHalfSecond)
{
    EXPECT_FLOAT_EQ(TimeManager::PapoTicksToSeconds(150.0f), 0.5f);
}

TEST(TimeManagerConversions, PapoTicksToCycles_ZeroIsZero)
{
    EXPECT_EQ(TimeManager::PapoTicksToCycles(0.0f), 0u);
}

TEST(TimeManagerConversions, PapoTicksToCycles_300Ticks_IsCycleFrequency)
{
    EXPECT_EQ(TimeManager::PapoTicksToCycles(300.0f), TimeManager::cycle_frequency_);
}

TEST(TimeManagerConversions, RoundTrip_SecondsToTicksToSeconds)
{
    float secs = 2.5f;
    EXPECT_FLOAT_EQ(TimeManager::PapoTicksToSeconds(TimeManager::SecondsToPapoTicks(secs)), secs);
}

TEST(TimeManagerConversions, RoundTrip_SecondsToCyclesToSeconds)
{
    EXPECT_NEAR(TimeManager::CyclesToSeconds(TimeManager::SecondsToCycles(1.0f)), 1.0f, 1e-5f);
}

TEST(TimeManagerConversions, RoundTrip_TicksToCyclesToTicks)
{
    float ticks = 600.0f;
    EXPECT_NEAR(TimeManager::CyclesToPapoTicks(TimeManager::PapoTicksToCycles(ticks)), ticks, 1e-2f);
}

// ============================================================
// UpdateTimeStruct tests
// ============================================================

TEST(TimeManagerUpdateTimeStruct, PAPO_TICK_source_populates_secs_and_cycles)
{
    TimeManager::time_unit_t t{};
    t.type  = TimeManager::PAPO_TICK;
    t.ticks = 300.0f;
    TimeManager::UpdateTimeStruct(t);
    EXPECT_NEAR(t.secs, 1.0f, 1e-5f);
    EXPECT_EQ(t.cycles, TimeManager::cycle_frequency_);
}

TEST(TimeManagerUpdateTimeStruct, SECOND_source_populates_ticks_and_cycles)
{
    TimeManager::time_unit_t t{};
    t.type = TimeManager::SECOND;
    t.secs = 1.0f;
    TimeManager::UpdateTimeStruct(t);
    EXPECT_FLOAT_EQ(t.ticks, 300.0f);
    EXPECT_EQ(t.cycles, TimeManager::cycle_frequency_);
}

TEST(TimeManagerUpdateTimeStruct, CYCLE_source_populates_secs_and_ticks)
{
    TimeManager::time_unit_t t{};
    t.type   = TimeManager::CYCLE;
    t.cycles = TimeManager::cycle_frequency_;
    TimeManager::UpdateTimeStruct(t);
    EXPECT_NEAR(t.secs, 1.0f, 1e-5f);
    EXPECT_NEAR(t.ticks, 300.0f, 1e-3f);
}

TEST(TimeManagerUpdateTimeStruct, PAPO_TICK_zero_leaves_secs_and_cycles_zero)
{
    TimeManager::time_unit_t t{};
    t.type  = TimeManager::PAPO_TICK;
    t.ticks = 0.0f;
    TimeManager::UpdateTimeStruct(t);
    EXPECT_FLOAT_EQ(t.secs, 0.0f);
    EXPECT_EQ(t.cycles, 0u);
}

TEST(TimeManagerUpdateTimeStruct, SECOND_zero_leaves_ticks_and_cycles_zero)
{
    TimeManager::time_unit_t t{};
    t.type = TimeManager::SECOND;
    t.secs = 0.0f;
    TimeManager::UpdateTimeStruct(t);
    EXPECT_FLOAT_EQ(t.ticks, 0.0f);
    EXPECT_EQ(t.cycles, 0u);
}

TEST(TimeManagerUpdateTimeStruct, CYCLE_zero_leaves_secs_and_ticks_zero)
{
    TimeManager::time_unit_t t{};
    t.type   = TimeManager::CYCLE;
    t.cycles = 0;
    TimeManager::UpdateTimeStruct(t);
    EXPECT_FLOAT_EQ(t.secs, 0.0f);
    EXPECT_FLOAT_EQ(t.ticks, 0.0f);
}

// ============================================================
// TimeManagerTest fixture — accesses private API via friendship.
//
// Friendship is granted to this class (not to individual TEST_F
// functions). TEST_F generates subclasses of this fixture, so
// private TimeManager members are only reachable through the
// protected proxy methods and setters defined here.
// ============================================================

class TimeManagerTest : public ::testing::Test
{
protected:
    TimeManager tm_;

    TimeManagerTest() : tm_() {}

    void SetUp() override
    {
        tm_.avgFramePeriod_     = 0;
        tm_.lastFramePeriod_    = 0;
        tm_.longestFramePeriod_ = 0;
    }

    // Field readers
    uint32_t avgFramePeriod()     const { return tm_.avgFramePeriod_; }
    uint32_t lastFramePeriod()    const { return tm_.lastFramePeriod_; }
    uint32_t longestFramePeriod() const { return tm_.longestFramePeriod_; }
    float    emaTimeConstant()    const { return tm_.EMATimeConstant_; }

    // Field writers
    void setAvgFramePeriod(uint32_t v)         { tm_.avgFramePeriod_     = v; }
    void setLastFramePeriod(uint32_t v)        { tm_.lastFramePeriod_    = v; }
    void setLongestFramePeriodDirect(uint32_t v){ tm_.longestFramePeriod_ = v; }

    // Private method proxies
    float calculateAlpha(uint32_t dt)      { return tm_.calculateAlpha(dt); }
    void  updateRunningAverage(uint32_t p) { tm_.updateRunningAverage(p); }
    void  processNewFramePeriod(uint32_t p){ tm_.processNewFramePeriod(p); }
};

// --- calculateAlpha ---

TEST_F(TimeManagerTest, CalculateAlpha_ZeroDt_ReturnsZero)
{
    EXPECT_FLOAT_EQ(calculateAlpha(0), 0.0f);
}

TEST_F(TimeManagerTest, CalculateAlpha_DtEqualsTimeConstant_IsOneMinusInvE)
{
    uint32_t tau      = static_cast<uint32_t>(emaTimeConstant());
    float    expected = 1.0f - std::exp(-1.0f);
    EXPECT_NEAR(calculateAlpha(tau), expected, 1e-5f);
}

TEST_F(TimeManagerTest, CalculateAlpha_VeryLargeDt_ApproachesOne)
{
    // Use a small time constant so 100× tau stays within uint32_t range.
    tm_.setEMATimeConstant(TimeManager::SecondsToCycles(0.001f));
    uint32_t huge = static_cast<uint32_t>(emaTimeConstant()) * 100;
    EXPECT_NEAR(calculateAlpha(huge), 1.0f, 1e-5f);
}

TEST_F(TimeManagerTest, CalculateAlpha_IncreasesMonotonically)
{
    uint32_t tau = static_cast<uint32_t>(emaTimeConstant());
    EXPECT_LT(calculateAlpha(tau / 4), calculateAlpha(tau / 2));
    EXPECT_LT(calculateAlpha(tau / 2), calculateAlpha(tau));
    EXPECT_LT(calculateAlpha(tau),     calculateAlpha(tau * 2));
}

TEST_F(TimeManagerTest, CalculateAlpha_IsInRange_ZeroToOne)
{
    uint32_t tau = static_cast<uint32_t>(emaTimeConstant());
    for (uint32_t dt : {0u, tau / 2, tau, tau * 2, tau * 10})
    {
        float a = calculateAlpha(dt);
        EXPECT_GE(a, 0.0f);
        EXPECT_LE(a, 1.0f);
    }
}

// --- updateRunningAverage ---

TEST_F(TimeManagerTest, UpdateRunningAverage_FromZero_ScalesByAlpha)
{
    uint32_t target = TimeManager::SecondsToCycles(0.016f);
    setAvgFramePeriod(0);
    updateRunningAverage(target);
    float expected = calculateAlpha(target) * static_cast<float>(target);
    EXPECT_NEAR(static_cast<float>(avgFramePeriod()), expected, 2.0f);
}

TEST_F(TimeManagerTest, UpdateRunningAverage_Converges_ToTarget)
{
    uint32_t target = TimeManager::SecondsToCycles(0.016f);
    setAvgFramePeriod(0);
    for (int i = 0; i < 1000; ++i)
        updateRunningAverage(target);
    EXPECT_NEAR(static_cast<float>(avgFramePeriod()),
                static_cast<float>(target),
                target * 0.01f);
}

TEST_F(TimeManagerTest, UpdateRunningAverage_SmallerTimeConstant_FasterConvergence)
{
    uint32_t target = TimeManager::SecondsToCycles(0.016f);
    constexpr int iterations = 10;

    setAvgFramePeriod(0);
    tm_.setEMATimeConstant(TimeManager::SecondsToCycles(0.5f));
    for (int i = 0; i < iterations; ++i)
        updateRunningAverage(target);
    uint32_t fast = avgFramePeriod();

    setAvgFramePeriod(0);
    tm_.setEMATimeConstant(TimeManager::SecondsToCycles(5.0f));
    for (int i = 0; i < iterations; ++i)
        updateRunningAverage(target);
    uint32_t slow = avgFramePeriod();

    EXPECT_GT(fast, slow);
}

// --- processNewFramePeriod ---

TEST_F(TimeManagerTest, ProcessNewFramePeriod_SetsLastFramePeriod)
{
    uint32_t p = TimeManager::SecondsToCycles(0.016f);
    processNewFramePeriod(p);
    EXPECT_EQ(lastFramePeriod(), p);
}

TEST_F(TimeManagerTest, ProcessNewFramePeriod_UpdatesAvgFramePeriod)
{
    uint32_t p = TimeManager::SecondsToCycles(0.016f);
    setAvgFramePeriod(0);
    processNewFramePeriod(p);
    EXPECT_GT(avgFramePeriod(), 0u);
}

TEST_F(TimeManagerTest, ProcessNewFramePeriod_SetsLongestWhenLarger)
{
    uint32_t big = TimeManager::SecondsToCycles(0.050f);
    setLongestFramePeriodDirect(0);
    processNewFramePeriod(big);
    EXPECT_EQ(longestFramePeriod(), big);
}

TEST_F(TimeManagerTest, ProcessNewFramePeriod_DoesNotDecreaseLongest)
{
    uint32_t big   = TimeManager::SecondsToCycles(0.050f);
    uint32_t small = TimeManager::SecondsToCycles(0.010f);
    setLongestFramePeriodDirect(big);
    processNewFramePeriod(small);
    EXPECT_EQ(longestFramePeriod(), big);
}

TEST_F(TimeManagerTest, ProcessNewFramePeriod_MultipleFrames_LongestIsMax)
{
    uint32_t p1 = TimeManager::SecondsToCycles(0.010f);
    uint32_t p2 = TimeManager::SecondsToCycles(0.050f);
    uint32_t p3 = TimeManager::SecondsToCycles(0.020f);
    setLongestFramePeriodDirect(0);
    processNewFramePeriod(p1);
    processNewFramePeriod(p2);
    processNewFramePeriod(p3);
    EXPECT_EQ(longestFramePeriod(), p2);
}

// --- setLongestFramePeriod ---

TEST_F(TimeManagerTest, SetLongestFramePeriod_UpdatesWhenGreater)
{
    setLongestFramePeriodDirect(100u);
    tm_.setLongestFramePeriod(200u);
    EXPECT_EQ(longestFramePeriod(), 200u);
}

TEST_F(TimeManagerTest, SetLongestFramePeriod_IgnoresWhenSmaller)
{
    setLongestFramePeriodDirect(200u);
    tm_.setLongestFramePeriod(100u);
    EXPECT_EQ(longestFramePeriod(), 200u);
}

TEST_F(TimeManagerTest, SetLongestFramePeriod_IgnoresWhenEqual)
{
    setLongestFramePeriodDirect(150u);
    tm_.setLongestFramePeriod(150u);
    EXPECT_EQ(longestFramePeriod(), 150u);
}

// --- setEMATimeConstant ---

TEST_F(TimeManagerTest, SetEMATimeConstant_UpdatesConstant)
{
    float newTau = TimeManager::SecondsToCycles(3.0f);
    tm_.setEMATimeConstant(newTau);
    EXPECT_FLOAT_EQ(emaTimeConstant(), newTau);
}

TEST_F(TimeManagerTest, SetEMATimeConstant_AffectsAlphaCalculation)
{
    uint32_t dt = TimeManager::SecondsToCycles(1.0f);

    tm_.setEMATimeConstant(TimeManager::SecondsToCycles(0.5f));
    float alphaFast = calculateAlpha(dt);

    tm_.setEMATimeConstant(TimeManager::SecondsToCycles(5.0f));
    float alphaSlow = calculateAlpha(dt);

    // smaller time constant → larger alpha for the same dt
    EXPECT_GT(alphaFast, alphaSlow);
}

// --- public getters ---

TEST_F(TimeManagerTest, Getters_AvgFramePeriod_ReturnsConsistentConversions)
{
    uint32_t p = TimeManager::SecondsToCycles(0.016f);
    setAvgFramePeriod(p);

    EXPECT_EQ(tm_.getAvgFramePeriodCycles(), p);
    EXPECT_NEAR(tm_.getAvgFramePeriodSeconds(),
                TimeManager::CyclesToSeconds(p), 1e-7f);
    EXPECT_NEAR(tm_.getAvgFramePeriodPapoTicks(),
                TimeManager::CyclesToPapoTicks(p), 1e-3f);
}

TEST_F(TimeManagerTest, Getters_LongestFramePeriod_ReturnsConsistentConversions)
{
    uint32_t p = TimeManager::SecondsToCycles(0.050f);
    setLongestFramePeriodDirect(p);

    EXPECT_EQ(tm_.getLongestFramePeriodCycles(), p);
    EXPECT_NEAR(tm_.getLongestFramePeriodSeconds(),
                TimeManager::CyclesToSeconds(p), 1e-7f);
    EXPECT_NEAR(tm_.getLongestFramePeriodPapoTicks(),
                TimeManager::CyclesToPapoTicks(p), 1e-3f);
}

TEST_F(TimeManagerTest, Getters_LastFramePeriod_ReturnsConsistentConversions)
{
    uint32_t p = TimeManager::SecondsToCycles(0.033f);
    setLastFramePeriod(p);

    EXPECT_EQ(tm_.getLastFramePeriodCycles(), p);
    EXPECT_NEAR(tm_.getLastFramePeriodSeconds(),
                TimeManager::CyclesToSeconds(p), 1e-7f);
    EXPECT_NEAR(tm_.getLastFramePeriodPapoTicks(),
                TimeManager::CyclesToPapoTicks(p), 1e-3f);
}
