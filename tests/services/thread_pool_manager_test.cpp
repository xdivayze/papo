#include <gtest/gtest.h>
#include "services/async/thread_pool_manager.hpp"
#include "utils/exception.hpp"
#include <atomic>
#include <barrier>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace async
{

// All private-member access goes through methods defined here (the friend class).
// gtest-generated subclasses inherit these helpers but cannot directly touch privates.
class ThreadPoolManagerTest : public ::testing::Test
{
protected:
    ThreadPoolManager* pool_ = nullptr;

    void SetUp() override
    {
        pool_ = new ThreadPoolManager(4);
    }

    void TearDown() override
    {
        if (pool_) {
            if (!pool_->stop_)
                pool_->stopSync(false, false);
            delete pool_;
            pool_ = nullptr;
        }
    }

    // --- factory / lifecycle helpers (defined in the friend so they can call privates) ---

    static ThreadPoolManager* makePool(size_t n)
    {
        return new ThreadPoolManager(n);
    }

    static void destroyPool(ThreadPoolManager* p, bool discard = false)
    {
        if (!p) return;
        if (!p->stop_)
            p->stopSync(discard, false);
        delete p;
    }

    static void callStopSync(ThreadPoolManager* p, bool discard)
    {
        p->stopSync(discard, false);
    }

    static void callPauseSync(ThreadPoolManager* p)
    {
        p->pauseSync();
    }

    static void callResume(ThreadPoolManager* p)
    {
        p->resume();
    }
};

// ---------------------------------------------------------------------------
// isAcceptingNewJobs
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, AcceptsJobsAfterConstruction)
{
    EXPECT_TRUE(pool_->isAcceptingNewJobs());
}

// ---------------------------------------------------------------------------
// enqueueTask — basic execution and return values
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, EnqueuedVoidTaskRuns)
{
    std::atomic<bool> ran{false};
    auto fut = pool_->enqueueTask(Task<std::function<void()>>{[&ran] { ran = true; }, REGULAR});
    fut.get();
    EXPECT_TRUE(ran.load());
}

TEST_F(ThreadPoolManagerTest, EnqueuedTaskReturnsValue)
{
    auto fut = pool_->enqueueTask(Task<std::function<int()>>{[] { return 42; }, REGULAR});
    EXPECT_EQ(fut.get(), 42);
}

TEST_F(ThreadPoolManagerTest, MultipleTasksAllExecute)
{
    constexpr int N = 16;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futs;
    futs.reserve(N);
    for (int i = 0; i < N; ++i)
        futs.push_back(pool_->enqueueTask(Task<std::function<void()>>{[&counter] { counter++; }, REGULAR}));
    for (auto& f : futs)
        f.get();
    EXPECT_EQ(counter.load(), N);
}

// ---------------------------------------------------------------------------
// busy()
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, NotBusyWhenIdle)
{
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(pool_->busy());
}

TEST_F(ThreadPoolManagerTest, BusyWhileTasksQueued)
{
    // Pin all 4 threads, then enqueue a 5th — it should sit in the queue.
    std::barrier sync{5};
    std::promise<void> release;
    auto gate = release.get_future().share();

    std::vector<std::future<void>> blockers;
    for (int i = 0; i < 4; ++i)
        blockers.push_back(pool_->enqueueTask(Task<std::function<void()>>{
            [&sync, gate] { sync.arrive_and_wait(); gate.wait(); }, REGULAR}));

    sync.arrive_and_wait(); // all 4 threads occupied

    auto extra = pool_->enqueueTask(Task<std::function<void()>>{[] {}, REGULAR});
    EXPECT_TRUE(pool_->busy());

    release.set_value();
    for (auto& f : blockers)
        f.get();
    extra.get();
}

// ---------------------------------------------------------------------------
// numThreadsWorking()
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, NoThreadsWorkingWhenIdle)
{
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(pool_->numThreadsWorking(), 0u);
}

TEST_F(ThreadPoolManagerTest, AllThreadsShowAsWorking)
{
    std::barrier sync{5};
    std::promise<void> release;
    auto gate = release.get_future().share();

    std::vector<std::future<void>> futs;
    for (int i = 0; i < 4; ++i)
        futs.push_back(pool_->enqueueTask(Task<std::function<void()>>{
            [&sync, gate] { sync.arrive_and_wait(); gate.wait(); }, REGULAR}));

    sync.arrive_and_wait(); // all 4 threads busy
    EXPECT_EQ(pool_->numThreadsWorking(), 4u);

    release.set_value();
    for (auto& f : futs)
        f.get();
}

// ---------------------------------------------------------------------------
// Priority ordering
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, HighPriorityTaskRunsBeforeLow)
{
    // Single-thread pool so we can control dispatch order deterministically.
    ThreadPoolManager* ordered = makePool(1);

    std::barrier sync{2};
    std::promise<void> release;
    auto gate = release.get_future().share();

    auto blocker = ordered->enqueueTask(Task<std::function<void()>>{
        [&sync, gate] { sync.arrive_and_wait(); gate.wait(); }, REGULAR});

    sync.arrive_and_wait(); // thread occupied

    std::vector<int> order;
    std::mutex order_mutex;

    auto low  = ordered->enqueueTask(Task<std::function<void()>>{
        [&order, &order_mutex] { std::lock_guard lg{order_mutex}; order.push_back(0); }, LOW});
    auto high = ordered->enqueueTask(Task<std::function<void()>>{
        [&order, &order_mutex] { std::lock_guard lg{order_mutex}; order.push_back(1); }, HIGH});

    release.set_value();
    blocker.get();
    low.get();
    high.get();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1) << "HIGH should run before LOW";
    EXPECT_EQ(order[1], 0);

    destroyPool(ordered);
}

// ---------------------------------------------------------------------------
// pauseSync / resume
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, PausedPoolDoesNotExecuteNewTasks)
{
    callPauseSync(pool_);

    std::atomic<bool> ran{false};
    auto fut = pool_->enqueueTask(Task<std::function<void()>>{[&ran] { ran = true; }, REGULAR});

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(ran.load()) << "task must not run while pool is paused";

    callResume(pool_);
    fut.get();
    EXPECT_TRUE(ran.load());
}

TEST_F(ThreadPoolManagerTest, ResumeAfterPauseExecutesQueuedTasks)
{
    callPauseSync(pool_);

    constexpr int N = 8;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futs;
    for (int i = 0; i < N; ++i)
        futs.push_back(pool_->enqueueTask(Task<std::function<void()>>{[&counter] { counter++; }, REGULAR}));

    callResume(pool_);
    for (auto& f : futs)
        f.get();
    EXPECT_EQ(counter.load(), N);
}

// ---------------------------------------------------------------------------
// stopSync
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, StopSyncWithDiscardEmptiesQueue)
{
    // Pause the pool so tasks accumulate without executing, then stop with discard.
    ThreadPoolManager* p = makePool(1);
    callPauseSync(p);

    std::atomic<int> ran{0};
    for (int i = 0; i < 4; ++i)
        p->enqueueTask(Task<std::function<void()>>{[&ran] { ran++; }, LOW});

    ASSERT_TRUE(p->busy());
    destroyPool(p, true); // stop + discard + delete

    EXPECT_EQ(ran.load(), 0) << "queued tasks should have been discarded";
}

TEST_F(ThreadPoolManagerTest, StopSyncWithoutDiscardDrainsQueue)
{
    ThreadPoolManager* p = makePool(4);

    std::atomic<int> counter{0};
    constexpr int N = 8;
    std::vector<std::future<void>> futs;
    for (int i = 0; i < N; ++i)
        futs.push_back(p->enqueueTask(Task<std::function<void()>>{[&counter] { counter++; }, REGULAR}));

    destroyPool(p, false); // stop (drain) + delete

    for (auto& f : futs)
        f.get();
    EXPECT_EQ(counter.load(), N);
}

TEST_F(ThreadPoolManagerTest, NotAcceptingJobsAfterStop)
{
    callStopSync(pool_, false);
    EXPECT_FALSE(pool_->isAcceptingNewJobs());
}

TEST_F(ThreadPoolManagerTest, EnqueueThrowsAfterStop)
{
    callStopSync(pool_, false);
    EXPECT_THROW(
        pool_->enqueueTask(Task<std::function<void()>>{[] {}, REGULAR}),
        Util::PapoException);
}

// ---------------------------------------------------------------------------
// clearQueue
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, ClearQueueEmptiesQueuedTasks)
{
    // Pause so tasks accumulate without executing.
    callPauseSync(pool_);

    std::atomic<int> ran{0};
    for (int i = 0; i < 8; ++i)
        pool_->enqueueTask(Task<std::function<void()>>{[&ran] { ran++; }, LOW});

    ASSERT_TRUE(pool_->busy());
    pool_->clearQueue();
    EXPECT_FALSE(pool_->busy());

    callResume(pool_);
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(ran.load(), 0) << "cleared tasks must not execute after resume";
}

TEST_F(ThreadPoolManagerTest, ClearQueueStillAcceptsAndRunsNewTasks)
{
    callPauseSync(pool_);
    for (int i = 0; i < 4; ++i)
        pool_->enqueueTask(Task<std::function<void()>>{[] {}, LOW});

    pool_->clearQueue();
    callResume(pool_);

    // Pool must still be functional after a clear.
    EXPECT_TRUE(pool_->isAcceptingNewJobs());
    std::atomic<bool> ran{false};
    auto fut = pool_->enqueueTask(Task<std::function<void()>>{[&ran] { ran = true; }, REGULAR});
    fut.get();
    EXPECT_TRUE(ran.load());
}

TEST_F(ThreadPoolManagerTest, ClearQueueDoesNotInterruptInFlightTasks)
{
    // Pin all 4 threads with long-running tasks, then clear the queue of pending ones.
    std::barrier sync{5};
    std::promise<void> release;
    auto gate = release.get_future().share();

    std::vector<std::future<void>> running;
    for (int i = 0; i < 4; ++i)
        running.push_back(pool_->enqueueTask(Task<std::function<void()>>{
            [&sync, gate] { sync.arrive_and_wait(); gate.wait(); }, REGULAR}));

    sync.arrive_and_wait(); // all 4 threads are now in-flight

    std::atomic<int> pending{0};
    for (int i = 0; i < 4; ++i)
        pool_->enqueueTask(Task<std::function<void()>>{[&pending] { pending++; }, LOW});

    pool_->clearQueue();

    release.set_value();
    for (auto& f : running)
        f.get(); // in-flight tasks must complete normally

    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(pending.load(), 0) << "pending tasks should have been cleared";
    EXPECT_EQ(pool_->numThreadsWorking(), 0u);
}

// ---------------------------------------------------------------------------
// Destructor safety
// ---------------------------------------------------------------------------

TEST_F(ThreadPoolManagerTest, DestructorDoesNotHangWithInFlightTasks)
{
    ThreadPoolManager* p = makePool(2);
    for (int i = 0; i < 4; ++i)
        p->enqueueTask(Task<std::function<void()>>{
            [] { std::this_thread::sleep_for(5ms); }, REGULAR});
    // destroyPool calls stopSync then delete — must join cleanly
    destroyPool(p);
    SUCCEED();
}

} // namespace async
